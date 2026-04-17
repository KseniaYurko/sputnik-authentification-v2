#pragma once
#include "Modules/data/DataTypes.h"
#include <vector>

/**
 * @brief Данные подготовленные для Core
 */
struct PreparedData {
    std::vector<Observation> observations;
    std::vector<SatelliteCatalogEntry> catalog;
    Observer observer;
    MeasurementSigma sigma;
};