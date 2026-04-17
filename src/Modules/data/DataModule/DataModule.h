#pragma once

#include "Modules/data/DataTypes.h"
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
     * @brief Скачать выбранные группы каталогов
     */
    std::vector<std::string>
    updateSelectedGroups(const std::vector<CelestrakGroup>& groups);

    /**
     * @brief Загрузка каталога из нескольких файлов
     *
     * Читает несколько TLE-файлов и объединяет их в один каталог спутников.
     *
     * Алгоритм:
     * 1. Проходит по списку путей
     * 2. Читает содержимое каждого файла
     * 3. Парсит TLE
     * 4. Объединяет результаты в один вектор
     *
     * Если файл не найден — пропускается с выводом предупреждения.
     *
     * @param paths Список путей к TLE-файлам
     * @return Объединённый каталог спутников
     */
    std::vector<SatelliteCatalogEntry>
    loadCatalogFromFiles(const std::vector<std::string>& paths);

    /**
     * @brief Загрузить выбранные каталоги
     *
     * Читает только те файлы, которые соответствуют выбранным группам.
     */
    std::vector<SatelliteCatalogEntry>
    loadSelectedCatalogs(const std::vector<CelestrakGroup>& groups);

    /**
     * @brief Загрузить все каталоги из папки data/
     */
    std::vector<SatelliteCatalogEntry> loadAllCatalogs();
};

