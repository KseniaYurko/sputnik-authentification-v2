#pragma once

#include "../core/CoreTypes.h"
#include <vector>

/**
 * @brief Модуль вывода результатов
 */
class OutputModule {
public:
    /**
     * @brief Вывод результатов в консоль
     * 
     * @param results Вектор результатов идентификации
     */
    void print(const std::vector<MatchResult>& results);
};