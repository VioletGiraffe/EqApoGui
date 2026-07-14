#pragma once

#include "Filter.h"

#include <cmath>
#include <numbers>
#include <vector>

// Biquad filter coefficients
struct BiquadCoefficients {
	double b0, b1, b2;  // Numerator coefficients
	double a0, a1, a2;  // Denominator coefficients
};

// Coefficients for any of the E-APO biquad filter types: a direct port of E-APO's BiQuad::BiQuad()
// (RBJ Audio EQ Cookbook formulas) plus the corner frequency adjustment from BiQuadFilter::initialize().
inline BiquadCoefficients calculateBiquadCoefficients(const BiquadFilter& filter, double sampleRate = 48000.0)
{
	const BiquadType type = filter.type();
	const double gain = filter.hasGain() ? filter.gain() : 0.0;
	const double A = std::pow(10.0, (type == BiquadType::Peaking || filter.isShelf()) ? gain / 40.0 : gain / 20.0);

	// Resolve the width into Q / shelf slope S / bandwidth in octaves, applying E-APO's defaults when not specified
	enum class WidthMode { Q, S, Bandwidth };
	WidthMode mode = WidthMode::Q;
	double widthValue = 0.0;
	switch (filter.widthKind())
	{
	case WidthKind::Default:
		if (filter.isShelf())
		{
			mode = WidthMode::S;
			widthValue = 0.9; // E-APO: "found out by experimentation with RoomEQWizard"
		}
		else if (type == BiquadType::Notch)
			widthValue = 30.0;
		else
			widthValue = 1.0 / std::numbers::sqrt2; // LP/HP/BP; PK/AP never come here (parser and UI guarantee an explicit width)
		break;
	case WidthKind::Q:
		widthValue = filter.width();
		break;
	case WidthKind::BandwidthOct:
		mode = WidthMode::Bandwidth;
		widthValue = filter.width();
		break;
	case WidthKind::SlopeDb:
		mode = WidthMode::S;
		widthValue = filter.width() / 12.0; // S = 1 at the maximum slope of 12 dB
		break;
	}

	// LS/HS (unlike LSC/HSC) with an explicit width treat Fc as the corner frequency - convert to the RBJ center frequency
	double fc = filter.fc();
	if (filter.isShelf() && !filter.shelfUsesCenterFreq() && filter.widthKind() != WidthKind::Default)
	{
		const double q = widthValue;
		const double s = (mode == WidthMode::S) ? widthValue : 1.0 / ((1.0 / (q * q) - 2.0) / (A + 1.0 / A) + 1.0);
		const double centerFreqFactor = std::pow(10.0, std::abs(gain) / 80.0 / s);
		fc = (type == BiquadType::LowShelf) ? fc * centerFreqFactor : fc / centerFreqFactor;
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
	switch (type)
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

// Calculate magnitude response of a biquad filter at a given frequency
inline double calculateMagnitudeResponse(const BiquadCoefficients& coef, double frequency, double sampleRate = 48000.0)
{
	const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
	const double cs = std::cos(omega);
	const double sn = std::sin(omega);

	// Evaluate numerator at z = e^(j*omega)
	const double numReal = coef.b0 + coef.b1 * cs + coef.b2 * std::cos(2.0 * omega);
	const double numImag = coef.b1 * sn + coef.b2 * std::sin(2.0 * omega);
	const double numMag = std::sqrt(numReal * numReal + numImag * numImag);

	// Evaluate denominator at z = e^(j*omega)
	const double denReal = coef.a0 + coef.a1 * cs + coef.a2 * std::cos(2.0 * omega);
	const double denImag = coef.a1 * sn + coef.a2 * std::sin(2.0 * omega);
	const double denMag = std::sqrt(denReal * denReal + denImag * denImag);

	// Return magnitude in dB
	return 20.0 * std::log10(numMag / denMag);
}

// Calculate combined frequency response for all filters
inline std::vector<double> calculateFrequencyResponse(
	const std::vector<FilterUniquePtr>& filters,
	const std::vector<double>& frequencies,
	double sampleRate = 48000.0)
{
	std::vector<double> response(frequencies.size(), 0.0);

	for (const auto& filter : filters)
	{
		if (!filter->isEnabled())
			continue;

		if (auto* preamp = dynamic_cast<PreampFilter*>(filter.get()))
		{
			// Preamp just adds a constant gain
			for (size_t i = 0; i < response.size(); ++i)
				response[i] += preamp->gain();
		}
		else if (auto* biquad = dynamic_cast<BiquadFilter*>(filter.get()))
		{
			const auto coef = calculateBiquadCoefficients(*biquad, sampleRate);
			for (size_t i = 0; i < frequencies.size(); ++i)
				response[i] += calculateMagnitudeResponse(coef, frequencies[i], sampleRate);
		}
		// Unsupported filters and comments contribute nothing
	}

	return response;
}
