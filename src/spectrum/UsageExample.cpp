// Example usage of WasapiLoopbackCapture and SpectrumAnalyzer
// Complete standalone Windows application

#include "WasapiLoopbackCapture.h"
#include "SpectrumAnalyzer.h"
#include <Windows.h>
#include <string>

// Global instances
static WasapiLoopbackCapture g_capture;
static SpectrumAnalyzer g_analyzer(65536, 48000);  // 2048 FFT size, 48kHz sample rate
static HWND g_hwnd = nullptr;

// Initialize audio capture
void InitializeAudioCapture() {
	HRESULT hr;

	// Initialize WASAPI capture
	hr = g_capture.Initialize();
	if (FAILED(hr)) {
		MessageBox(nullptr, L"Failed to initialize audio capture", L"Error", MB_OK);
		return;
	}

	// Update analyzer sample rate to match capture
	g_analyzer.SetSampleRate(g_capture.GetSampleRate());

	// Start capturing
	hr = g_capture.Start();
	if (FAILED(hr)) {
		MessageBox(nullptr, L"Failed to start audio capture", L"Error", MB_OK);
		return;
	}
}

// Process audio and update spectrum
void UpdateSpectrum() {
	std::vector<float> audioSamples;

	// Get captured audio
	HRESULT hr = g_capture.ProcessAudio(audioSamples);
	if (SUCCEEDED(hr) && !audioSamples.empty()) {
		// Process with FFT
		g_analyzer.ProcessSamples(audioSamples.data(),
			(int)audioSamples.size(),
			g_capture.GetChannelCount());

		// Trigger window redraw
		InvalidateRect(g_hwnd, nullptr, FALSE);
	}
}

// Draw spectrum visualization
void DrawSpectrum(HDC hdc, RECT clientRect) {
	// Clear background
	HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 30));
	FillRect(hdc, &clientRect, bgBrush);
	DeleteObject(bgBrush);

	const std::vector<float>& spectrum = g_analyzer.GetPowerSpectrum();
	const int numBins = g_analyzer.GetBinCount();

	constexpr int maxFreq = 16000;
	const int sampleRate = g_analyzer.GetSampleRate();
	int displayBins = (numBins * maxFreq) / (sampleRate / 2);
	displayBins = min(displayBins, numBins);

	if (displayBins == 0)
		return;

	float barWidth = (float)clientRect.right / displayBins;

	for (int i = 0; i < displayBins; i++) {
		float db = spectrum[i];

		// Map dB to screen height
		// Range: -60dB (silence) to 0dB (full scale)
		float normalized = (db + 60.0f) / 60.0f;
		normalized = max(0.0f, min(1.0f, normalized));

		int barHeight = (int)(normalized * clientRect.bottom);

		if (barHeight > 0) {
			// Create bar rectangle
			RECT bar;
			bar.left = (LONG)(i * barWidth);
			bar.right = (LONG)((i + 1) * barWidth - 2);  // 2px gap between bars
			bar.top = clientRect.bottom - barHeight;
			bar.bottom = clientRect.bottom;

			// Color gradient based on height (green -> yellow -> red)
			int r = (int)(normalized * 255);
			int g = (int)((1.0f - normalized * 0.5f) * 255);
			int b = 0;

			HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
			FillRect(hdc, &bar, brush);
			DeleteObject(brush);
		}
	}

	// Draw frequency labels
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(200, 200, 200));

	wchar_t text[64];
	wsprintf(text, L"0 Hz");
	TextOut(hdc, 5, clientRect.bottom - 20, text, lstrlen(text));

	wsprintf(text, L"%d kHz", maxFreq / 1000);
	TextOut(hdc, clientRect.right - 60, clientRect.bottom - 20, text, lstrlen(text));

	wsprintf(text, L"Sample Rate: %d Hz | FFT Size: %d",
		g_analyzer.GetSampleRate(), g_analyzer.GetFftSize());
	TextOut(hdc, 5, 5, text, lstrlen(text));
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		g_hwnd = hwnd;
		InitializeAudioCapture();
		// Set timer for 60 FPS (~16ms)
		SetTimer(hwnd, 1, 16, nullptr);
		break;

	case WM_TIMER:
		if (wParam == 1) {
			UpdateSpectrum();
		}
		break;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT clientRect;
		GetClientRect(hwnd, &clientRect);

		DrawSpectrum(hdc, clientRect);

		EndPaint(hwnd, &ps);
		break;
	}

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		g_capture.Stop();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow) {
	// Register window class
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = L"SpectrumAnalyzerClass";

	if (!RegisterClassEx(&wc)) {
		MessageBox(nullptr, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Create window
	HWND hwnd = CreateWindowEx(
		0,
		L"SpectrumAnalyzerClass",
		L"Real-Time Audio Spectrum Analyzer",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		800, 600,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (hwnd == nullptr) {
		MessageBox(nullptr, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// Message loop
	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
