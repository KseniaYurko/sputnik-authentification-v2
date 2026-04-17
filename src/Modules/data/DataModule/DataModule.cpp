#include "DataModule.h"
#include "Modules/data/CelestrakFetcher/CelestrakFetcher.h"
#include "Modules/data/TLEParser/TLEParser.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <unordered_map>


/**
 * ЗАГЛУШКА!!! ВСТАВИТЬ CatalogLoader
 */
std::vector<SatelliteCatalogEntry> DataModule::loadCatalog(const std::string& path) {
    return {};
}

std::vector<Observation> DataModule::loadObservations(const std::string& path) {
    return {};
}

/**
 * @brief Скачивает группы и сохраняет в data/<category>/<group>.tle
 */
std::vector<std::string>
DataModule::updateSelectedGroups(const std::vector<CelestrakGroup>& groups) {

    std::vector<std::string> saved;

    for (const auto& g : groups) {

        std::string path =
            "data/" + g.category_slug + "/" + g.group_slug + ".tle";

        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path()
        );

        std::cout << "[update] " << g.display_name
                  << " -> " << path << std::endl;

        std::string content = downloadCelestrakTLE(g.group_slug);
        if (content.empty()) {
            std::cout << "[error] failed: " << g.display_name << std::endl;
            continue;
        }
        std::ofstream out(path, std::ios::trunc);
        out << content;

        if (!out) {
            std::cout << "[error] cannot write: " << path << std::endl;
            continue;
        }

        saved.push_back(path);
    }

    std::cout << "[update] Done. Saved " << saved.size() << " files\n";

    return saved;
}

std::vector<SatelliteCatalogEntry>
DataModule::loadCatalogFromFiles(const std::vector<std::string>& paths) {

    std::vector<SatelliteCatalogEntry> result;

    for (const auto& path : paths) {

        std::ifstream file(path);
        if (!file) {
            std::cout << "[skip] " << path << std::endl;
            continue;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        auto part = parseTLE(buffer.str());

        result.insert(result.end(), part.begin(), part.end());
    }

    std::cout << "[Data] Loaded total: " << result.size() << std::endl;

    return result;
}

std::vector<SatelliteCatalogEntry>
DataModule::loadSelectedCatalogs(const std::vector<CelestrakGroup>& groups) {

    std::vector<SatelliteCatalogEntry> result;

    for (const auto& g : groups) {

        std::string path =
            "data/" + g.category_slug + "/" + g.group_slug + ".tle";

        std::ifstream file(path);

        if (!file) {
            std::cout << "[skip] missing: " << path << std::endl;
            continue;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        auto part = parseTLE(buffer.str());

        result.insert(result.end(), part.begin(), part.end());
    }

    /// удаление дубликатов
    std::unordered_map<int, SatelliteCatalogEntry> unique;

    for (const auto& sat : result) {
        unique[sat.norad_id] = sat;
    }

    std::vector<SatelliteCatalogEntry> cleaned;
    for (const auto& [id, sat] : unique) {
        cleaned.push_back(sat);
    }

    std::cout << "[Data] Loaded selected: " << cleaned.size() << std::endl;

    return cleaned;
}

std::vector<SatelliteCatalogEntry>
DataModule::loadAllCatalogs() {

    std::vector<SatelliteCatalogEntry> result;

    for (auto& p : std::filesystem::recursive_directory_iterator("data")) {

        if (p.path().extension() == ".tle") {

            std::ifstream file(p.path());

            if (!file) {
                std::cout << "[skip] " << p.path() << std::endl;
                continue;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();

            auto part = parseTLE(buffer.str());

            result.insert(result.end(), part.begin(), part.end());
        }
    }

    std::cout << "[Data] Loaded total: " << result.size() << std::endl;

    std::unordered_map<int, SatelliteCatalogEntry> unique;

    for (const auto& sat : result) {
        unique[sat.norad_id] = sat;
    }

    std::vector<SatelliteCatalogEntry> cleaned;
    for (const auto& [id, sat] : unique) {
        cleaned.push_back(sat);
    }

    std::cout << "[Data] After dedup: " << cleaned.size() << std::endl;

    return cleaned;
}
