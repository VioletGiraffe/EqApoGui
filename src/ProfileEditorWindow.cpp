#include "ProfileEditorWindow.h"
#include "ProfileParser.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

ProfileEditorWindow::ProfileEditorWindow(const QString& profilePath, QWidget* parent)
	: QMainWindow(parent), _profilePath(profilePath)
{
	setWindowTitle("Edit Profile: " + QFileInfo(profilePath).fileName());
	setMinimumSize(500, 350);

	auto* central = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(central);
	mainLayout->setContentsMargins(2, 2, 2, 2);
	mainLayout->setSpacing(0);

	// Create the main splitter (vertical)
	QSplitter* mainSplitter = new QSplitter(Qt::Vertical, this);
	mainSplitter->setChildrenCollapsible(false);  // Optional: prevent complete collapse
	mainSplitter->setHandleWidth(6);

	// Frequency response widget at the top
	_responseWidget = new FrequencyResponseWidget(this);
	_responseWidget->setMinimumHeight(200);
	_responseWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	mainSplitter->addWidget(_responseWidget);

	// Filters group (middle section)
	QWidget* filtersContainer = new QWidget(this);
	QVBoxLayout* filtersLayout = new QVBoxLayout(filtersContainer);
	filtersLayout->setSpacing(0);

	_filterScrollArea = new QScrollArea(this);
	_filterScrollArea->setWidgetResizable(true);
	_filterScrollArea->setMinimumHeight(150);

	QWidget* scrollContent = new QWidget();
	_filterListLayout = new QVBoxLayout(scrollContent);
	_filterListLayout->setAlignment(Qt::AlignTop);
	_filterListLayout->setContentsMargins(0, 0, 0, 0);
	_filterListLayout->setSpacing(2);

	_filterScrollArea->setWidget(scrollContent);

	filtersLayout->addWidget(_filterScrollArea);

	// Add filter button
	QPushButton* addPkButton = new QPushButton("Add Peaking Filter", this);
	connect(addPkButton, &QPushButton::clicked, this, &ProfileEditorWindow::addPeakingFilter);
	filtersLayout->addWidget(addPkButton);

	mainSplitter->addWidget(filtersContainer);
	mainSplitter->setStretchFactor(0, 1);
	mainSplitter->setStretchFactor(1, 2);

	// Add splitter to main layout (takes all available space)
	mainLayout->addWidget(mainSplitter, 1);   // stretch factor 1

	// Bottom buttons - pinned at the bottom
	QHBoxLayout* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();

	QPushButton* saveButton = new QPushButton("Save", this);
	connect(saveButton, &QPushButton::clicked, this, &ProfileEditorWindow::saveProfile);
	buttonLayout->addWidget(saveButton);

	QPushButton* cancelButton = new QPushButton("Cancel", this);
	connect(cancelButton, &QPushButton::clicked, this, &ProfileEditorWindow::close);
	buttonLayout->addWidget(cancelButton);

	mainLayout->addLayout(buttonLayout);
	setCentralWidget(central);

	loadProfile();
}

void ProfileEditorWindow::loadProfile()
{
	auto result = ProfileParser::parseProfile(_profilePath);
	if (!result.has_value())
	{
		QMessageBox::critical(this, "Error", "Failed to load profile:\n" + result.error());
		close();
		return;
	}

	_filters = std::move(result.value().filters);
	rebuildFilterUI();
	_responseWidget->setFilters(_filters);
}

void ProfileEditorWindow::rebuildFilterUI()
{
	// Clear existing widgets
	QLayoutItem* item;
	while ((item = _filterListLayout->takeAt(0)) != nullptr)
	{
		if (item->widget())
			item->widget()->deleteLater();
		delete item;
	}

	// Create widgets for each filter
	for (size_t i = 0; i < _filters.size(); ++i)
	{
		createFilterWidget(_filterListLayout, _filters[i].get(), static_cast<int>(i));
	}
}

void ProfileEditorWindow::createFilterWidget(QVBoxLayout* layout, IFilter* filter, int index)
{
	QGroupBox* filterBox = new QGroupBox(this);
	QHBoxLayout* boxLayout = new QHBoxLayout(filterBox);
	boxLayout->setContentsMargins(0, 1, 0, 1);
	boxLayout->setSpacing(6);

	// Enable checkbox
	QCheckBox* enableCheck = new QCheckBox(filterBox);
	enableCheck->setChecked(filter->isEnabled());
	connect(enableCheck, &QCheckBox::toggled, this, [this, filter](bool checked) {
		filter->setEnabled(checked);
		onFilterChanged();
	});
	boxLayout->addWidget(enableCheck);

	if (auto* preamp = dynamic_cast<PreampFilter*>(filter))
	{
		// Preamp controls
		boxLayout->addWidget(new QLabel("Preamp Gain:", filterBox));

		QDoubleSpinBox* gainSpin = new QDoubleSpinBox(filterBox);
		gainSpin->setRange(-30.0, 30.0);
		gainSpin->setSingleStep(0.5);
		gainSpin->setSuffix(" dB");
		gainSpin->setValue(preamp->gain());
		connect(gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, preamp](double value) {
			preamp->setGain(value);
			onFilterChanged();
		});
		boxLayout->addWidget(gainSpin);
	}
	else if (auto* pk = dynamic_cast<PeakingFilter*>(filter))
	{
		// Peaking filter controls
		boxLayout->addWidget(new QLabel("Peak", filterBox));

		boxLayout->addWidget(new QLabel("Frequency:", filterBox));
		QDoubleSpinBox* fcSpin = new QDoubleSpinBox(filterBox);
		fcSpin->setRange(15.0, 20000.0);
		fcSpin->setSingleStep(10.0);
		fcSpin->setSuffix(" Hz");
		fcSpin->setValue(pk->fc());
		connect(fcSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, pk](double value) {
			pk->setFc(value);
			onFilterChanged();
		});
		boxLayout->addWidget(fcSpin);

		boxLayout->addWidget(new QLabel("Gain:", filterBox));
		QDoubleSpinBox* gainSpin = new QDoubleSpinBox(filterBox);
		gainSpin->setRange(-20.0, 20.0);
		gainSpin->setSingleStep(0.1);
		gainSpin->setSuffix(" dB");
		gainSpin->setValue(pk->gain());
		connect(gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, pk](double value) {
			pk->setGain(value);
			onFilterChanged();
		});
		boxLayout->addWidget(gainSpin);

		boxLayout->addWidget(new QLabel("Q:", filterBox));
		QDoubleSpinBox* qSpin = new QDoubleSpinBox(filterBox);
		qSpin->setRange(0.1, 10.0);
		qSpin->setSingleStep(0.1);
		qSpin->setValue(pk->q());
		connect(qSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, pk](double value) {
			pk->setQ(value);
			onFilterChanged();
		});
		boxLayout->addWidget(qSpin);

		// Delete button
		QPushButton* deleteBtn = new QPushButton("Delete", filterBox);
		connect(deleteBtn, &QPushButton::clicked, this, [this, index]() {
			if (index >= 0 && static_cast<size_t>(index) < _filters.size())
			{
				_filters.erase(_filters.begin() + index);
				rebuildFilterUI();
				onFilterChanged();
			}
		});
		boxLayout->addStretch(1);
		boxLayout->addWidget(deleteBtn);
	}
	else if (auto* unsupported = dynamic_cast<UnsupportedFilter*>(filter))
	{
		// Unsupported filter - just show info
		QLabel* label = new QLabel("[Unsupported] " + unsupported->originalLine(), filterBox);
		label->setStyleSheet("color: gray;");
		boxLayout->addWidget(label);
	}

	boxLayout->addStretch();
	layout->addWidget(filterBox);
}

void ProfileEditorWindow::addPeakingFilter()
{
	// Add a default peaking filter
	_filters.push_back(std::make_unique<PeakingFilter>(1000.0, 0.0, 1.0, true));
	rebuildFilterUI();
	onFilterChanged();
}

void ProfileEditorWindow::saveProfile()
{
	auto result = ProfileParser::saveProfile(_profilePath, _filters);
	if (!result.has_value())
	{
		QMessageBox::critical(this, "Error", "Failed to save profile:\n" + result.error());
		return;
	}

	QMessageBox::information(this, "Success", "Profile saved successfully!");
	close();
}

void ProfileEditorWindow::onFilterChanged()
{
	_responseWidget->updateResponse();
}
