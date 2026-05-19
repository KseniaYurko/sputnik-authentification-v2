#pragma once

#include "../prepare/PrepareTypes.h"
#include "CoreTypes.h"
#include "Modules/core/sgp4/DateTime.h"
#include "Modules/core/sgp4/Tle.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Основной вычислительный модуль
 *
 * Сопоставляет угловые наблюдения с TLE-каталогом.
 *
 * Для каждого спутника модуль распространяет орбиту через SGP4 на моменты
 * наблюдений, переводит ECI-состояние в topocentric azimuth/elevation для
 * заданного наблюдателя и считает chi-square по угловым невязкам и невязкам
 * угловой скорости между соседними наблюдениями.
 */
class CoreModule {
public:
    /**
     * @brief Идентификация спутников по наблюдениям
     *
     * @param data Подготовленные наблюдения, каталог, observer и sigma
     * @param topN Ограничение на количество результатов (-1 = все)
     * @param verbose Печатать прогресс и агрегированную диагностику
     * @return std::vector<MatchResult> Результаты, отсортированные по chi-square
     *
     * @details
     * probability в MatchResult — posterior probability в процентах:
     * P(candidate | observations, catalog, Gaussian angular error model,
     * uniform catalog prior). Нормировка считается по всем валидным кандидатам
     * до применения topN, поэтому напечатанный top-N не обязан суммироваться
     * в 100%.
     */
    std::vector<MatchResult> identifySatellites(
        const PreparedData& data,
        int topN = -1,
        bool verbose = true
    );

private:
    struct CompiledSatellite {
        SatelliteCatalogEntry source;
        libsgp4::Tle tle;
        libsgp4::DateTime epoch;
        double inclination_deg;

        CompiledSatellite(
            const SatelliteCatalogEntry& source,
            libsgp4::Tle tle
        );
    };

    mutable std::unordered_map<
        std::string,
        std::shared_ptr<const CompiledSatellite>
    > compiledCache_;
};
