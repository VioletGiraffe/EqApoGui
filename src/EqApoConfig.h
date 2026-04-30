#pragma once

#include <QString>

#include <expected>
#include <vector>

struct EqProfile {
	QString name;
	bool enabled;
};

struct PreampState {
	double gain = 0.0;
	bool enabled = false;
};

class EqApoConfig
{
public:
	[[nodiscard]] std::expected<void, QString> reloadConfig() noexcept;

	[[nodiscard]] QString configFolder() const;
	[[nodiscard]] const std::vector<EqProfile>& profiles() const;
	[[nodiscard]] PreampState preamp() const;


	[[nodiscard]] std::expected<QString /* filepath */, QString> createNewProfile(const QString& name) noexcept;
	void setProfileEnabled(size_t index, bool enabled);
	void setPreampGain(double gain, bool enabled);
	[[nodiscard]] std::expected<void, QString> saveState() noexcept;

private:
	const QString _configFolder = "C:/Program Files/EqualizerAPO/config";

	std::vector<EqProfile> _profiles;
	PreampState _preampState;
};
