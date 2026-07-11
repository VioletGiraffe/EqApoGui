#include "Filter.h"

QString PreampFilter::toConfigLine() const
{
	return QString("Preamp: %1 dB").arg(_gain, 0, 'f', 1);
}

QString PreampFilter::displayName() const
{
	return QString("Preamp: %1 dB").arg(_gain, 0, 'f', 1);
}

QString PeakingFilter::toConfigLine() const
{
	return QString("Filter: ON PK Fc %1 Hz Gain %2 dB Q %3")
		.arg(_fc, 0, 'f', 1)
		.arg(_gain, 0, 'f', 1)
		.arg(_q, 0, 'f', 2);
}

QString PeakingFilter::displayName() const
{
	return QString("PK: %1 Hz, %2%3 dB, Q=%4")
		.arg(_fc, 0, 'f', 1)
		.arg(_gain > 0 ? "+" : "")
		.arg(_gain, 0, 'f', 1)
		.arg(_q, 0, 'f', 2);
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
