#pragma once

#include <vector>
#include <cmath>
#include "kiss_fft.h"

class SpectrumAnalyzer {
public:
	SpectrumAnalyzer(int fftSize = 2048, int sampleRate = 48000);
	~SpectrumAnalyzer();

	// Process audio samples and update the spectrum
	// Input: mono or stereo samples (will be mixed to mono if stereo)
	void ProcessSamples(const float* samples, int numSamples, int channels = 1);

	// Linear magnitude spectrum, array of size (fftSize/2 + 1)
	[[nodiscard]] const std::vector<float>& GetMagnitudeSpectrum() const;

	// Get frequency for a given bin
	[[nodiscard]] float GetFrequency(int bin) const;

	// Get number of frequency bins
	[[nodiscard]] int GetBinCount() const;

	// Configuration
	void SetFftSize(int fftSize);
	[[nodiscard]] int GetFftSize() const;
	void SetSampleRate(int sampleRate);
	[[nodiscard]] int GetSampleRate() const;

	// Apply window function (improves frequency resolution)
	enum WindowType {
		WINDOW_NONE,
		WINDOW_HANN,
		WINDOW_HAMMING,
		WINDOW_BLACKMAN
	};
	void SetWindowType(WindowType type);

private:
	void InitializeFFT();
	void CleanupFFT();
	void GenerateWindow();
	void MixToMono(const float* samples, int numSamples, int channels);

private:
	int m_fftSize = 0;
	int m_sampleRate = 0;
	WindowType m_windowType = WINDOW_BLACKMAN;

	kiss_fft_cfg m_fftConfig = nullptr;
	std::vector<float> m_inputBuffer;
	std::vector<float> m_window;
	std::vector<kiss_fft_cpx> m_fftInput;
	std::vector<kiss_fft_cpx> m_fftOutput;
	std::vector<float> m_magnitudeSpectrum;
};
