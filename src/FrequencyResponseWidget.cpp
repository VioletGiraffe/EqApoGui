#include "FrequencyResponseWidget.h"
#include "FrequencyResponse.h"

#include <QPainter>
#include <QPen>

#include <cmath>

// Display ranges
inline constexpr double MIN_DB = -12.0;
inline constexpr double MAX_DB = 12.0;
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

inline double dbToY(double db)
{
	// Linear scale, inverted (0 dB at center, positive up, negative down)
	return 1.0 - (db - MIN_DB) / (MAX_DB - MIN_DB);
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
	updateResponse();
}

void FrequencyResponseWidget::updateResponse()
{
	if (!_filters)
	{
		std::fill(_response.begin(), _response.end(), 0.0);
		update();
		return;
	}

	_response = calculateFrequencyResponse(*_filters, _frequencies);
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

	for (double db = MIN_DB; db <= MAX_DB; db += 3.0)
	{
		int y = margin + static_cast<int>(dbToY(db) * graphHeight);

		if (std::abs(db) < 0.1)  // Zero line
			painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
		else
			painter.setPen(QPen(Qt::lightGray, 1));

		painter.drawLine(margin, y, margin + graphWidth, y);

		// Draw label
		painter.setPen(Qt::black);
		QString label = QString("%1").arg(db, 0, 'f', 0);
		painter.drawText(5, y + 5, label + " dB");
		painter.setPen(QPen(Qt::lightGray, 1));
	}

	// Draw vertical grid lines (frequency)
	const std::vector<double> freqMarkers{_frequencies.front(), 25, 50.0, 100.0, 200.0, 500.0, 1e3, 2e3, 5e3, 10e3, _frequencies.back()};

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

		painter.save();
		painter.translate(x, height() - 10);
		painter.rotate(-45);
		painter.drawText(0, 0, label);
		painter.restore();
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
		db = std::max(MIN_DB, std::min(MAX_DB, db));

		const double x = (double)Margin + freqToX(freq) * graphWidth;
		const double y = (double)Margin + dbToY(db) * graphHeight;

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
