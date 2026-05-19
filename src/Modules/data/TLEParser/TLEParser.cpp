#include "Modules/data/TLEParser/TLEParser.h"
#include <sstream>
#include <algorithm> 

/**
 * @brief Удаление \r
 */
static void trimLine(std::string& s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
}

/**
 * @brief Проверка что строка похожа на TLE
 */
static bool isValidTLELine(const std::string& line, char expectedStart) {
    if (line.size() != 69) return false;   // TLE 69 символов
    if (line[0] != expectedStart) return false;
    return true;
}

std::vector<SatelliteCatalogEntry> parseTLE(const std::string& content) {
    std::vector<SatelliteCatalogEntry> result;

    std::istringstream stream(content);
    std::string name, line1, line2;

    while (std::getline(stream, name)) {
        if (!std::getline(stream, line1)) break;
        if (!std::getline(stream, line2)) break;

        trimLine(name);
        trimLine(line1);
        trimLine(line2);

        if (!isValidTLELine(line1, '1') || !isValidTLELine(line2, '2')) {
            continue;
        }

        SatelliteCatalogEntry sat;
        sat.name = name;
        sat.tle_line1 = line1;
        sat.tle_line2 = line2;

        try {
            sat.norad_id = std::stoi(line1.substr(2, 5));
        } catch (...) {
            sat.norad_id = 0;
        }
        

        result.push_back(sat);
    }
    

    return result;
}