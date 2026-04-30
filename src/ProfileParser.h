#pragma once

#include "Filter.h"

#include <QString>

#include <expected>
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

private:
	static std::expected<FilterUniquePtr, QString> parseLine(const QString& line);
};
