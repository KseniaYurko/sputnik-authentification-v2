#pragma once
#include <string>

/**
 * @brief Результат сравнения спутника с наблюдениями
 */
struct MatchResult {
    std::string satellite_name; ///< Название спутника
    int norad_id;               ///< NORAD ID
    double chi_squared;         ///< Значение χ²
    double probability;         ///< Вероятность совпадения (%)

    /**
     * @brief Сортировка результатов (по возрастанию χ²)
     */
    bool operator<(const MatchResult& other) const {
        return chi_squared < other.chi_squared;
    }
};

/**
 * @brief Оценки ошибок измерений
 */
struct MeasurementSigma {
    double sigmaAz_deg;
    double sigmaEl_deg;
    double sigmaAzRate_degps;
    double sigmaElRate_degps;
};
