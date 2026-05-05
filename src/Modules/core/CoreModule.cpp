#include "CoreModule.h"

#include <algorithm>

/**
 * @brief Тестовая реализация алгоритма идентификации
 */
std::vector<MatchResult> CoreModule::identifySatellites(
    const std::vector<Observation>& observations,
    const std::vector<SatelliteCatalogEntry>& catalog,
    const Observer& observer,
    int topN
) {
    std::vector<MatchResult> results;

    if (catalog.empty() || observations.empty()) {
        return results;
    }

    /**
     * @note Временная логика:
     * создаём фиктивные результаты на основе каталога.
     * Это нужно только для проверки работы программы.
     */
    for (size_t i = 0; i < catalog.size(); ++i) {
        MatchResult r;

        r.satellite_name = catalog[i].name;
        r.norad_id = catalog[i].norad_id;

        /// Фиктивная метрика (меньше = лучше)
        r.chi_squared = static_cast<double>(i);
        r.probability = 100.0 / (i + 1);
        results.push_back(r);
    }

    /// Сортировка по chi² (используется operator<)
    std::sort(results.begin(), results.end());

    if (topN > 0 && results.size() > static_cast<size_t>(topN)) {
        results.resize(topN);
    }

    return results;
}