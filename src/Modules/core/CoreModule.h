#pragma once

#include "../data/DataTypes.h"
#include "CoreTypes.h"
#include <vector>

/**
 * @brief Основной вычислительный модуль
 *
 * Выполняет сопоставление наблюдений с каталогом спутников
 * Пока что реализована упрощённая (тестовая) версия алгоритма
 */
class CoreModule {
public:
    /**
     * @brief Идентификация спутников по наблюдениям
     *
     * @param observations Вектор наблюдений
     * @param catalog Каталог спутников (TLE)
     * @param observer Параметры наблюдателя
     * @param topN Ограничение на количество результатов (-1 = все)
     * @return std::vector<MatchResult> Отсортированные результаты
     *
     * @details
     * Текущая реализация:
     * - просто проходит по каталогу
     * - формирует фиктивные значения chi² и вероятности
     * - сортирует результаты
     *
     * Используется для проверки работоспособности
     */
    std::vector<MatchResult> identifySatellites(
        const std::vector<Observation>& observations,
        const std::vector<SatelliteCatalogEntry>& catalog,
        const Observer& observer,
        int topN = -1
    );
};