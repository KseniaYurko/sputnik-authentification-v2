#pragma once

#include "DataTypes.h"
#include <string>
#include <vector>

/**
 * @brief Модуль загрузки и предоставления данных
 *
 * Отвечает за чтение файлов и преобразование их
 * в структуры данных.
 */
class DataModule {
public:
    /**
     * @brief Загрузка каталога спутников из TLE-файла
     */
    std::vector<SatelliteCatalogEntry> loadCatalog(const std::string& path);

    /**
     * @brief Загрузка наблюдений (пока заглушка)
     */
    std::vector<Observation> loadObservations(const std::string& path);

    /**
     * @brief Загрузка каталога с Celestrak
     */
    std::vector<SatelliteCatalogEntry> loadCatalogFromCelestrak(
        const std::string& groupName);
};

