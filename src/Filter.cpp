#include "Filter.h"

#include <cmath>
#include <numbers>

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

double BiquadCoefficients::gainDbAt(double frequency, double sampleRate) const
{
	const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
	const double cs = std::cos(omega);
	const double sn = std::sin(omega);

	// Evaluate numerator at z = e^(j*omega)
	const double numReal = b0 + b1 * cs + b2 * std::cos(2.0 * omega);
	const double numImag = b1 * sn + b2 * std::sin(2.0 * omega);
	const double numMag = std::sqrt(numReal * numReal + numImag * numImag);

	// Evaluate denominator at z = e^(j*omega)
	const double denReal = a0 + a1 * cs + a2 * std::cos(2.0 * omega);
	const double denImag = a1 * sn + a2 * std::sin(2.0 * omega);
	const double denMag = std::sqrt(denReal * denReal + denImag * denImag);

	return 20.0 * std::log10(numMag / denMag);
}

BiquadCoefficients BiquadFilter::coefficients(double sampleRate) const
{
	const double gain = hasGain() ? _gain : 0.0;
	const double A = std::pow(10.0, (_type == BiquadType::Peaking || isShelf()) ? gain / 40.0 : gain / 20.0);

	// Resolve the width into Q / shelf slope S / bandwidth in octaves, applying E-APO's defaults when not specified
	enum class WidthMode { Q, S, Bandwidth };
	WidthMode mode = WidthMode::Q;
	double widthValue = 0.0;
	switch (_widthKind)
	{
	case WidthKind::Default:
		if (isShelf())
		{
			mode = WidthMode::S;
			widthValue = 0.9; // E-APO: "found out by experimentation with RoomEQWizard"
		}
		else if (_type == BiquadType::Notch)
			widthValue = 30.0;
		else
			widthValue = 1.0 / std::numbers::sqrt2; // LP/HP/BP; PK/AP never come here (parser and UI guarantee an explicit width)
		break;
	case WidthKind::Q:
		widthValue = _width;
		break;
	case WidthKind::BandwidthOct:
		mode = WidthMode::Bandwidth;
		widthValue = _width;
		break;
	case WidthKind::SlopeDb:
		mode = WidthMode::S;
		widthValue = _width / 12.0; // S = 1 at the maximum slope of 12 dB
		break;
	}

	// LS/HS (unlike LSC/HSC) with an explicit width treat Fc as the corner frequency - convert to the RBJ center frequency
	double fc = _fc;
	if (isShelf() && !_shelfUsesCenterFreq && _widthKind != WidthKind::Default)
	{
		const double q = widthValue;
		const double s = (mode == WidthMode::S) ? widthValue : 1.0 / ((1.0 / (q * q) - 2.0) / (A + 1.0 / A) + 1.0);
		const double centerFreqFactor = std::pow(10.0, std::abs(gain) / 80.0 / s);
		fc = (_type == BiquadType::LowShelf) ? fc * centerFreqFactor : fc / centerFreqFactor;
	}

	const double omega = 2.0 * std::numbers::pi * fc / sampleRate;
	const double sn = std::sin(omega);
	const double cs = std::cos(omega);

	double alpha = 0.0;
	switch (mode)
	{
	case WidthMode::Q:         alpha = sn / (2.0 * widthValue); break;
	case WidthMode::S:         alpha = sn / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / widthValue - 1.0) + 2.0); break;
	case WidthMode::Bandwidth: alpha = sn * std::sinh(std::numbers::ln2 / 2.0 * widthValue * omega / sn); break;
	}
	const double beta = 2.0 * std::sqrt(A) * alpha;

	BiquadCoefficients c{};
	switch (_type)
	{
	case BiquadType::LowPass:
		c.b0 = (1.0 - cs) / 2.0;
		c.b1 = 1.0 - cs;
		c.b2 = (1.0 - cs) / 2.0;
		c.a0 = 1.0 + alpha;
		c.a1 = -2.0 * cs;
		c.a2 = 1.0 - alpha;
		break;
	case BiquadType::HighPass:
		c.b0 = (1.0 + cs) / 2.0;
		c.b1 = -(1.0 + cs);
		c.b2 = (1.0 + cs) / 2.0;
		c.a0 = 1.0 + alpha;
		c.a1 = -2.0 * cs;
		c.a2 = 1.0 - alpha;
		break;
	case BiquadType::BandPass:
		c.b0 = alpha;
		c.b1 = 0.0;
		c.b2 = -alpha;
		c.a0 = 1.0 + alpha;
		c.a1 = -2.0 * cs;
		c.a2 = 1.0 - alpha;
		break;
	case BiquadType::Notch:
		c.b0 = 1.0;
		c.b1 = -2.0 * cs;
		c.b2 = 1.0;
		c.a0 = 1.0 + alpha;
		c.a1 = -2.0 * cs;
		c.a2 = 1.0 - alpha;
		break;
	case BiquadType::AllPass:
		c.b0 = 1.0 - alpha;
		c.b1 = -2.0 * cs;
		c.b2 = 1.0 + alpha;
		c.a0 = 1.0 + alpha;
		c.a1 = -2.0 * cs;
		c.a2 = 1.0 - alpha;
		break;
	case BiquadType::Peaking:
		c.b0 = 1.0 + alpha * A;
		c.b1 = -2.0 * cs;
		c.b2 = 1.0 - alpha * A;
		c.a0 = 1.0 + alpha / A;
		c.a1 = -2.0 * cs;
		c.a2 = 1.0 - alpha / A;
		break;
	case BiquadType::LowShelf:
		c.b0 = A * ((A + 1.0) - (A - 1.0) * cs + beta);
		c.b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
		c.b2 = A * ((A + 1.0) - (A - 1.0) * cs - beta);
		c.a0 = (A + 1.0) + (A - 1.0) * cs + beta;
		c.a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
		c.a2 = (A + 1.0) + (A - 1.0) * cs - beta;
		break;
	case BiquadType::HighShelf:
		c.b0 = A * ((A + 1.0) + (A - 1.0) * cs + beta);
		c.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
		c.b2 = A * ((A + 1.0) + (A - 1.0) * cs - beta);
		c.a0 = (A + 1.0) - (A - 1.0) * cs + beta;
		c.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
		c.a2 = (A + 1.0) - (A - 1.0) * cs - beta;
		break;
	}

	return c;
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
