#include "PrepareModule.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

/**
 * @brief Подготовка всех входных данных для Core
 */
PreparedData PrepareModule::process(
    const std::vector<Observation>& observations,
    const std::vector<SatelliteCatalogEntry>& catalog,
    const Observer& observer,
    const MeasurementSigma& sigma
) {
    PreparedData out;

    /// --- 1. проверка наблюдений ---
    if (observations.empty()) {
        throw std::runtime_error("No observations provided");
    }

    /// --- 2. фильтрация (ниже горизонта убрать) ---
    for (const auto& o : observations) {
        if (o.elevation > 0.0) {
            out.observations.push_back(o);
        }
    }

    if (out.observations.size() < 2) {
        throw std::runtime_error("Not enough valid observations");
    }

    /// --- 3. сортировка по времени ---
    std::sort(out.observations.begin(), out.observations.end(),
        [](const Observation& a, const Observation& b) {
            return (a.time_jd + a.time_jdFr) <
                   (b.time_jd + b.time_jdFr);
        });

    /// --- 4. проверка каталога ---
    if (catalog.empty()) {
        throw std::runtime_error("Empty satellite catalog");
    }

    out.catalog = catalog;

    /// --- 5. проверка observer ---
    if (observer.lat_deg < -90 || observer.lat_deg > 90) {
        throw std::runtime_error("Invalid latitude");
    }

    out.observer = observer;

    /// --- 6. проверка sigma ---
    if (sigma.sigmaAz_deg <= 0 || sigma.sigmaEl_deg <= 0) {
        throw std::runtime_error("Invalid measurement sigma");
    }

    out.sigma = sigma;

    std::cout << "[Prepare] OK: "
              << out.observations.size()
              << " observations ready\n";

    return out;
}