#include "EqApoConfig.h"

#include <QFile>
#include <QTextStream>
#include <QThread>

#include <algorithm>

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

std::expected<void, QString> EqApoConfig::reloadConfig() noexcept
{
	_profiles.clear();
	_preampState = {};

	QFile configFile(_configFolder + "/config.txt");
	if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
		return std::unexpected("Failed to open config for reading: " + configFile.fileName());

	QTextStream in(&configFile);
	in.setEncoding(QStringConverter::Utf8);

	QString line;
	while (in.readLineInto(&line))
	{
		line = line.trimmed();
		if (line.isEmpty())
			continue;

		const bool isCommented = line.startsWith("#");
		const QString cleanLine = isCommented ? line.mid(1).trimmed() : line;
		if (cleanLine.startsWith("Preamp:", Qt::CaseInsensitive))
		{
			_preampState.enabled = !isCommented;
			const QString gainStr = QString{cleanLine}.remove("Preamp:", Qt::CaseInsensitive).remove("dB", Qt::CaseInsensitive).trimmed();
			bool ok = false;
			const double gain = gainStr.toDouble(&ok);
			if (ok)
				_preampState.gain = gain;
			else
				return std::unexpected("Failed to parse preamp gain from the line\n" + cleanLine);
		}
		else if (cleanLine.startsWith("Include:", Qt::CaseInsensitive))
		{
			const QString configFileName = cleanLine.mid(cleanLine.indexOf(':') + 1).trimmed();
			_profiles.emplace_back(configFileName, !isCommented);
		}
		else
			return std::unexpected("Unknown line in the config: " + line);
	}

	if (in.status() != QTextStream::Ok && in.status() != QTextStream::ReadPastEnd)
		return std::unexpected("Error while reading config: " + configFile.errorString());

	configFile.close();
	if (configFile.error() != QFile::NoError)
		return std::unexpected("Error closing config after reading: " + configFile.errorString());

	return {}; // Success
}

QString EqApoConfig::configFolder() const
{
	return _configFolder;
}

const std::vector<EqProfile>& EqApoConfig::profiles() const
{
	return _profiles;
}

PreampState EqApoConfig::preamp() const
{
	return _preampState;
}

std::expected<QString, QString> EqApoConfig::createNewProfile(const QString& name) noexcept
{
	QString fileName = name;
	if (!fileName.endsWith(".txt", Qt::CaseInsensitive))
		fileName += ".txt";

	const QString filePath = configFolder() + "/" + fileName;

	{
		QFile file(filePath);
		if (!file.exists())
		{
			if (!file.open(QIODevice::WriteOnly))
				return std::unexpected("Failed to create file: " + file.errorString());
		}
	}

	// Append the new config to config.txt as a commented Include so it appears in the UI
	{
		QFile cfg(configFolder() + "/config.txt");
		if (!tryOpenFile(cfg, QIODevice::Append | QIODevice::Text))
			return std::unexpected("Failed to open config.txt for appending: " + cfg.errorString());

		const QString includeLine = QString("#Include: %1\n").arg(fileName);

		const bool includeExists = std::find_if(_profiles.begin(), _profiles.end(), [&](const EqProfile& item) -> bool { return item.name.compare(name, Qt::CaseInsensitive) == 0; }) != _profiles.end();
		if (includeExists)
			return filePath; // Success and do nothing

		cfg.write(includeLine.toUtf8());
		if (cfg.error() != QFile::NoError)
			return std::unexpected("Failed to write to config.txt: " + cfg.errorString());
	}

	return filePath; // success
}

void EqApoConfig::setProfileEnabled(size_t index, bool enabled)
{
	_profiles[index].enabled = enabled;
}

void EqApoConfig::setPreampGain(double gain, bool enabled)
{
	_preampState.gain = gain;
	_preampState.enabled = enabled;
}

std::expected<void, QString> EqApoConfig::saveState() noexcept
{
	QFile configFile(_configFolder + "/config.txt");
	if (!tryOpenFile(configFile, QFile::WriteOnly))
		return std::unexpected("Failed to open config for writing: " + configFile.errorString());

	QString preampLine = QString("Preamp: %1 dB\n").arg(_preampState.gain, 0, 'f', 1);
	if (!_preampState.enabled)
		preampLine.prepend("#"); // Comment out if disabled
	configFile.write(preampLine.toLatin1());

	// Profiles
	for (const EqProfile& profile: std::as_const(_profiles))
	{
		QString line = "Include: " + profile.name + "\n";
		if (!profile.enabled)
			line.prepend('#');

		configFile.write(line.toUtf8());
	}

	configFile.close();
	if (configFile.error() != QFile::NoError)
		return std::unexpected("Error writing to the config file: " + configFile.errorString());

	return {};
}
