#include "FrequencyResponseWidget.h"
#include "FrequencyResponse.h"

#include <QPainter>
#include <QPen>

#include <array>
#include <cmath>

inline constexpr int MarginLeft = 40;
inline constexpr int MarginRight = 5;
inline constexpr int MarginTop = 5;
inline constexpr int MarginBottom = 25;

// Generate logarithmically-spaced frequencies from 20 Hz to 20 kHz
inline void generateLogFrequencies(std::vector<double>& frequencies, size_t numPoints)
{
	frequencies.resize(numPoints);

	const double minFreq = 15.0;
	const double maxFreq = 20000.0;
	const double logMin = std::log10(minFreq);
	const double logMax = std::log10(maxFreq);

	for (size_t i = 0; i < numPoints; ++i)
	{
		double logFreq = logMin + (logMax - logMin) * i / (numPoints - 1);
		frequencies[i] = std::pow(10.0, logFreq);
	}
}

inline double dbToY(double db, double minDb, double maxDb)
{
	// Linear scale, inverted (0 dB at center, positive up, negative down)
	return 1.0 - (db - minDb) / (maxDb - minDb);
}

FrequencyResponseWidget::FrequencyResponseWidget(QWidget* parent) :
	QWidget(parent)
{
	setMinimumHeight(200);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

	_response.resize(_frequencies.size(), 0.0);
}

void FrequencyResponseWidget::setFilters(const std::vector<FilterUniquePtr>& filters)
{
	_filters = &filters;

	_response.clear();
	_frequencies.clear(); // Will be regenerated in paintEvent

	update();
}

void FrequencyResponseWidget::updateResponse()
{
	assert(_frequencies.size() > 1);

	if (!_filters)
	{
		std::fill(_response.begin(), _response.end(), 0.0);
		_minDb = -12.0;
		_maxDb = 12.0;
		update();
		return;
	}

	_response = calculateFrequencyResponse(*_filters, _frequencies);

	// Calculate dynamic min and max dB values
	auto [minIt, maxIt] = std::minmax_element(_response.begin(), _response.end());
	_minDb = std::floor(*minIt);
	_maxDb = std::ceil(*maxIt);

	update();
}

void FrequencyResponseWidget::paintEvent(QPaintEvent* /*event*/)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Fill background
	painter.fillRect(rect(), Qt::white);

	const auto canvasWidth = width() - MarginLeft - MarginRight;

	if (canvasWidth != _frequencies.size())
	{
		generateLogFrequencies(_frequencies, canvasWidth);
		updateResponse();
	}

	drawGrid(painter);
	drawResponse(painter);
}

void FrequencyResponseWidget::drawGrid(QPainter& painter)
{
	const int graphWidth = width() - MarginLeft - MarginRight;
	const int graphHeight = height() - MarginTop - MarginBottom;

	// Draw border
	painter.setPen(QPen(Qt::black, 2));
	painter.drawRect(MarginLeft, MarginTop, graphWidth, graphHeight);

	// Draw horizontal grid lines (dB)
	painter.setPen(QPen(Qt::lightGray, 1));
	QFont font = painter.font();
	font.setPointSize(9);
	painter.setFont(font);
	QFontMetrics fm(font);
	const int labelHeight = fm.height();

	for (double db = _minDb; db <= _maxDb; db += 3.0)
	{
		const int y = MarginTop + static_cast<int>(dbToY(db, _minDb, _maxDb) * graphHeight);

		if (std::abs(db) < 0.1)  // Zero line
			painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
		else
			painter.setPen(QPen(Qt::lightGray, 1));

		painter.drawLine(MarginLeft, y, MarginLeft + graphWidth, y);

		// Draw label
		painter.setPen(Qt::black);
		QString label = QString("%1 dB").arg(db, 0, 'f', 0);

		if (db == _maxDb)
			painter.drawText(5, labelHeight + 1, label);
		else if (db == _minDb)
			painter.drawText(5, height() - MarginBottom - 1, label);
		else
			painter.drawText(5, y + 5, label);
	}

	// Draw vertical grid lines (frequency)
	const std::array freqMarkers{
		(int)_frequencies.front(),
		20, 30, 40, 60, 80,
		100, 200, 300, 400, 600, 800,
		1000, 2000, 3000, 4000, 5000, 7000,
		10000, 14000,
		(int)_frequencies.back()
	};

	for (size_t i = 0; i < freqMarkers.size(); ++i)
	{
		const int freq = freqMarkers[i];
		int x = MarginLeft + static_cast<int>(freqToX(freq) * graphWidth);
		painter.setPen(QPen(Qt::lightGray, 1));
		painter.drawLine(x, MarginTop, x, MarginTop + graphHeight);

		// Draw label
		painter.setPen(Qt::black);
		QString label;
		if (freq >= 1000 && (freq % 1000 == 0))
			label = QString("%1k").arg(freq / 1000);
		else if (freq >= 1000 && (freq % 1000 != 0))
			label = QString("%1k").arg((float)freq / 1000.0f, 0, 'f', 1);
		else
			label = QString::number(static_cast<int>(freq));

		if (i == freqMarkers.size() - 1) // The last marker is offset to the left so it's not cut off by the right edge
			painter.drawText(x - fm.horizontalAdvance(label), MarginTop + graphHeight + labelHeight, label);
		else
			painter.drawText(x - fm.horizontalAdvance(label) / 2, MarginTop + graphHeight + labelHeight, label);
	}
}

void FrequencyResponseWidget::drawResponse(QPainter& p)
{
	if (_response.empty())
		return;

	const double graphWidth = static_cast<double>(width() - MarginLeft - MarginRight);
	const double graphHeight = static_cast<double>(height() - MarginTop - MarginBottom);

	// Draw the frequency response curve
	p.setPen(QPen(QColor(0, 120, 215), 2));  // Blue curve

	std::vector<QPointF> points;
	points.reserve(_frequencies.size());

	for (size_t i = 0, n = _frequencies.size(); i < n; ++i)
	{
		const double freq = _frequencies[i];
		double db = _response[i];

		// Clamp to visible range
		db = std::max(_minDb, std::min(_maxDb, db));

		const double x = (double)MarginLeft + freqToX(freq) * graphWidth;
		const double y = (double)MarginTop + dbToY(db, _minDb, _maxDb) * graphHeight;

		points.emplace_back(x, y);
	}

	p.drawPolyline(points.data(), (int)points.size());
}

double FrequencyResponseWidget::freqToX(double freq) const
{
	// Logarithmic scale
	double logMin = std::log10(_frequencies.front());
	double logMax = std::log10(_frequencies.back());
	double logFreq = std::log10(freq);
	return (logFreq - logMin) / (logMax - logMin);
}
