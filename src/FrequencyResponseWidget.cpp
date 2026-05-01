#include "FrequencyResponseWidget.h"
#include "FrequencyResponse.h"

#include <QPainter>
#include <QPen>

#include <array>
#include <cmath>

inline constexpr int Margin = 50;

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

	const auto canvasWidth = width() - 2 * Margin;

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
	const int margin = 50;
	const int graphWidth = width() - 2 * margin;
	const int graphHeight = height() - 2 * margin;

	// Draw border
	painter.setPen(QPen(Qt::black, 2));
	painter.drawRect(margin, margin, graphWidth, graphHeight);

	// Draw horizontal grid lines (dB)
	painter.setPen(QPen(Qt::lightGray, 1));
	QFont font = painter.font();
	font.setPointSize(9);
	painter.setFont(font);

	for (double db = _minDb; db <= _maxDb; db += 3.0)
	{
		int y = margin + static_cast<int>(dbToY(db, _minDb, _maxDb) * graphHeight);

		if (std::abs(db) < 0.1)  // Zero line
			painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
		else
			painter.setPen(QPen(Qt::lightGray, 1));

		painter.drawLine(margin, y, margin + graphWidth, y);

		// Draw label
		painter.setPen(Qt::black);
		QString label = QString("%1").arg(db, 0, 'f', 0);
		painter.drawText(5, y + 5, label + " dB");
	}

	// Draw vertical grid lines (frequency)
	const std::array freqMarkers{ _frequencies.front(), 25.0, 50.0, 100.0, 200.0, 500.0, 1e3, 2e3, 5e3, 10e3, _frequencies.back() };

	QFontMetrics fm(font);
	const int labelHeight = fm.height();

	for (double freq : freqMarkers)
	{
		int x = margin + static_cast<int>(freqToX(freq) * graphWidth);
		painter.setPen(QPen(Qt::lightGray, 1));
		painter.drawLine(x, margin, x, margin + graphHeight);

		// Draw label
		painter.setPen(Qt::black);
		QString label;
		if (freq >= 1000)
			label = QString("%1k").arg(freq / 1000, 0, 'f', freq == 1000 ? 0 : 1);
		else
			label = QString::number(static_cast<int>(freq));

		painter.drawText(x - fm.horizontalAdvance(label) / 2, margin + graphHeight + labelHeight, label);
	}
}

void FrequencyResponseWidget::drawResponse(QPainter& p)
{
	if (_response.empty())
		return;

	const double graphWidth = static_cast<double>(width() - 2 * Margin);
	const double graphHeight = static_cast<double>(height() - 2 * Margin);

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

		const double x = (double)Margin + freqToX(freq) * graphWidth;
		const double y = (double)Margin + dbToY(db, _minDb, _maxDb) * graphHeight;

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
