#pragma once
#include "EqApoConfig.h"

#include <QMainWindow>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QRadioButton;
class QScrollArea;
class QWidget;

class MainWindow final : public QMainWindow {
public:
	MainWindow(QWidget* parent = nullptr);

private:
	void createNewConfig();
	void applyChanges();
	void loadConfig();
	void editConfigTxt();
	void editFile(QString fileName);
	void filterProfiles(const QString& searchText);
	void focusSearch();

private:
	EqApoConfig _config;
	std::vector<QRadioButton*> profileButtons;

	QCheckBox* preampCheck = nullptr;
	QDoubleSpinBox* preampSpin = nullptr;

	QButtonGroup* profileButtonGroup = nullptr;
	QGridLayout* scrollLayout = nullptr;
	QScrollArea* scrollArea = nullptr;

	QWidget* searchWidget = nullptr;
	QLineEdit* searchEdit = nullptr;
	QLabel* searchResultLabel = nullptr;
};
