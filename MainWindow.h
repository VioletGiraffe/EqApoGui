#pragma once
#include "EqApoConfig.h"

#include <QMainWindow>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QRadioButton;

class MainWindow final : public QMainWindow {
public:
	MainWindow(QWidget* parent = nullptr);

private:
	void createNewConfig();
	void applyChanges();
	void loadConfig();
	void editConfigTxt();
	void editFile(QString fileName);

private:
	EqApoConfig _config;
	std::vector<QRadioButton*> profileButtons;

	QCheckBox* preampCheck = nullptr;
	QDoubleSpinBox* preampSpin = nullptr;

	QButtonGroup* profileButtonGroup = nullptr;
	QGroupBox* profilesGroupBox = nullptr;
};
