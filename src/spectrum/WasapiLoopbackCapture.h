#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <functional>

// Link with: Ole32.lib

class WasapiLoopbackCapture {
public:
	WasapiLoopbackCapture();
	~WasapiLoopbackCapture();

	// Initialize the capture device
	HRESULT Initialize();

	// Start/stop capture
	HRESULT Start();
	HRESULT Stop();

	// Call this regularly (e.g., in your message loop or timer)
	// Returns audio data as float samples in range [-1.0, 1.0]
	HRESULT ProcessAudio(std::vector<float>& outSamples);

	// Get audio format info
	UINT32 GetSampleRate() const { return m_sampleRate; }
	UINT32 GetChannelCount() const { return m_channels; }

	// Set callback for when audio is captured
	void SetAudioCallback(std::function<void(const float*, UINT32)> callback) {
		m_audioCallback = callback;
	}

private:
	void Cleanup();
	HRESULT ConvertToFloat(BYTE* data, UINT32 numFrames, std::vector<float>& outSamples);

	IMMDeviceEnumerator* m_deviceEnumerator;
	IMMDevice* m_device;
	IAudioClient* m_audioClient;
	IAudioCaptureClient* m_captureClient;
	WAVEFORMATEX* m_waveFormat;

	UINT32 m_sampleRate;
	UINT32 m_channels;
	UINT32 m_bitsPerSample;

	std::function<void(const float*, UINT32)> m_audioCallback;
	bool m_isCapturing;
};
