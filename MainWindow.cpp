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
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QInputDialog>
#include <QScrollArea>

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
	QGroupBox* profilesGroupBox = new QGroupBox("EQ Profiles", this);
	QVBoxLayout* groupBoxLayout = new QVBoxLayout(profilesGroupBox);
	QScrollArea* scrollArea = new QScrollArea(profilesGroupBox);
	scrollArea->setWidgetResizable(true);  // Automatically resizes viewport widget to fit contents
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	groupBoxLayout->addWidget(scrollArea);

	QWidget* scrollContent = new QWidget(scrollArea);
	scrollLayout = new QVBoxLayout(scrollContent);
	scrollArea->setWidget(scrollContent);

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
	//mainLayout->addStretch();
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

	const auto result = _config.createNewProfile(fileName);
	if (!result.has_value())
	{
		QMessageBox::critical(this, "Error", result.error());
		return;
	}

	// Refresh UI to reflect the newly added profile
	loadConfig();
	editFile(result.value());
}

void MainWindow::applyChanges()
{
	assert(profileButtons.size() == _config.profiles().size());

	_config.setPreampGain(preampSpin->value(), preampCheck->isChecked());
	for (size_t i = 0; i < profileButtons.size(); ++i)
	{
		auto* btn = profileButtons[i];
		_config.setProfileEnabled(i, btn->isChecked());
	}

	if (const auto result = _config.saveState(); !result)
		QMessageBox::critical(this, "Error", result.error());
}

void MainWindow::loadConfig()
{
	for (auto* btn : profileButtons)
	{
		profileButtonGroup->removeButton(btn);
		scrollLayout->removeWidget(btn);
		btn->deleteLater();
	}

	profileButtons.clear();
	const auto result = _config.reloadConfig();
	if (!result)
		QMessageBox::critical(this, "Error", result.error());

	for (const auto& profile: _config.profiles())
	{
		QRadioButton* profileRadio = new QRadioButton(profile.name, this);
		profileRadio->setChecked(profile.enabled);
		profileButtonGroup->addButton(profileRadio);
		profileButtons.push_back(profileRadio);
		scrollLayout->addWidget(profileRadio);

		profileRadio->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(profileRadio, &QWidget::customContextMenuRequested, this, [this, profileRadio, name{ profile.name }](QPoint pos) {
			QMenu contextMenu(this);
			QAction* openAction = contextMenu.addAction("Open in Notepad");
			connect(openAction, &QAction::triggered, [this, name]() {
				editFile(name);
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

	const auto preamp = _config.preamp();
	preampSpin->setValue(preamp.gain);
	preampSpin->setEnabled(preamp.enabled);
	preampCheck->setChecked(preamp.enabled);
}

void MainWindow::editConfigTxt()
{
	editFile("config.txt");
}

void MainWindow::editFile(QString fileName)
{
	if (!fileName.contains(':'))
		fileName = _config.configFolder() + '/' + fileName;

	QProcess::startDetached("notepad.exe", { fileName });
}
