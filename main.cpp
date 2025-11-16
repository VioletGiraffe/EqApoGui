#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QFile>
#include <QMessageBox>
#include <QGroupBox>
#include <QStringList>
#include <QThread>

#include <stdexcept>

class ApoSwitcher : public QMainWindow {
public:
	ApoSwitcher(QWidget *parent = nullptr) : QMainWindow(parent) {
		setWindowTitle("Equalizer APO Profile Switcher");
		QWidget *centralWidget = new QWidget(this);
		QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
		// Preamp section
		preampCheck = new QCheckBox("Preamp", this);
		mainLayout->addWidget(preampCheck);
		// Profiles section
		profilesGroupBox = new QGroupBox("EQ Profiles", this);
		profilesGroupBox->setLayout(new QVBoxLayout);

		profileButtonGroup = new QButtonGroup(this);
		profileButtonGroup->setExclusive(false); // Allow none selected
		mainLayout->addWidget(profilesGroupBox);
		mainLayout->addStretch();
		setCentralWidget(centralWidget);

		configPath = "C:/Program Files/EqualizerAPO/config/config.txt"; // Adjust if needed
		loadConfig();

		connect(profileButtonGroup, &QButtonGroup::buttonToggled, this, &ApoSwitcher::applyChanges);
		connect(preampCheck, &QCheckBox::toggled, this, &ApoSwitcher::applyChanges);
	}
private:
	void applyChanges() {
		try {
			// Collect active items
			QStringList newConfig;
			// Preamp
			if (preampCheck->isChecked() && !preampLine.isEmpty())
				newConfig << preampLine;

			else if (!preampCheck->isChecked() && !preampLine.isEmpty())
				newConfig << "#" + preampLine; // Comment out if disabled

			// Profiles
			for (int i = 0; i < profileButtons.size(); ++i) {
				if (profileButtons[i]->isChecked())
					newConfig << includeLines[i];

				else
					newConfig << "#" + includeLines[i]; // Comment out disabled

			}
			// Write back to file
			QFile configFile(configPath);
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

		} catch (const std::exception& e) {
			QMessageBox::warning(this, "Error", QString::fromStdString(e.what()));
		}
	}
	void loadConfig() {
		try {
			QFile configFile(configPath);
			if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
				throw std::runtime_error("Failed to open config for reading: " + configFile.errorString().toStdString());

			QTextStream in(&configFile);
			QString line;
			includeLines.clear();
			profileButtons.clear();
			while (in.readLineInto(&line)) {
				line = line.trimmed();
				if (line.isEmpty())
					continue;

				const bool isCommented = line.startsWith("#");
				QString cleanLine = isCommented ? line.mid(1).trimmed() : line;
				if (cleanLine.startsWith("Preamp:", Qt::CaseInsensitive)) {
					preampLine = cleanLine;
					preampCheck->setChecked(!isCommented);
				} else if (cleanLine.startsWith("Include:", Qt::CaseInsensitive)) {
					includeLines << cleanLine;
					QRadioButton *profileRadio = new QRadioButton(cleanLine, this);
					profileRadio->setChecked(!isCommented);
					profileButtonGroup->addButton(profileRadio);
					profileButtons << profileRadio;
					profilesGroupBox->layout()->addWidget(profileRadio);
					connect(profileRadio, &QRadioButton::toggled, [this, profileRadio](bool checked) {
						if (checked)
							for (auto* button : profileButtons) {
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
		} catch (const std::exception& e) {
			QMessageBox::warning(this, "Error", QString::fromStdString(e.what()));
		}
	}
	QString configPath;
	QCheckBox *preampCheck;
	QString preampLine;
	QButtonGroup *profileButtonGroup;
	QVector<QRadioButton*> profileButtons;
	QStringList includeLines;
	QGroupBox *profilesGroupBox;
};


int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	ApoSwitcher window;
	window.show();
	return app.exec();
}
