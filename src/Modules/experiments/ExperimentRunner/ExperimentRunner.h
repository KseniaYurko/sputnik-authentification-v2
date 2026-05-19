#pragma once

#include "Modules/core/CoreModule.h"
#include "Modules/data/DataTypes.h"
#include "Modules/prepare/PrepareTypes.h"

#include <chrono>

/**
 * @brief Запуск экспериментальных прогонов для дипломных графиков
 */
class ExperimentRunner {
public:
    /**
     * @brief Запустить базовый набор экспериментов и сохранить CSV
     */
    void runDefaultSuite(
        const PreparedData& prepared,
        const SatelliteCatalogEntry& target
    ) const;

private:
    void runNoiseRobustness(
        const PreparedData& prepared,
        const SatelliteCatalogEntry& target,
        const std::chrono::steady_clock::time_point& suiteStart
    ) const;

    void runCatalogPresenceDetection(
        const PreparedData& prepared,
        const SatelliteCatalogEntry& target,
        const std::chrono::steady_clock::time_point& suiteStart
    ) const;

    void runRuntimeByCatalogSize(
        const PreparedData& prepared,
        const std::chrono::steady_clock::time_point& suiteStart
    ) const;

    void runOrbitTypeRobustness(
        const PreparedData& prepared,
        const std::chrono::steady_clock::time_point& suiteStart
    ) const;
};
