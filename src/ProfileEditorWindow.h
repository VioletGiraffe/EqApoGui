#pragma once

#include "Filter.h"
#include "FrequencyResponseWidget.h"

#include <QMainWindow>

#include <vector>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class ProfileEditorWindow final : public QMainWindow {
public:
	explicit ProfileEditorWindow(const QString& profilePath, QWidget* parent = nullptr);

private slots:
	void addPeakingFilter();
	void saveProfile();
	void onFilterChanged();

private:
	void loadProfile();
	void rebuildFilterUI();
	void createFilterWidget(QVBoxLayout* layout, IFilter* filter, int index);

private:
	const QString _profilePath;
	std::vector<FilterUniquePtr> _filters;

	QScrollArea* _filterScrollArea = nullptr;
	QVBoxLayout* _filterListLayout = nullptr;
	FrequencyResponseWidget* _responseWidget = nullptr;
};
