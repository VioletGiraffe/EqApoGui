#pragma once

#include "Filter.h"

#include <QWidget>
#include <vector>

class FrequencyResponseWidget final : public QWidget {
public:
	explicit FrequencyResponseWidget(QWidget* parent = nullptr);

	void setFilters(const std::vector<FilterUniquePtr>& filters);
	void updateResponse();

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	void drawGrid(QPainter& painter);
	void drawResponse(QPainter& painter);
	double freqToX(double freq) const;

private:
	std::vector<double> _frequencies;
	std::vector<double> _response;
	const std::vector<FilterUniquePtr>* _filters = nullptr;
};
