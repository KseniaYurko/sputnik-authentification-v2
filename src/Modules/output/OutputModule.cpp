#include "OutputModule.h"
#include <iostream>

void OutputModule::print(const std::vector<MatchResult>& results) {
    std::cout << "\n=== RESULTS ===\n";

    if (results.empty()) {
        std::cout << "No matches found\n";
        return;
    }

    int index = 1;

    for (const auto& r : results) {
        std::cout << index++ << ". "
                  << r.satellite_name
                  << " | NORAD: " << r.norad_id
                  << " | chi^2: " << r.chi_squared
                  << " | Probability: " << r.probability
                  << std::endl;
    }
}