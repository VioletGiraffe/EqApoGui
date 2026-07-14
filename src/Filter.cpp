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

QString PeakingFilter::toConfigLine() const
{
	return QString("Filter: ON PK Fc %1 Hz Gain %2 dB Q %3")
		.arg(formatValue(_fc), formatValue(_gain), formatValue(_q));
}

QString PeakingFilter::displayName() const
{
	return QString("PK: %1 Hz, %2%3 dB, Q=%4")
		.arg(formatValue(_fc), _gain > 0 ? "+" : "", formatValue(_gain), formatValue(_q));
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
	return "[Unsupported] " + _originalLine;
}
