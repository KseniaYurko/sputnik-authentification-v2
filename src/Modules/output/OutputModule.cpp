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
                  << " | reduced chi^2: " << r.reduced_chi_squared
                  << " | P(catalog): " << r.probability << "%"
                  << " | fit p-value: " << r.fit_p_value
                  << " | fit: "
                  << (r.catalog_match ? "compatible" : "rejected")
                  << std::endl;
    }
}
