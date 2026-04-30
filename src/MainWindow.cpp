#include "MainWindow.h"
#include "version.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
	setWindowTitle(QString{"Equalizer APO Profile Selector v"} + VersionString);
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

	// Search widget (initially hidden)
	searchWidget = new QWidget(profilesGroupBox);
	QHBoxLayout* searchLayout = new QHBoxLayout(searchWidget);
	searchLayout->setContentsMargins(0, 0, 0, 0);

	searchEdit = new QLineEdit(searchWidget);
	searchEdit->setPlaceholderText("Search profiles... (Ctrl+F to focus, Esc to clear)");
	searchEdit->setClearButtonEnabled(true);
	searchLayout->addWidget(searchEdit);

	searchResultLabel = new QLabel(searchWidget);
	searchResultLabel->setStyleSheet("color: gray;");
	searchLayout->addWidget(searchResultLabel);

	searchWidget->setVisible(false);
	groupBoxLayout->addWidget(searchWidget);

	scrollArea = new QScrollArea(profilesGroupBox);
	scrollArea->setWidgetResizable(true);  // Automatically resizes viewport widget to fit contents
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	groupBoxLayout->addWidget(scrollArea);

	QWidget* scrollContent = new QWidget(scrollArea);
	scrollLayout = new QGridLayout(scrollContent);
	scrollLayout->setColumnStretch(0, 1);
	scrollLayout->setColumnStretch(1, 1);
	scrollLayout->setAlignment(Qt::AlignTop);
	scrollLayout->setContentsMargins(0, 0, 0, 0);
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

	QPushButton* openFolder = new QPushButton("Open config folder", this);
	connect(openFolder, &QPushButton::clicked, this, [this] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(_config.configFolder()));
	});
	buttonsLayout->addWidget(openFolder);

	mainLayout->addLayout(buttonsLayout);
	//mainLayout->addStretch();
	setCentralWidget(centralWidget);

	loadConfig();

	connect(profileButtonGroup, &QButtonGroup::buttonToggled, this, &MainWindow::applyChanges);
	connect(preampCheck, &QCheckBox::toggled, this, &MainWindow::applyChanges);
	connect(preampCheck, &QCheckBox::toggled, preampSpin, &QDoubleSpinBox::setEnabled);
	connect(preampSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::applyChanges);

	// Search functionality
	connect(searchEdit, &QLineEdit::textChanged, this, &MainWindow::filterProfiles);

	// Ctrl+F to focus search
	QShortcut* searchShortcut = new QShortcut(QKeySequence::Find, this);
	connect(searchShortcut, &QShortcut::activated, this, &MainWindow::focusSearch);

	// Escape to clear search and hide search widget
	QShortcut* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), searchEdit);
	connect(escapeShortcut, &QShortcut::activated, this, [this] {
		searchEdit->clear();
		searchWidget->setVisible(false);
	});

	// Set the window height to half of the screen height or 600, whichever is smaller
	const int screenHeight = screen() ? screen()->size().height() : 720;
	const int windowHeight = std::max(screenHeight * 2 / 3, 400);
	adjustSize();
	resize(width(), windowHeight);
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

	QWidget* lastCheckedButton = nullptr;

	for (const auto& profile: _config.profiles())
	{
		QRadioButton* profileRadio = new QRadioButton(profile.name, this);
		connect(profileRadio, &QRadioButton::toggled, [this, profileRadio](bool checked) {
			QFont f = profileRadio->font();
			f.setBold(checked);
			profileRadio->setFont(f);

			if (!checked)
				return;
			for (auto* button : profileButtons)
			{
				if (button != profileRadio)
					button->setChecked(false);
			}
		});

		if (profile.enabled)
		{
			profileRadio->setChecked(true);
			lastCheckedButton = profileRadio;
		}
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
	}

	const auto preamp = _config.preamp();
	preampSpin->setValue(preamp.gain);
	preampSpin->setEnabled(preamp.enabled);
	preampCheck->setChecked(preamp.enabled);

	QTimer::singleShot(0, this, [this, lastCheckedButton] {
		if (lastCheckedButton)
			scrollArea->ensureWidgetVisible(lastCheckedButton);
	});
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

void MainWindow::filterProfiles(const QString& searchText)
{
	if (searchText.isEmpty())
	{
		// Show all profiles
		for (auto* btn : profileButtons)
			btn->setVisible(true);
		searchResultLabel->clear();
		return;
	}

	const QString lowerSearch = searchText.toLower().remove(' ');
	size_t visibleCount = 0;

	for (auto* btn : profileButtons)
	{
		const bool matches = btn->text().toLower().remove(' ').contains(lowerSearch);
		btn->setVisible(matches);
		if (matches)
			++visibleCount;
	}

	// Update result label
	if (visibleCount == 0)
		searchResultLabel->setText("No matches");
	else if (visibleCount == profileButtons.size())
		searchResultLabel->setText(QString("All %1 profiles").arg(visibleCount));
	else
		searchResultLabel->setText(QString("%1 of %2").arg(visibleCount).arg(profileButtons.size()));
}

void MainWindow::focusSearch()
{
	searchWidget->setVisible(true);
	searchEdit->setFocus();
	searchEdit->selectAll();
}
