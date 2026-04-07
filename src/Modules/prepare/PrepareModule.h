#pragma once

#include "../data/DataTypes.h"
#include <vector>

/**
 * @brief Модуль предварительной обработки данных
 */
class PrepareModule {
public:
    /**
     * @brief Обработка наблюдений
     */
    std::vector<Observation> process(
        const std::vector<Observation>& input);
};