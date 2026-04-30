#pragma once

#include <QString>
#include <memory>

// Base filter interface
class IFilter {
public:
	virtual ~IFilter() = default;
	virtual QString toConfigLine() const = 0;
	virtual bool isEnabled() const = 0;
	virtual void setEnabled(bool enabled) = 0;
	virtual QString displayName() const = 0;
};

// Preamp filter (global gain adjustment)
class PreampFilter final : public IFilter {
public:
	PreampFilter(double gain, bool enabled = true)
		: _gain(gain), _enabled(enabled) {}

	QString toConfigLine() const override;
	QString displayName() const override;
	bool isEnabled() const override { return _enabled; }
	void setEnabled(bool enabled) override { _enabled = enabled; }

	double gain() const { return _gain; }
	void setGain(double gain) { _gain = gain; }

private:
	double _gain = 0.0;
	bool _enabled = true;
};

// Peaking filter (PK)
class PeakingFilter final : public IFilter {
public:
	PeakingFilter(double fc, double gain, double q, bool enabled = true)
		: _fc(fc), _gain(gain), _q(q), _enabled(enabled) {}

	QString toConfigLine() const override;
	QString displayName() const override;
	bool isEnabled() const override { return _enabled; }
	void setEnabled(bool enabled) override { _enabled = enabled; }

	double fc() const { return _fc; }
	double gain() const { return _gain; }
	double q() const { return _q; }

	void setFc(double fc) { _fc = fc; }
	void setGain(double gain) { _gain = gain; }
	void setQ(double q) { _q = q; }

private:
	double _fc = 1000.0;  // Center frequency in Hz
	double _gain = 0.0;   // Gain in dB
	double _q = 1.0;      // Q factor
	bool _enabled = true;
};

// Unsupported filter (preserved as-is)
class UnsupportedFilter final : public IFilter {
public:
	UnsupportedFilter(const QString& originalLine, bool enabled)
		: _originalLine(originalLine), _enabled(enabled) {}

	QString toConfigLine() const override;
	QString displayName() const override;
	bool isEnabled() const override { return _enabled; }
	void setEnabled(bool enabled) override { _enabled = enabled; }

	QString originalLine() const { return _originalLine; }

private:
	QString _originalLine;
	bool _enabled;
};

using FilterUniquePtr = std::unique_ptr<IFilter>;
