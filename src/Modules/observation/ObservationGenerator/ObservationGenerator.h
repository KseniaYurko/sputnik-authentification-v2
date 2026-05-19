#pragma once

#include "Modules/data/DataTypes.h"
#include <vector>

/**
 * @brief Синтетический набор идеальных наблюдений одного спутника
 */
struct GeneratedObservationSet {
    SatelliteCatalogEntry target;
    Observer observer;
    std::vector<Observation> observations;
};

/**
 * @brief Генератор идеальных наблюдений через SGP4
 */
class ObservationGenerator {
public:
    /**
     * @brief Сгенерировать видимый из Москвы проход одного спутника
     *
     * Перебирает каталог, ищет спутник с видимым проходом около эпохи TLE
     * и генерирует count угловых наблюдений без шума.
     */
    GeneratedObservationSet generateIdealMoscowPass(
        const std::vector<SatelliteCatalogEntry>& catalog,
        int count = 10
    ) const;
};
