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

// The biquad filter types of Equalizer APO (see its BiQuadFilterFactory.cpp)
enum class BiquadType {
	Peaking,   // PK, PEQ, Modal
	LowPass,   // LP, LPQ
	HighPass,  // HP, HPQ
	BandPass,  // BP
	Notch,     // NO
	AllPass,   // AP
	LowShelf,  // LS (corner freq semantics), LSC (center freq)
	HighShelf  // HS (corner freq semantics), HSC (center freq)
};

// How the filter width was specified. Default = not given in the config line;
// E-APO then uses Q=0.7071 (LP/HP/BP), Q=30 (NO) or shelf slope S=0.9 (LS/HS/LSC/HSC).
enum class WidthKind {
	Default,
	Q,
	BandwidthOct, // "BW Oct <n>" - peaking and notch, ignored by E-APO for shelves
	SlopeDb       // "<n> dB" right after the type token - shelves only
};

// Any of E-APO's RBJ cookbook biquad filters
class BiquadFilter final : public IFilter {
public:
	BiquadFilter(BiquadType type, bool shelfUsesCenterFreq, double fc, double gain, WidthKind widthKind, double width, bool enabled = true)
		: _fc(fc), _gain(gain), _width(width), _type(type), _widthKind(widthKind), _shelfUsesCenterFreq(shelfUsesCenterFreq), _enabled(enabled) {}

	QString toConfigLine() const override;
	QString displayName() const override;
	bool isEnabled() const override { return _enabled; }
	void setEnabled(bool enabled) override { _enabled = enabled; }

	BiquadType type() const { return _type; }
	bool shelfUsesCenterFreq() const { return _shelfUsesCenterFreq; }
	double fc() const { return _fc; }
	double gain() const { return _gain; }
	WidthKind widthKind() const { return _widthKind; }
	double width() const { return _width; }

	void setType(BiquadType type, bool shelfUsesCenterFreq)
	{
		_type = type;
		_shelfUsesCenterFreq = shelfUsesCenterFreq;
		// A width kind the new type can't express in E-APO syntax (slope is shelf-only, bandwidth is non-shelf) resets to unspecified
		if ((_widthKind == WidthKind::SlopeDb && !isShelf()) || (_widthKind == WidthKind::BandwidthOct && isShelf()))
			setWidth(WidthKind::Default, 0.0);
	}
	void setFc(double fc) { _fc = fc; }
	void setGain(double gain) { _gain = gain; }
	void setWidth(WidthKind kind, double width) { _widthKind = kind; _width = width; }

	bool isShelf() const { return _type == BiquadType::LowShelf || _type == BiquadType::HighShelf; }
	bool hasGain() const { return _type == BiquadType::Peaking || isShelf(); } // E-APO ignores gain for the other types
	bool requiresWidth() const { return _type == BiquadType::Peaking || _type == BiquadType::AllPass; } // no E-APO default width for these
	QString typeToken() const;

private:
	double _fc = 1000.0;
	double _gain = 0.0;
	double _width = 0.0;
	BiquadType _type = BiquadType::Peaking;
	WidthKind _widthKind = WidthKind::Default;
	bool _shelfUsesCenterFreq = false;
	bool _enabled = true;
};

// Unsupported line (preserved as-is)
class UnsupportedFilter final : public IFilter {
public:
	// noOp: valid line with no effect in E-APO (e.g. "Filter: ON None" placeholders), doesn't block editing the profile
	UnsupportedFilter(const QString& originalLine, bool enabled, bool noOp = false)
		: _originalLine(originalLine), _enabled(enabled), _noOp(noOp) {}

	QString toConfigLine() const override;
	QString displayName() const override;
	bool isEnabled() const override { return _enabled; }
	void setEnabled(bool enabled) override { _enabled = enabled; }

	QString originalLine() const { return _originalLine; }
	bool isNoOp() const { return _noOp; }

private:
	QString _originalLine;
	bool _enabled;
	bool _noOp;
};

// Comment line (preserved verbatim, never enabled; saveProfile re-adds the '#' prefix)
class CommentLine final : public IFilter {
public:
	explicit CommentLine(const QString& text) : _text(text) {}

	QString toConfigLine() const override;
	QString displayName() const override;
	bool isEnabled() const override { return false; }
	void setEnabled(bool) override {}

private:
	QString _text;
};

using FilterUniquePtr = std::unique_ptr<IFilter>;
