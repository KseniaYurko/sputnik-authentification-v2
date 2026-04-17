#include "Modules/data/TLEParser/TLEParser.h"
#include <sstream>

std::vector<SatelliteCatalogEntry> parseTLE(const std::string& content) {
    std::vector<SatelliteCatalogEntry> result;

    std::istringstream stream(content);
    std::string name, line1, line2;

    while (std::getline(stream, name) &&
           std::getline(stream, line1) &&
           std::getline(stream, line2)) {

        SatelliteCatalogEntry sat;
        sat.name = name;
        sat.tle_line1 = line1;
        sat.tle_line2 = line2;

        // NORAD ID из строки 1 (позиции 2-7)
        if (line1.size() > 7) {
            sat.norad_id = std::stoi(line1.substr(2, 5));
        } else {
            sat.norad_id = 0;
        }

        result.push_back(sat);
    }

    return result;
}