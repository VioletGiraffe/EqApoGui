#include "Filter.h"

// Shortest representation that survives a save/reload cycle unchanged (no clamping to fixed decimals, no trailing zeros)
static QString formatValue(double value)
{
	return QString::number(value, 'g', 10);
}

QString PreampFilter::toConfigLine() const
{
	return QString("Preamp: %1 dB").arg(formatValue(_gain));
}

QString PreampFilter::displayName() const
{
	return QString("Preamp: %1 dB").arg(_gain, 0, 'f', 1);
}

QString BiquadFilter::typeToken() const
{
	switch (_type)
	{
	case BiquadType::Peaking:   return "PK";
	case BiquadType::LowPass:   return "LP";
	case BiquadType::HighPass:  return "HP";
	case BiquadType::BandPass:  return "BP";
	case BiquadType::Notch:     return "NO";
	case BiquadType::AllPass:   return "AP";
	case BiquadType::LowShelf:  return _shelfUsesCenterFreq ? "LSC" : "LS";
	case BiquadType::HighShelf: return _shelfUsesCenterFreq ? "HSC" : "HS";
	}
	return {};
}

QString BiquadFilter::toConfigLine() const
{
	QString line = "Filter: ON " + typeToken();
	if (_widthKind == WidthKind::SlopeDb)
		line += " " + formatValue(_width) + " dB";
	line += " Fc " + formatValue(_fc) + " Hz";
	if (hasGain())
		line += " Gain " + formatValue(_gain) + " dB";
	if (_widthKind == WidthKind::Q)
		line += " Q " + formatValue(_width);
	else if (_widthKind == WidthKind::BandwidthOct)
		line += " BW Oct " + formatValue(_width);
	return line;
}

QString BiquadFilter::displayName() const
{
	QString name = typeToken() + ": " + formatValue(_fc) + " Hz";
	if (hasGain())
		name += QString(", %1%2 dB").arg(_gain > 0 ? "+" : "", formatValue(_gain));

	switch (_widthKind)
	{
	case WidthKind::Q:            name += ", Q=" + formatValue(_width); break;
	case WidthKind::BandwidthOct: name += ", BW=" + formatValue(_width) + " oct"; break;
	case WidthKind::SlopeDb:      name += ", slope " + formatValue(_width) + " dB"; break;
	case WidthKind::Default:      break;
	}
	return name;
}

QString CommentLine::toConfigLine() const
{
	return _text;
}

QString CommentLine::displayName() const
{
	return "# " + _text;
}

QString UnsupportedFilter::toConfigLine() const
{
	return _originalLine;
}

QString UnsupportedFilter::displayName() const
{
	return (_noOp ? "[Ignored] " : "[Unsupported] ") + _originalLine;
}
