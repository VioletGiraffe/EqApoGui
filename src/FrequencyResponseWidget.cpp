#include "FrequencyResponseWidget.h"
#include "FrequencyResponse.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <cmath>

// Display ranges
inline constexpr double MIN_DB = -12.0;
inline constexpr double MAX_DB = 12.0;

// Generate logarithmically-spaced frequencies from 20 Hz to 20 kHz
inline std::vector<double> generateLogFrequencies(size_t numPoints)
{
	std::vector<double> frequencies;
	frequencies.reserve(numPoints);

	const double minFreq = 15.0;
	const double maxFreq = 20000.0;
	const double logMin = std::log10(minFreq);
	const double logMax = std::log10(maxFreq);

	for (size_t i = 0; i < numPoints; ++i)
	{
		double logFreq = logMin + (logMax - logMin) * i / (numPoints - 1);
		frequencies.push_back(std::pow(10.0, logFreq));
	}

	return frequencies;
}


FrequencyResponseWidget::FrequencyResponseWidget(QWidget* parent) :
	QWidget(parent),
	_frequencies{ generateLogFrequencies(1000) }
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

void FrequencyResponseWidget::drawResponse(QPainter& painter)
{
	if (_response.empty())
		return;

	const int margin = 50;
	const int graphWidth = width() - 2 * margin;
	const int graphHeight = height() - 2 * margin;

	// Draw the frequency response curve
	painter.setPen(QPen(QColor(0, 120, 215), 2));  // Blue curve

	QPainterPath path;
	bool firstPoint = true;

	for (size_t i = 0; i < _frequencies.size(); ++i)
	{
		double freq = _frequencies[i];
		double db = _response[i];

		// Clamp to visible range
		db = std::max(MIN_DB, std::min(MAX_DB, db));

		int x = margin + static_cast<int>(freqToX(freq) * graphWidth);
		int y = margin + static_cast<int>(dbToY(db) * graphHeight);

		if (firstPoint)
		{
			path.moveTo(x, y);
			firstPoint = false;
		}
		else
		{
			path.lineTo(x, y);
		}
	}

	painter.drawPath(path);
}

double FrequencyResponseWidget::freqToX(double freq) const
{
	// Logarithmic scale
	double logMin = std::log10(_frequencies.front());
	double logMax = std::log10(_frequencies.back());
	double logFreq = std::log10(freq);
	return (logFreq - logMin) / (logMax - logMin);
}

double FrequencyResponseWidget::dbToY(double db) const
{
	// Linear scale, inverted (0 dB at center, positive up, negative down)
	return 1.0 - (db - MIN_DB) / (MAX_DB - MIN_DB);
}
