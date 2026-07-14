#include "ProfileEditorWindow.h"
#include "ProfileParser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
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
	QPushButton* addFilterButton = new QPushButton("Add Filter", this);
	connect(addFilterButton, &QPushButton::clicked, this, &ProfileEditorWindow::addFilter);
	filtersLayout->addWidget(addFilterButton);

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

	// Close on Escape key
	QShortcut* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this, nullptr, nullptr, Qt::WindowShortcut);
	connect(escShortcut, &QShortcut::activated, this, &ProfileEditorWindow::close);

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

	// Comment lines are read-only text, no enable checkbox
	if (auto* comment = dynamic_cast<CommentLine*>(filter))
	{
		QLabel* label = new QLabel(comment->displayName(), filterBox);
		label->setStyleSheet("color: gray;");
		boxLayout->addWidget(label);
		boxLayout->addStretch();
		layout->addWidget(filterBox);
		return;
	}

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
		gainSpin->setRange(-60.0, 60.0);
		gainSpin->setSingleStep(0.5);
		gainSpin->setSuffix(" dB");
		gainSpin->setValue(preamp->gain());
		connect(gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, preamp](double value) {
			preamp->setGain(value);
			onFilterChanged();
		});
		boxLayout->addWidget(gainSpin);
	}
	else if (auto* biquad = dynamic_cast<BiquadFilter*>(filter))
	{
		struct TypeEntry { const char* label; BiquadType type; bool centerFreq; };
		static constexpr TypeEntry typeEntries[] = {
			{ "PK (peaking)", BiquadType::Peaking, false },
			{ "LP (low-pass)", BiquadType::LowPass, false },
			{ "HP (high-pass)", BiquadType::HighPass, false },
			{ "BP (band-pass)", BiquadType::BandPass, false },
			{ "NO (notch)", BiquadType::Notch, false },
			{ "AP (all-pass)", BiquadType::AllPass, false },
			{ "LS (low shelf, corner Fc)", BiquadType::LowShelf, false },
			{ "HS (high shelf, corner Fc)", BiquadType::HighShelf, false },
			{ "LSC (low shelf)", BiquadType::LowShelf, true },
			{ "HSC (high shelf)", BiquadType::HighShelf, true },
		};

		QComboBox* typeCombo = new QComboBox(filterBox);
		for (const TypeEntry& entry : typeEntries)
		{
			typeCombo->addItem(entry.label);
			if (entry.type == biquad->type() && entry.centerFreq == biquad->shelfUsesCenterFreq())
				typeCombo->setCurrentIndex(typeCombo->count() - 1);
		}
		connect(typeCombo, &QComboBox::currentIndexChanged, this, [this, biquad](int comboIndex) {
			const TypeEntry& entry = typeEntries[comboIndex];
			biquad->setType(entry.type, entry.centerFreq);
			if (biquad->requiresWidth() && biquad->widthKind() == WidthKind::Default)
				biquad->setWidth(WidthKind::Q, 0.7071); // E-APO has no default width for PK/AP
			rebuildFilterUI(); // the set of shown fields differs per type
			onFilterChanged();
		});
		boxLayout->addWidget(typeCombo);

		boxLayout->addWidget(new QLabel("Fc:", filterBox));
		QDoubleSpinBox* fcSpin = new QDoubleSpinBox(filterBox);
		fcSpin->setRange(1.0, 48000.0); // beyond the drawn 15-20k range: subsonic HP rumble filters and >20 kHz LP on 96 kHz setups exist
		fcSpin->setSingleStep(10.0);
		fcSpin->setSuffix(" Hz");
		fcSpin->setValue(biquad->fc());
		connect(fcSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, biquad](double value) {
			biquad->setFc(value);
			onFilterChanged();
		});
		boxLayout->addWidget(fcSpin);

		if (biquad->hasGain())
		{
			boxLayout->addWidget(new QLabel("Gain:", filterBox));
			QDoubleSpinBox* gainSpin = new QDoubleSpinBox(filterBox);
			gainSpin->setRange(-60.0, 60.0);
			gainSpin->setSingleStep(0.1);
			gainSpin->setSuffix(" dB");
			gainSpin->setValue(biquad->gain());
			connect(gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, biquad](double value) {
				biquad->setGain(value);
				onFilterChanged();
			});
			boxLayout->addWidget(gainSpin);
		}

		// Width: Q / bandwidth / shelf slope, labeled by how the profile specified it
		const WidthKind widthKind = biquad->widthKind();
		const char* widthLabel = widthKind == WidthKind::BandwidthOct ? "BW Oct:" : (widthKind == WidthKind::SlopeDb ? "Slope:" : "Q:");
		boxLayout->addWidget(new QLabel(widthLabel, filterBox));
		QDoubleSpinBox* widthSpin = new QDoubleSpinBox(filterBox);
		widthSpin->setDecimals(4);
		widthSpin->setSingleStep(0.1);
		if (biquad->requiresWidth())
			widthSpin->setRange(0.05, 100.0);
		else
		{
			widthSpin->setRange(0.0, 100.0);
			widthSpin->setSpecialValueText("default"); // 0 = E-APO's per-type default width
		}
		if (widthKind == WidthKind::SlopeDb)
		{
			widthSpin->setSuffix(" dB");
			widthSpin->setMaximum(12.0); // S = slope/12 must not exceed 1: steeper slopes make the RBJ alpha formula NaN (in E-APO too)
		}
		widthSpin->setValue(widthKind == WidthKind::Default ? 0.0 : biquad->width());
		connect(widthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, biquad, widthSpin](double value) {
			// Below the 4-decimal display resolution = zero. Qt's stepping accumulates float error and can land on
			// e.g. +5.6e-18 instead of 0, which displays as "0.0000" instead of "default" and would poison the math.
			if (value < 0.00005)
			{
				biquad->setWidth(WidthKind::Default, 0.0);
				if (value > 0.0)
					widthSpin->setValue(0.0); // snap to the real minimum; re-fires valueChanged, which then takes the value == 0 path
			}
			else // a fresh width on a "default" filter is a Q; otherwise keep the kind the profile used
				biquad->setWidth(biquad->widthKind() == WidthKind::Default ? WidthKind::Q : biquad->widthKind(), value);
			onFilterChanged();
		});
		boxLayout->addWidget(widthSpin);

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
	else if (dynamic_cast<UnsupportedFilter*>(filter))
	{
		// Unsupported or no-op filter - just show info
		QLabel* label = new QLabel(filter->displayName(), filterBox);
		label->setStyleSheet("color: gray;");
		boxLayout->addWidget(label);
	}

	boxLayout->addStretch();
	layout->addWidget(filterBox);
}

void ProfileEditorWindow::addFilter()
{
	// A default peaking filter; the type can be changed in the filter's row
	_filters.push_back(std::make_unique<BiquadFilter>(BiquadType::Peaking, false, 1000.0, 0.0, WidthKind::Q, 1.0, true));
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
