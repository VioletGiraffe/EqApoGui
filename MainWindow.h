#pragma once

#include <QMainWindow>
#include <QStringList>

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
	const QString configFolder = "C:/Program Files/EqualizerAPO/config"; // Adjust if needed;

	std::vector<QRadioButton*> profileButtons;
	QStringList includeLines;

	QCheckBox* preampCheck = nullptr;
	QDoubleSpinBox* preampSpin = nullptr;
	double preampGain = 0.0;

	QButtonGroup* profileButtonGroup = nullptr;
	QGroupBox* profilesGroupBox = nullptr;
};
