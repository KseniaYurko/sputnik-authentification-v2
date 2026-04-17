#pragma once
#include "Modules/data/DataTypes.h"
#include <vector>
#include <string>

/**
 * @brief Парсинг TLE из строки
 */
std::vector<SatelliteCatalogEntry> parseTLE(const std::string& content);