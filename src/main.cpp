#include "Modules/data/DataModule.h"
#include "Modules/prepare/PrepareModule.h"
#include "Modules/core/CoreModule.h"
#include "Modules/output/OutputModule.h"

#include <iostream>

// int main() {
//     std::cout << "HELLO 1\n";
//     std::cout.flush();

//     exit(0);
// }

int main() {
    std::cout << "Program started" << std::endl;

    DataModule data;
    PrepareModule prep;
    CoreModule core;
    OutputModule output;

    std::vector<Observation> obs;
    Observer observer{0.0, 0.0, 0.0};

    std::cout << "Loading catalog..." << std::endl;

    std::vector<SatelliteCatalogEntry> catalog;

    try {
        catalog = data.loadCatalogFromCelestrak("stations");
        std::cout << "Loaded satellites: " << catalog.size() << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Before prepare" << std::endl;
    auto prepared = prep.process(obs);

    std::cout << "Before core" << std::endl;
    auto results = core.identifySatellites(prepared, catalog, observer);

    std::cout << "Before output" << std::endl;
    output.print(results);

    std::cout << "Finished" << std::endl;

    return 0;
}