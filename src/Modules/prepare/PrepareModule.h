#pragma once

#include "PrepareTypes.h"
#include <vector>

/**
 * @brief Модуль предварительной обработки данных
 */
class PrepareModule {
public:
    /**
     * @brief Обработка
     */
    PreparedData process(
        const std::vector<Observation>& observations,
        const std::vector<SatelliteCatalogEntry>& catalog,
        const Observer& observer,
        const MeasurementSigma& sigma
    );
};