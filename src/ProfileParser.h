#pragma once

#include "Filter.h"

#include <QString>

#include <expected>
#include <optional>
#include <vector>

struct ProfileData {
	std::vector<FilterUniquePtr> filters;
};

class ProfileParser {
public:
	// Parse a profile file and return filters
	// Returns error if enabled unsupported filters are found
	static std::expected<ProfileData, QString> parseProfile(const QString& filePath);

	// Write filters back to profile file
	static std::expected<void, QString> saveProfile(const QString& filePath, const std::vector<FilterUniquePtr>& filters);

	// Preamp lines appear both in profile files and in the main config.txt - shared so both parse them identically.
	// isPreampLine: the line has the Preamp key; gain: empty if the value failed to parse.
	struct PreampLine {
		bool isPreampLine = false;
		std::optional<double> gain;
	};
	static PreampLine parsePreampLine(const QString& cleanLine);

private:
	static std::expected<FilterUniquePtr, QString> parseLine(const QString& line);
};
