/**
 * @file CelestrakRegistry.cpp
 * @brief Реестр каталогов Celestrak и обработка пользовательского выбора
 */
#include "Modules/data/CelestrakRegistry/CelestrakRegistry.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unordered_set>

/**
 * @brief Вспомогательная функция. Удаляет пробелы в начале и конце строки
 *
 * Используется при обработке пользовательского ввода.
 *
 * @param s Исходная строка
 * @return Обрезанная строка
 */
static std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace((unsigned char)s[b])) ++b;
    size_t e = s.size();
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

/**
 * @brief Приводит строку к нижнему регистру
 *
 * @param s Исходная строка
 * @return Строка в нижнем регистре
 */
static std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

/**
 * @brief Разбивает строку по запятым
 *
 * @param s Входная строка
 * @return Вектор строк (без пустых элементов)
 */
static std::vector<std::string> splitCommaList(const std::string& s) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) tokens.push_back(item);
    }
    return tokens;
}

const std::vector<CelestrakCategory>& getCelestrakCatalogRegistry() {
    static const std::vector<CelestrakCategory> reg = {
        {
            "Special-Interest Satellites", "special-interest-satellites",
            {
                {"Space Stations", "stations", "Special-Interest Satellites", "special-interest-satellites"},
                {"100 (or so) Brightest", "visual", "Special-Interest Satellites", "special-interest-satellites"},
                {"Active Satellites", "active", "Special-Interest Satellites", "special-interest-satellites"},
                {"Analyst Satellites", "analyst", "Special-Interest Satellites", "special-interest-satellites"},
                {"Russian ASAT Test Debris (COSMOS 1408)", "cosmos-1408-debris", "Special-Interest Satellites", "special-interest-satellites"},
                {"Chinese ASAT Test Debris (FENGYUN 1C)", "fengyun-1c-debris", "Special-Interest Satellites", "special-interest-satellites"},
                {"IRIDIUM 33 Debris", "iridium-33-debris", "Special-Interest Satellites", "special-interest-satellites"},
                {"COSMOS 2251 Debris", "cosmos-2251-debris", "Special-Interest Satellites", "special-interest-satellites"},
            }
        },
        {
            "Weather & Earth Resources Satellites", "weather-earth-resources-satellites",
            {
                {"Weather", "weather", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"NOAA", "noaa", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"GOES", "goes", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"Earth Resources", "resource", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"Search & Rescue (SARSAT)", "sarsat", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"Disaster Monitoring", "dmc", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"Tracking and Data Relay Satellite System (TDRSS)", "tdrss", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"ARGOS Data Collection System", "argos", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"Planet", "planet", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
                {"Spire", "spire", "Weather & Earth Resources Satellites", "weather-earth-resources-satellites"},
            }
        },
        {
            "Communications Satellites", "communications-satellites",
            {
                {"Active Geosynchronous", "geo", "Communications Satellites", "communications-satellites"},
                {"GEO Protected Zone", "gpz", "Communications Satellites", "communications-satellites"},
                {"GEO Protected Zone Plus", "gpz-plus", "Communications Satellites", "communications-satellites"},
                {"Intelsat", "intelsat", "Communications Satellites", "communications-satellites"},
                {"SES", "ses", "Communications Satellites", "communications-satellites"},
                {"Eutelsat", "eutelsat", "Communications Satellites", "communications-satellites"},
                {"Telesat", "telesat", "Communications Satellites", "communications-satellites"},
                {"Starlink", "starlink", "Communications Satellites", "communications-satellites"},
                {"OneWeb", "oneweb", "Communications Satellites", "communications-satellites"},
                {"Qianfan", "qianfan", "Communications Satellites", "communications-satellites"},
                {"Hulianwang Digi", "hulianwang", "Communications Satellites", "communications-satellites"},
                {"Kuiper", "kuiper", "Communications Satellites", "communications-satellites"},
                {"Iridium NEXT", "iridium-NEXT", "Communications Satellites", "communications-satellites"},
                {"Orbcomm", "orbcomm", "Communications Satellites", "communications-satellites"},
                {"Globalstar", "globalstar", "Communications Satellites", "communications-satellites"},
                {"Amateur Radio", "amateur", "Communications Satellites", "communications-satellites"},
                {"SatNOGS", "satnogs", "Communications Satellites", "communications-satellites"},
                {"Experimental Comm", "x-comm", "Communications Satellites", "communications-satellites"},
                {"Other Comm", "other-comm", "Communications Satellites", "communications-satellites"},
            }
        },
        {
            "Navigation Satellites", "navigation-satellites",
            {
                {"GNSS", "gnss", "Navigation Satellites", "navigation-satellites"},
                {"GPS Operational", "gps-ops", "Navigation Satellites", "navigation-satellites"},
                {"GLONASS Operational", "glo-ops", "Navigation Satellites", "navigation-satellites"},
                {"Galileo", "galileo", "Navigation Satellites", "navigation-satellites"},
                {"Beidou", "beidou", "Navigation Satellites", "navigation-satellites"},
                {"Satellite-Based Augmentation System (WAAS/EGNOS/MSAS)", "sbas", "Navigation Satellites", "navigation-satellites"},
                {"Navy Navigation Satellite System (NNSS)", "nnss", "Navigation Satellites", "navigation-satellites"},
                {"Russian LEO Navigation", "musson", "Navigation Satellites", "navigation-satellites"},
            }
        },
        {
            "Scientific Satellites", "scientific-satellites",
            {
                {"Space & Earth Science", "science", "Scientific Satellites", "scientific-satellites"},
                {"Geodetic", "geodetic", "Scientific Satellites", "scientific-satellites"},
                {"Engineering", "engineering", "Scientific Satellites", "scientific-satellites"},
                {"Education", "education", "Scientific Satellites", "scientific-satellites"},
            }
        },
        {
            "Miscellaneous Satellites", "miscellaneous-satellites",
            {
                {"Miscellaneous Military", "military", "Miscellaneous Satellites", "miscellaneous-satellites"},
                {"Radar Calibration", "radar", "Miscellaneous Satellites", "miscellaneous-satellites"},
                {"CubeSats", "cubesat", "Miscellaneous Satellites", "miscellaneous-satellites"},
                {"Other Satellites", "other", "Miscellaneous Satellites", "miscellaneous-satellites"},
            }
        }
    };
    return reg;
}

void printCelestrakMenu() {
    std::cout << "Available catalogs (type names separated by commas):\n";
    std::cout << "  - Update All\n";
    for (const auto& cat : getCelestrakCatalogRegistry()) {
        std::cout << "  - " << cat.title << "\n";
        for (const auto& g : cat.groups) {
            std::cout << "      * " << g.display_name << "\n";
        }
    }
    std::cout << "\nExamples:\n";
    std::cout << "  Space Stations\n";
    std::cout << "  Navigation Satellites\n";
    std::cout << "  Space Stations, GPS Operational, Starlink\n";
    std::cout << "  Update All\n\n";
}

std::vector<CelestrakGroup> resolveUserSelection(const std::string& userInput) {
    const auto tokens = splitCommaList(userInput);
    if (tokens.empty()) return {};

    std::vector<CelestrakGroup> allGroups;
    for (const auto& cat : getCelestrakCatalogRegistry())
        for (const auto& g : cat.groups)
            allGroups.push_back(g);

    const std::string inputLower = lower(trim(userInput));
    if (inputLower == "update all" || inputLower == "all") {
        return allGroups;
    }

    std::unordered_set<std::string> want; 
    for (const auto& t : tokens) want.insert(lower(t));

    std::vector<CelestrakGroup> selected;
    selected.reserve(allGroups.size());

    for (const auto& cat : getCelestrakCatalogRegistry()) {
        const bool catChosen = want.count(lower(cat.title)) > 0;
        for (const auto& g : cat.groups) {
            if (catChosen || want.count(lower(g.display_name)) > 0) {
                selected.push_back(g);
            }
        }
    }

    std::unordered_set<std::string> seen;
    std::vector<CelestrakGroup> uniq;
    for (const auto& g : selected) {
        if (seen.insert(lower(g.group_slug)).second) uniq.push_back(g);
    }
    return uniq;
}