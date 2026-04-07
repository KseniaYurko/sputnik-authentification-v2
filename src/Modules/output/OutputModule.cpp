#include "OutputModule.h"
#include <iostream>

void OutputModule::print(const std::vector<MatchResult>& results) {
    for (const auto& r : results) {
        std::cout << r.satellite_name
                  << " | NORAD: " << r.norad_id
                  << " | chi^2: " << r.chi_squared
                  << " | P: " << r.probability
                  << std::endl;
    }
}