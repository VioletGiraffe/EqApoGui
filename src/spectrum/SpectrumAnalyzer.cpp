#include "SpectrumAnalyzer.h"
#include <algorithm>
#include <cstring>

#include <numbers>

static constexpr float Pi = std::numbers::pi_v<float>;

SpectrumAnalyzer::SpectrumAnalyzer(int fftSize, int sampleRate)
	: m_fftSize(fftSize)
	, m_sampleRate(sampleRate)
{
	InitializeFFT();
}

SpectrumAnalyzer::~SpectrumAnalyzer() {
	CleanupFFT();
}

void SpectrumAnalyzer::InitializeFFT() {
	CleanupFFT();

	// Create KissFFT configuration for complex-to-complex FFT
	m_fftConfig = kiss_fft_alloc(m_fftSize, 0, nullptr, nullptr);

	// Allocate buffers
	m_inputBuffer.resize(m_fftSize, 0.0f);
	m_window.resize(m_fftSize);
	m_fftInput.resize(m_fftSize);
	m_fftOutput.resize(m_fftSize);
	m_powerMagnitude.resize(m_fftSize / 2 + 1, -100.0f);

	GenerateWindow();
}

void SpectrumAnalyzer::CleanupFFT() {
	if (m_fftConfig) {
		kiss_fft_free(m_fftConfig);
		m_fftConfig = nullptr;
	}
}



// Get the spectrum magnitude
// Returns array of size (fftSize/2 + 1)
const std::vector<float>& SpectrumAnalyzer::GetPowerSpectrum() const {
	return m_powerMagnitude;
}

// Get frequency for a given bin
float SpectrumAnalyzer::GetFrequency(int bin) const {
	return (float)bin * m_sampleRate / m_fftSize;
}

// Get number of frequency bins
int SpectrumAnalyzer::GetBinCount() const {
	return m_fftSize / 2 + 1;
}

void SpectrumAnalyzer::SetFftSize(int fftSize) {
	if (fftSize != m_fftSize) {
		m_fftSize = fftSize;
		InitializeFFT();
	}
}

int SpectrumAnalyzer::GetFftSize() const {
	return m_fftSize;
}

void SpectrumAnalyzer::SetSampleRate(int sampleRate) {
	m_sampleRate = sampleRate;
}

int SpectrumAnalyzer::GetSampleRate() const {
	return m_sampleRate;
}

void SpectrumAnalyzer::SetWindowType(WindowType type) {
	if (type != m_windowType) {
		m_windowType = type;
		GenerateWindow();
	}
}

void SpectrumAnalyzer::GenerateWindow() {
	for (int i = 0; i < m_fftSize; i++) {
		float value = 1.0f;
		const float n = (float)i / (m_fftSize - 1);

		switch (m_windowType) {
		case WINDOW_HANN:
			value = 0.5f * (1.0f - cosf(2.0f * Pi * n));
			break;

		case WINDOW_HAMMING:
			value = 0.54f - 0.46f * cosf(2.0f * Pi * n);
			break;

		case WINDOW_BLACKMAN:
			value = 0.42f - 0.5f * cosf(2.0f * Pi * n) + 0.08f * cosf(4.0f * Pi * n);
			break;

		case WINDOW_NONE:
		default:
			value = 1.0f;
			break;
		}

		m_window[i] = value;
	}
}

void SpectrumAnalyzer::MixToMono(const float* samples, int numSamples, int channels) {
	int framesToProcess = std::min(numSamples / channels, m_fftSize);

	if (channels == 1) {
		// Already mono, just copy
		memcpy(m_inputBuffer.data(), samples, framesToProcess * sizeof(float));
	}
	else {
		// Mix stereo (or multi-channel) to mono
		for (int i = 0; i < framesToProcess; i++) {
			float sum = 0.0f;
			for (int c = 0; c < channels; c++) {
				sum += samples[i * channels + c];
			}
			m_inputBuffer[i] = sum / channels;
		}
	}

	// Zero-pad if necessary
	if (framesToProcess < m_fftSize) {
		memset(&m_inputBuffer[framesToProcess], 0,
			(m_fftSize - framesToProcess) * sizeof(float));
	}
}

void SpectrumAnalyzer::ProcessSamples(const float* samples, int numSamples, int channels) {
	// Mix to mono and fill input buffer
	MixToMono(samples, numSamples, channels);

	// Apply window and prepare FFT input
	for (int i = 0; i < m_fftSize; i++) {
		m_fftInput[i].r = m_inputBuffer[i] * m_window[i];
		m_fftInput[i].i = 0.0f;
	}

	// Perform FFT
	kiss_fft(m_fftConfig, m_fftInput.data(), m_fftOutput.data());

	// Calculate magnitude spectrum in dB
	// Only need first half of spectrum (Nyquist)
	const int numBins = m_fftSize / 2 + 1;
	const float normalFactor = 1.0f / (float)m_fftSize;

	for (int i = 0; i < numBins; i++) {
		float real = m_fftOutput[i].r;
		float imag = m_fftOutput[i].i;

		// Calculate magnitude
		const float magnitude = sqrtf(real * real + imag * imag) * normalFactor;

		// Convert to dB (20 * log10(magnitude))
		// Add small epsilon to avoid log(0)
		//float db = 20.0f * log10f(magnitude + 1e-10f);

		m_powerMagnitude[i] = magnitude;
	}
}
