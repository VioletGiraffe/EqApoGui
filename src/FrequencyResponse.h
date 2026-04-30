#pragma once

#include "Filter.h"

#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Biquad filter coefficients
struct BiquadCoefficients {
	double b0, b1, b2;  // Numerator coefficients
	double a0, a1, a2;  // Denominator coefficients
};

// Calculate biquad coefficients for a peaking filter using RBJ Audio EQ Cookbook
inline BiquadCoefficients calculatePeakingCoefficients(double fc, double gain, double q, double sampleRate = 48000.0)
{
	const double A = std::pow(10.0, gain / 40.0);  // Amplitude (linear)
	const double omega = 2.0 * M_PI * fc / sampleRate;
	const double sn = std::sin(omega);
	const double cs = std::cos(omega);
	const double alpha = sn / (2.0 * q);

	BiquadCoefficients coef;
	coef.b0 = 1.0 + alpha * A;
	coef.b1 = -2.0 * cs;
	coef.b2 = 1.0 - alpha * A;
	coef.a0 = 1.0 + alpha / A;
	coef.a1 = -2.0 * cs;
	coef.a2 = 1.0 - alpha / A;

	return coef;
}

// Calculate magnitude response of a biquad filter at a given frequency
inline double calculateMagnitudeResponse(const BiquadCoefficients& coef, double frequency, double sampleRate = 48000.0)
{
	const double omega = 2.0 * M_PI * frequency / sampleRate;
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
		else if (auto* pk = dynamic_cast<PeakingFilter*>(filter.get()))
		{
			// Calculate biquad coefficients
			auto coef = calculatePeakingCoefficients(pk->fc(), pk->gain(), pk->q(), sampleRate);

			// Add this filter's response to the total
			for (size_t i = 0; i < frequencies.size(); ++i)
			{
				double mag = calculateMagnitudeResponse(coef, frequencies[i], sampleRate);
				response[i] += mag;
			}
		}
		// Unsupported filters are ignored
	}

	return response;
}
