#include <QApplication>
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

#include <stdexcept>

class ApoSwitcher final : public QMainWindow {
public:
	ApoSwitcher(QWidget* parent = nullptr) : QMainWindow(parent)
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
		mainLayout->addStretch();
		setCentralWidget(centralWidget);

		loadConfig();

		connect(profileButtonGroup, &QButtonGroup::buttonToggled, this, &ApoSwitcher::applyChanges);
		connect(preampCheck, &QCheckBox::toggled, this, &ApoSwitcher::applyChanges);
		connect(preampCheck, &QCheckBox::toggled, preampSpin, &QDoubleSpinBox::setEnabled);
		connect(preampSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ApoSwitcher::applyChanges);
	}

private:
	void applyChanges()
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
			const auto tryOpenFile = [&]() -> bool {
				for (size_t attempt = 0; attempt < 5; ++attempt) {
					if (configFile.open(QIODevice::WriteOnly))
						return true;

					QThread::msleep(20);
				}
				return false;
				};

			if (!tryOpenFile())
				throw std::runtime_error("Failed to open config for writing: " + configFile.errorString().toStdString());

			for (const QString& line : std::as_const(newConfig))
			{
				const QByteArray data = line.toLatin1().append('\n');
				if (configFile.write(data) != data.size())
					throw std::runtime_error("Failed to write to config file: " + configFile.errorString().toStdString());

			}
			configFile.close();
			if (configFile.error() != QFileDevice::NoError)
				throw std::runtime_error("Error closing config after writing: " + configFile.errorString().toStdString());

		}
		catch (const std::exception& e) {
			QMessageBox::warning(this, "Error", QString::fromStdString(e.what()));
		}
	}

	void loadConfig()
	{
		try {
			QFile configFile(configFolder + "/config.txt");
			if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
				throw std::runtime_error("Failed to open config for reading: " + configFile.errorString().toStdString());

			QTextStream in(&configFile);
			in.setEncoding(QStringConverter::Utf8);

			QString line;
			includeLines.clear();
			profileButtons.clear();
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
					QRadioButton* profileRadio = new QRadioButton(cleanLine, this);
					profileRadio->setChecked(!isCommented);
					profileButtonGroup->addButton(profileRadio);
					profileButtons << profileRadio;
					profilesGroupBox->layout()->addWidget(profileRadio);

					const QString filePath = configFolder + '/' +  cleanLine.mid(cleanLine.indexOf(':') + 1).trimmed();
					profileRadio->setContextMenuPolicy(Qt::CustomContextMenu);
					connect(profileRadio, &QWidget::customContextMenuRequested, this, [this, profileRadio, filePath](const QPoint& pos) {
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
			if (configFile.error() != QFileDevice::NoError)
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


	QString configFolder = "C:/Program Files/EqualizerAPO/config"; // Adjust if needed;
	QCheckBox* preampCheck;
	double preampGain = 0.0;
	QDoubleSpinBox* preampSpin;
	QButtonGroup* profileButtonGroup;
	QVector<QRadioButton*> profileButtons;
	QStringList includeLines;
	QGroupBox* profilesGroupBox;
};


int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	ApoSwitcher window;
	window.show();
	return app.exec();
}
