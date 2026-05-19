#pragma once
#include <string>

/**
 * @brief Результат сравнения спутника с наблюдениями
 */
struct MatchResult {
    std::string satellite_name; ///< Название спутника
    int norad_id;               ///< NORAD ID
    double chi_squared;         ///< Значение χ²
    double probability;         ///< Posterior P(candidate | data, catalog), %
    double reduced_chi_squared; ///< χ² / degrees of freedom
    double fit_p_value;         ///< Goodness-of-fit p-value for this candidate
    bool catalog_match;         ///< true если candidate совместим при выбранном alpha

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
// struct MeasurementSigma {
//     double sigmaAz_deg;
//     double sigmaEl_deg;
//     double sigmaAzRate_degps;
//     double sigmaElRate_degps;
// };
