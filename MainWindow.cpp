#include "MainWindow.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>
#include <QThread>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QInputDialog>

#include <stdexcept>

static bool tryOpenFile(QFile& file, QIODevice::OpenMode mode)
{
	for (int attempt = 0; attempt < 5; ++attempt)
	{
		if (file.open(mode))
			return true;
		QThread::msleep(20);
	}
	return false;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
	setWindowTitle("Equalizer APO Profile Switcher");
	QWidget* centralWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
	// Preamp section
	preampCheck = new QCheckBox("Preamp", this);
	preampSpin = new QDoubleSpinBox(this);
	preampSpin->setRange(-30.0, 30.0);
	preampSpin->setSingleStep(0.5);
	preampSpin->setSuffix(" dB");
	preampSpin->setEnabled(false);
	QHBoxLayout* preampLayout = new QHBoxLayout;
	preampLayout->addWidget(preampCheck);
	preampLayout->addWidget(preampSpin);
	mainLayout->addLayout(preampLayout);
	// Profiles section
	profilesGroupBox = new QGroupBox("EQ Profiles", this);
	profilesGroupBox->setLayout(new QVBoxLayout);
	profileButtonGroup = new QButtonGroup(this);
	profileButtonGroup->setExclusive(false); // Allow none selected
	mainLayout->addWidget(profilesGroupBox);

	auto* buttonsLayout = new QHBoxLayout;
	buttonsLayout->setSpacing(1);
	QPushButton* newConfigButton = new QPushButton("Create new EQ", this);
	buttonsLayout->addWidget(newConfigButton);
	connect(newConfigButton, &QPushButton::clicked, this, &MainWindow::createNewConfig);

	QPushButton* editConfigTxt = new QPushButton("Edit config.txt", this);
	connect(editConfigTxt, &QPushButton::clicked, this, &MainWindow::editConfigTxt);
	buttonsLayout->addWidget(editConfigTxt);

	QPushButton* reloadFromDisk = new QPushButton("Reload from disk", this);
	connect(reloadFromDisk, &QPushButton::clicked, this, &MainWindow::loadConfig);
	buttonsLayout->addWidget(reloadFromDisk);

	mainLayout->addLayout(buttonsLayout);
	mainLayout->addStretch();
	setCentralWidget(centralWidget);

	loadConfig();

	connect(profileButtonGroup, &QButtonGroup::buttonToggled, this, &MainWindow::applyChanges);
	connect(preampCheck, &QCheckBox::toggled, this, &MainWindow::applyChanges);
	connect(preampCheck, &QCheckBox::toggled, preampSpin, &QDoubleSpinBox::setEnabled);
	connect(preampSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::applyChanges);
}

void MainWindow::createNewConfig()
{
	QString fileName = QInputDialog::getText(this, "New Config File",
											 "Enter config file name (you can include or omit the .txt extension):", QLineEdit::Normal, "").trimmed();

	if (fileName.isEmpty())
		return;

	if (!fileName.endsWith(".txt", Qt::CaseInsensitive))
		fileName += ".txt";

	const QString filePath = configFolder + "/" + fileName;

	{
		QFile file(filePath);
		if (!file.exists())
		{
			if (!file.open(QIODevice::WriteOnly))
			{
				QMessageBox::warning(this, "Error", "Failed to create file: " + file.errorString());
				return;
			}
		}
	}

	// Append the new config to config.txt as a commented Include so it appears in the UI
	{
		QFile cfg(configFolder + "/config.txt");
		if (!tryOpenFile(cfg, QIODevice::Append | QIODevice::Text)) {
			QMessageBox::warning(this, "Error", "Failed to open config.txt for appending: " + cfg.errorString());
			return;
		}

		const QString includeLine = QString("#Include: %1\n").arg(fileName);
		bool includeExists = false;
		for (const QString& line : std::as_const(includeLines))
		{
			if (line.contains(fileName, Qt::CaseInsensitive) == 0)
			{
				includeExists = true;
				break;
			}
		}

		if (!includeExists)
		{
			cfg.write(includeLine.toUtf8());
			if (cfg.error() != QFile::NoError)
			{
				QMessageBox::warning(this, "Error", "Failed to write to config.txt: " + cfg.errorString());
				return;
			}
		}
	}

	// Refresh UI to reflect the newly added (but commented-out) profile
	loadConfig();

	QProcess::startDetached("notepad.exe", { filePath });
}

void MainWindow::applyChanges()
{
	try {
		// Collect active items
		QStringList newConfig;
		// Preamp
		QString currentPreampLine = QString("Preamp: %1 dB").arg(preampSpin->value(), 0, 'f', 1);
		if (!preampCheck->isChecked())
			currentPreampLine.prepend("#"); // Comment out if disabled
		newConfig << currentPreampLine;

		// Profiles
		for (int i = 0; i < profileButtons.size(); ++i) {
			if (profileButtons[i]->isChecked())
				newConfig << includeLines[i];
			else
				newConfig << "#" + includeLines[i]; // Comment out disabled

		}

		// Write back to file
		QFile configFile(configFolder + "/config.txt");
		if (!tryOpenFile(configFile, QFile::WriteOnly))
			throw std::runtime_error("Failed to open config for writing: " + configFile.errorString().toStdString());

		for (const QString& line : std::as_const(newConfig))
		{
			configFile.write(line.toUtf8().append('\n'));
		}

		configFile.close();
		if (configFile.error() != QFile::NoError)
			throw std::runtime_error("Error closing config after writing: " + configFile.errorString().toStdString());

	}
	catch (const std::exception& e) {
		QMessageBox::warning(this, "Error", QString::fromStdString(e.what()));
	}
}

void MainWindow::loadConfig()
{
	try {
		for (auto* btn : profileButtons) {
			profileButtonGroup->removeButton(btn);
			profilesGroupBox->layout()->removeWidget(btn);
			btn->deleteLater();
		}
		profileButtons.clear();
		includeLines.clear();

		QFile configFile(configFolder + "/config.txt");
		if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
			throw std::runtime_error("Failed to open config for reading: " + configFile.errorString().toStdString());

		QTextStream in(&configFile);
		in.setEncoding(QStringConverter::Utf8);

		QString line;
		bool preampFound = false;
		while (in.readLineInto(&line))
		{
			line = line.trimmed();
			if (line.isEmpty())
				continue;

			const bool isCommented = line.startsWith("#");
			QString cleanLine = isCommented ? line.mid(1).trimmed() : line;
			if (cleanLine.startsWith("Preamp:", Qt::CaseInsensitive)) {
				QString gainStr = cleanLine.remove("Preamp:", Qt::CaseInsensitive).remove("dB", Qt::CaseInsensitive).trimmed();
				bool ok = false;
				const double gain = gainStr.toDouble(&ok);
				if (ok) {
					preampGain = gain;
					preampSpin->setValue(preampGain);
					preampCheck->setChecked(!isCommented);
					preampSpin->setEnabled(!isCommented);
					preampFound = true;
				}
			}
			else if (cleanLine.startsWith("Include:", Qt::CaseInsensitive)) {
				includeLines << cleanLine;

				const QString configFileName = cleanLine.mid(cleanLine.indexOf(':') + 1).trimmed();
				QRadioButton* profileRadio = new QRadioButton(configFileName, this);
				profileRadio->setChecked(!isCommented);
				profileButtonGroup->addButton(profileRadio);
				profileButtons.push_back(profileRadio);
				profilesGroupBox->layout()->addWidget(profileRadio);

				const QString filePath = configFolder + '/' + configFileName;
				profileRadio->setContextMenuPolicy(Qt::CustomContextMenu);
				connect(profileRadio, &QWidget::customContextMenuRequested, this, [this, profileRadio, filePath](QPoint pos) {
					QMenu contextMenu(this);
					QAction* openAction = contextMenu.addAction("Open in Notepad");
					connect(openAction, &QAction::triggered, [filePath]() {
						QProcess::startDetached("notepad.exe", { filePath });
					});
					contextMenu.exec(profileRadio->mapToGlobal(pos));
				});

				connect(profileRadio, &QRadioButton::toggled, [this, profileRadio](bool checked) {
					if (!checked)
						return;

					for (auto* button : profileButtons)
					{
						if (button != profileRadio)
							button->setChecked(false);
					}
				});
			}
			// Ignore other lines for simplicity
		}

		if (in.status() != QTextStream::Ok && in.status() != QTextStream::ReadPastEnd)
			throw std::runtime_error("Error while reading config: " + configFile.errorString().toStdString());

		configFile.close();
		if (configFile.error() != QFile::NoError)
			throw std::runtime_error("Error closing config after reading: " + configFile.errorString().toStdString());

		if (!preampFound) {
			preampSpin->setValue(0.0);
			preampCheck->setChecked(false);
		}
		// If multiple profiles were active, warn or handle (but per spec, enforce one)
		int activeCount = 0;
		for (auto* button : profileButtons)
			if (button->isChecked()) ++activeCount;

		if (activeCount > 1) {
			QMessageBox::information(this, "Note", "Multiple profiles were active; only the first will be kept enabled.");
			for (int i = 1; i < profileButtons.size(); ++i)
				profileButtons[i]->setChecked(false);

		}
		adjustSize();
	}
	catch (const std::exception& e) {
		QMessageBox::warning(this, "Error", QString::fromStdString(e.what()));
	}
}

void MainWindow::editConfigTxt()
{
	QProcess::startDetached("notepad.exe", { configFolder + "/config.txt" });
}
