#include "ProfileParser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

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
		
		// Check if it's an enabled unsupported filter
		if (auto* unsupported = dynamic_cast<UnsupportedFilter*>(filter.get()))
		{
			if (unsupported->isEnabled())
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

	// Parse Preamp
	if (cleanLine.startsWith("Preamp:", Qt::CaseInsensitive))
	{
		// Extract gain value
		QRegularExpression re(R"(Preamp:\s*([-+]?\d+\.?\d*)\s*dB)", QRegularExpression::CaseInsensitiveOption);
		auto match = re.match(cleanLine);
		if (!match.hasMatch())
			return std::unexpected("Failed to parse Preamp line: " + line);

		double gain = match.captured(1).toDouble();
		return std::make_unique<PreampFilter>(gain, !isCommented);
	}

	// Parse Filter lines
	if (cleanLine.startsWith("Filter ", Qt::CaseInsensitive))
	{
		// Check if it's ON or OFF
		bool enabled = cleanLine.contains(" ON ", Qt::CaseInsensitive);
		bool disabled = cleanLine.contains(" OFF ", Qt::CaseInsensitive);
		
		if (!enabled && !disabled)
			return std::unexpected("Filter line missing ON/OFF: " + line);

		// Check filter type
		if (cleanLine.contains(" PK ", Qt::CaseInsensitive))
		{
			// Parse peaking filter: Filter: ON PK Fc 160.7 Hz Gain -2 dB Q 3.92
			QRegularExpression re(
				R"(Filter [0-9]*:\s*(ON|OFF)\s+PK\s+Fc\s+([-+]?\d+\.?\d*)\s*Hz\s+Gain\s+([-+]?\d+\.?\d*)\s*dB\s+Q\s+([-+]?\d+\.?\d*))",
				QRegularExpression::CaseInsensitiveOption
			);
			auto match = re.match(cleanLine);
			if (!match.hasMatch())
				return std::unexpected("Failed to parse PK filter line: " + line);

			double fc = match.captured(2).toDouble();
			double gain = match.captured(3).toDouble();
			double q = match.captured(4).toDouble();

			return std::make_unique<PeakingFilter>(fc, gain, q, enabled);
		}
		else
		{
			// Unsupported filter type - preserve as-is
			return std::make_unique<UnsupportedFilter>(cleanLine, enabled);
		}
	}

	// Unknown line format
	return std::unexpected("Unknown line format: " + line);
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
