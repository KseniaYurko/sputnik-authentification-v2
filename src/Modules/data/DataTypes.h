#pragma once
#include <vector>
#include <string>

/**
 * @brief Одна точка наблюдения спутника
 *
 * Содержит измеренные значения азимута и высоты,
 * а также момент времени в формате Julian Date.
 */
struct Observation {
    double azimuth;     ///< Азимут (градусы)
    double elevation;   ///< Высота над горизонтом (градусы)
    double time_jd;     ///< Целая часть Julian Date
    double time_jdFr;   ///< Дробная часть Julian Date
};

/**
 * @brief Координаты точки наблюдения ГСК
 */
struct Observer {
    double lat_deg;  ///< Широта (градусы)
    double lon_deg;  ///< Долгота (градусы)
    double alt_km;   ///< Высота над уровнем моря (км)
};

/**
 * @brief Один элемент каталога спутников (TLE)
 */
struct SatelliteCatalogEntry {
    std::string name;        ///< Название спутника
    int norad_id;            ///< NORAD ID
    std::string tle_line1;   ///< Первая строка TLE
    std::string tle_line2;   ///< Вторая строка TLE
};


/**
 * @brief Группа каталога Celestrak
 */
struct CelestrakGroup {
    std::string display_name;
    std::string group_slug;
    std::string category_name;
    std::string category_slug;
};

/**
 * @brief Категория каталогов Celestrak
 */
struct CelestrakCategory {
    std::string title;
    std::string slug;
    std::vector<CelestrakGroup> groups;
};