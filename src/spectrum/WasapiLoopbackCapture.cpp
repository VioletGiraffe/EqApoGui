#include "WasapiLoopbackCapture.h"
#include <functiondiscoverykeys_devpkey.h>

WasapiLoopbackCapture::WasapiLoopbackCapture()
	: m_deviceEnumerator(nullptr)
	, m_device(nullptr)
	, m_audioClient(nullptr)
	, m_captureClient(nullptr)
	, m_waveFormat(nullptr)
	, m_sampleRate(0)
	, m_channels(0)
	, m_bitsPerSample(0)
	, m_isCapturing(false)
{
	CoInitialize(nullptr);
}

WasapiLoopbackCapture::~WasapiLoopbackCapture() {
	Cleanup();
	CoUninitialize();
}

void WasapiLoopbackCapture::Cleanup() {
	Stop();

	if (m_waveFormat) {
		CoTaskMemFree(m_waveFormat);
		m_waveFormat = nullptr;
	}

	if (m_captureClient) {
		m_captureClient->Release();
		m_captureClient = nullptr;
	}

	if (m_audioClient) {
		m_audioClient->Release();
		m_audioClient = nullptr;
	}

	if (m_device) {
		m_device->Release();
		m_device = nullptr;
	}

	if (m_deviceEnumerator) {
		m_deviceEnumerator->Release();
		m_deviceEnumerator = nullptr;
	}
}

HRESULT WasapiLoopbackCapture::Initialize() {
	HRESULT hr;

	// Create device enumerator
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		(void**)&m_deviceEnumerator);
	if (FAILED(hr)) return hr;

	// Get default audio endpoint (speakers/headphones)
	hr = m_deviceEnumerator->GetDefaultAudioEndpoint(
		eRender, eConsole, &m_device);
	if (FAILED(hr)) return hr;

	// Activate audio client
	hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
		nullptr, (void**)&m_audioClient);
	if (FAILED(hr)) return hr;

	// Get the mix format
	hr = m_audioClient->GetMixFormat(&m_waveFormat);
	if (FAILED(hr)) return hr;

	// Store format info
	m_sampleRate = m_waveFormat->nSamplesPerSec;
	m_channels = m_waveFormat->nChannels;
	m_bitsPerSample = m_waveFormat->wBitsPerSample;

	// Initialize audio client in loopback mode
	hr = m_audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		0,  // hnsBufferDuration (use default)
		0,  // hnsPeriodicity (use default)
		m_waveFormat,
		nullptr);
	if (FAILED(hr)) return hr;

	// Get the capture client
	hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient),
		(void**)&m_captureClient);
	if (FAILED(hr)) return hr;

	return S_OK;
}

HRESULT WasapiLoopbackCapture::Start() {
	if (m_isCapturing) return S_OK;

	HRESULT hr = m_audioClient->Start();
	if (SUCCEEDED(hr)) {
		m_isCapturing = true;
	}
	return hr;
}

HRESULT WasapiLoopbackCapture::Stop() {
	if (!m_isCapturing) return S_OK;

	HRESULT hr = m_audioClient->Stop();
	m_isCapturing = false;
	return hr;
}

HRESULT WasapiLoopbackCapture::ProcessAudio(std::vector<float>& outSamples) {
	if (!m_isCapturing) return E_NOT_VALID_STATE;

	HRESULT hr;
	UINT32 packetLength = 0;
	BYTE* data = nullptr;
	UINT32 numFramesAvailable = 0;
	DWORD flags = 0;

	outSamples.clear();

	// Check if data is available
	hr = m_captureClient->GetNextPacketSize(&packetLength);
	if (FAILED(hr)) return hr;

	while (packetLength != 0) {
		// Get the captured data
		hr = m_captureClient->GetBuffer(
			&data,
			&numFramesAvailable,
			&flags,
			nullptr,
			nullptr);

		if (FAILED(hr)) return hr;

		// Convert to float and append
		if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
			ConvertToFloat(data, numFramesAvailable, outSamples);
		}
		else {
			// Silent buffer - append zeros
			outSamples.resize(outSamples.size() + numFramesAvailable * m_channels, 0.0f);
		}

		// Release the buffer
		hr = m_captureClient->ReleaseBuffer(numFramesAvailable);
		if (FAILED(hr)) return hr;

		// Check for next packet
		hr = m_captureClient->GetNextPacketSize(&packetLength);
		if (FAILED(hr)) return hr;
	}

	// Trigger callback if we have data
	if (!outSamples.empty() && m_audioCallback) {
		m_audioCallback(outSamples.data(), (UINT32)outSamples.size());
	}

	return S_OK;
}

HRESULT WasapiLoopbackCapture::ConvertToFloat(BYTE* data, UINT32 numFrames, std::vector<float>& outSamples) {
	UINT32 sampleCount = numFrames * m_channels;
	size_t currentSize = outSamples.size();
	outSamples.resize(currentSize + sampleCount);

	if (m_waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
		(m_waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
			reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_waveFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
		// Already float format
		float* floatData = reinterpret_cast<float*>(data);
		memcpy(&outSamples[currentSize], floatData, sampleCount * sizeof(float));
	}
	else if (m_bitsPerSample == 16) {
		// 16-bit PCM to float
		INT16* pcmData = reinterpret_cast<INT16*>(data);
		for (UINT32 i = 0; i < sampleCount; i++) {
			outSamples[currentSize + i] = pcmData[i] / 32768.0f;
		}
	}
	else if (m_bitsPerSample == 24) {
		// 24-bit PCM to float (packed in 32-bit container)
		BYTE* pcmData = data;
		for (UINT32 i = 0; i < sampleCount; i++) {
			INT32 sample = (pcmData[i * 3] << 8) | (pcmData[i * 3 + 1] << 16) | (pcmData[i * 3 + 2] << 24);
			outSamples[currentSize + i] = sample / 2147483648.0f;
		}
	}
	else if (m_bitsPerSample == 32) {
		// 32-bit PCM to float
		INT32* pcmData = reinterpret_cast<INT32*>(data);
		for (UINT32 i = 0; i < sampleCount; i++) {
			outSamples[currentSize + i] = pcmData[i] / 2147483648.0f;
		}
	}

	return S_OK;
}
