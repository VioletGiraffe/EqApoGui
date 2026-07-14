#include "ProfileParser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

#include <algorithm>
#include <iterator>

// Port of E-APO's BiQuadFilterFactory::getFreq(). Returns -1 on parse failure, same as E-APO.
static double parseFreq(QString s)
{
	s.remove(QChar(0x00A0)); // remove thousands separator for locales utilizing non-breaking space
	bool ok = false;
	double result = s.toDouble(&ok);
	if (!ok)
		return -1.0;

	// "1.234" (dot exactly 3 from the end, no exponent) is interpreted as a thousands separator because of Room EQ Wizard
	if (s.length() >= 5 && !s.contains('e') && !s.contains('E') && s[s.length() - 4] == '.')
		result *= 1000.0;

	return result;
}

std::expected<ProfileData, QString> ProfileParser::parseProfile(const QString& filePath)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return std::unexpected("Failed to open file for reading: " + filePath);

	QTextStream in(&file);
	in.setEncoding(QStringConverter::Utf8);

	ProfileData data;
	QString line;
	bool hasEnabledUnsupportedFilter = false;

	while (in.readLineInto(&line))
	{
		line = line.trimmed();
		if (line.isEmpty())
			continue;

		auto filterResult = parseLine(line);
		if (!filterResult.has_value())
			return std::unexpected(filterResult.error());

		auto& filter = filterResult.value();
		
		// Enabled unsupported filters block editing (no-op lines like "Filter: ON None" don't)
		if (auto* unsupported = dynamic_cast<UnsupportedFilter*>(filter.get()))
		{
			if (unsupported->isEnabled() && !unsupported->isNoOp())
				hasEnabledUnsupportedFilter = true;
		}

		data.filters.push_back(std::move(filter));
	}

	if (hasEnabledUnsupportedFilter)
		return std::unexpected("Profile contains enabled unsupported filters. Please disable them in EQ APO GUI first.");

	return data;
}

std::expected<FilterUniquePtr, QString> ProfileParser::parseLine(const QString& line)
{
	QString trimmedLine = line.trimmed();
	bool isCommented = trimmedLine.startsWith("#");
	QString cleanLine = isCommented ? trimmedLine.mid(1).trimmed() : trimmedLine;

	// A commented line that doesn't parse as a known command is a plain comment; an active one is an error
	const auto errorOrComment = [&](QString&& message) -> std::expected<FilterUniquePtr, QString> {
		if (isCommented)
			return std::make_unique<CommentLine>(cleanLine);
		return std::unexpected(std::move(message));
	};

	if (const auto preamp = parsePreampLine(cleanLine); preamp.isPreampLine)
	{
		if (!preamp.gain.has_value())
			return errorOrComment("Failed to parse Preamp line: " + line);

		return std::make_unique<PreampFilter>(*preamp.gain, !isCommented);
	}

	// Filter lines: both "Filter:" and the numbered "Filter 1:" form.
	// Matching mirrors E-APO's BiQuadFilterFactory.cpp (case-sensitive keywords and type tokens),
	// with one extension: OFF (which E-APO simply ignores) and a comment prefix both mean "disabled".
	if (cleanLine.startsWith("Filter"))
	{
		static const QRegularExpression reHeader(R"(^Filter\s*[0-9]*\s*:\s*(ON|OFF)\s+([A-Za-z]+))");
		const auto header = reHeader.match(cleanLine);
		if (!header.hasMatch())
			return errorOrComment("Malformed filter line: " + line);

		const bool enabled = !isCommented && header.captured(1) == "ON";
		const QString token = header.captured(2);

		if (token == "None") // placeholder lines written by some tools; E-APO creates no filter for them
			return std::make_unique<UnsupportedFilter>(cleanLine, enabled, /*noOp=*/true);

		struct TokenMapping { const char* token; BiquadType type; bool centerFreq; };
		static constexpr TokenMapping tokenMap[] = {
			{ "PK", BiquadType::Peaking, false }, { "PEQ", BiquadType::Peaking, false }, { "Modal", BiquadType::Peaking, false },
			{ "LP", BiquadType::LowPass, false }, { "LPQ", BiquadType::LowPass, false },
			{ "HP", BiquadType::HighPass, false }, { "HPQ", BiquadType::HighPass, false },
			{ "BP", BiquadType::BandPass, false },
			{ "NO", BiquadType::Notch, false },
			{ "AP", BiquadType::AllPass, false },
			{ "LS", BiquadType::LowShelf, false }, { "LSC", BiquadType::LowShelf, true },
			{ "HS", BiquadType::HighShelf, false }, { "HSC", BiquadType::HighShelf, true },
		};
		const auto* mapping = std::find_if(std::begin(tokenMap), std::end(tokenMap),
			[&token](const TokenMapping& m) { return token == m.token; });
		if (mapping == std::end(tokenMap))
			return std::make_unique<UnsupportedFilter>(cleanLine, enabled); // unknown filter type - preserved; blocks editing while enabled

		const BiquadType type = mapping->type;
		const bool isShelf = type == BiquadType::LowShelf || type == BiquadType::HighShelf;

		QString params = cleanLine.mid(header.capturedEnd(2));
		params.replace(',', '.'); // E-APO accepts comma as decimal mark

		static const QRegularExpression reFc(R"(\s+Fc\s*([-+0-9.eE\x{00A0}]+)\s*H\s*z)"); // yes, E-APO really allows "H z"
		static const QRegularExpression reGain(R"(\s+Gain\s*([-+0-9.eE]+)\s*dB)");
		static const QRegularExpression reQ(R"(\s+Q\s*([-+0-9.eE]+))");
		static const QRegularExpression reBw(R"(\s+BW\s+Oct\s*([-+0-9.eE]+))");
		static const QRegularExpression reSlope(R"(^\s*([-+0-9.eE]+)\s*dB)"); // shelf slope immediately follows the type token

		const auto fcMatch = reFc.match(params);
		if (!fcMatch.hasMatch())
			return errorOrComment("No Fc in filter line: " + line);
		const double fc = parseFreq(fcMatch.captured(1));

		double gain = 0.0;
		const bool needsGain = type == BiquadType::Peaking || isShelf;
		if (const auto gainMatch = reGain.match(params); gainMatch.hasMatch())
			gain = gainMatch.captured(1).toDouble(); // stored but unused for types where E-APO ignores gain
		else if (needsGain)
			return errorOrComment("No gain in filter line: " + line);

		// Width: Q, or bandwidth in octaves (except shelves), or slope in dB (shelves only) - later ones take precedence, as in E-APO
		WidthKind widthKind = WidthKind::Default;
		double width = 0.0;
		if (const auto qMatch = reQ.match(params); qMatch.hasMatch())
		{
			widthKind = WidthKind::Q;
			width = qMatch.captured(1).toDouble();
		}
		if (const auto bwMatch = reBw.match(params); bwMatch.hasMatch() && !isShelf)
		{
			widthKind = WidthKind::BandwidthOct;
			width = bwMatch.captured(1).toDouble();
		}
		if (const auto slopeMatch = reSlope.match(params); slopeMatch.hasMatch() && isShelf)
		{
			widthKind = WidthKind::SlopeDb;
			width = slopeMatch.captured(1).toDouble();
		}
		if (width == 0.0) // an explicit zero behaves as "not specified" in E-APO
			widthKind = WidthKind::Default;

		if (widthKind == WidthKind::Default && (type == BiquadType::Peaking || type == BiquadType::AllPass))
			return errorOrComment("No Q or bandwidth in filter line: " + line);

		return std::make_unique<BiquadFilter>(type, mapping->centerFreq, fc, gain, widthKind, width, enabled);
	}

	if (isCommented)
		return std::make_unique<CommentLine>(cleanLine);

	// Unrecognized active command (GraphicEQ:, Convolution:, Device:, ...) - it would affect processing
	// without being represented here, so treat as enabled unsupported and let parseProfile refuse the profile
	return std::make_unique<UnsupportedFilter>(cleanLine, true);
}

ProfileParser::PreampLine ProfileParser::parsePreampLine(const QString& cleanLine)
{
	// E-APO matches the trimmed key "Preamp" exactly (case-sensitively, spaces before ':' allowed) and scans
	// the value as "<number> dB" where the "dB" suffix is not actually verified, so it is optional here too
	static const QRegularExpression rePreamp(R"(^Preamp\s*:\s*([-+0-9.eE,]+))");
	const auto match = rePreamp.match(cleanLine);
	if (!match.hasMatch())
		return {};

	QString gainString = match.captured(1);
	gainString.replace(',', '.'); // E-APO accepts comma as decimal mark

	bool ok = false;
	const double gain = gainString.toDouble(&ok);
	return { .isPreampLine = true, .gain = ok ? std::optional(gain) : std::nullopt };
}

std::expected<void, QString> ProfileParser::saveProfile(const QString& filePath, const std::vector<FilterUniquePtr>& filters)
{
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return std::unexpected("Failed to open file for writing: " + filePath);

	QTextStream out(&file);
	out.setEncoding(QStringConverter::Utf8);

	for (const auto& filter : filters)
	{
		QString line = filter->toConfigLine();
		
		// Add comment prefix if filter is disabled
		if (!filter->isEnabled())
			line = "# " + line;

		out << line << "\r\n";
	}

	file.close();
	if (file.error() != QFile::NoError)
		return std::unexpected("Error writing to file: " + file.errorString());

	return {};
}
