#pragma once

#include "../data/DataTypes.h"
#include "CoreTypes.h"
#include <vector>

/**
 * @brief Основной вычислительный модуль
 *
 * Выполняет сопоставление наблюдений с каталогом спутников
 */
class CoreModule {
public:
    /**
     * @brief Основной алгоритм
     */
    std::vector<MatchResult> identifySatellites(
        const std::vector<Observation>& observations,
        const std::vector<SatelliteCatalogEntry>& catalog,
        const Observer& observer,
        int topN = -1
    );
};