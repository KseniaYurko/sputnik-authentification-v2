#include "Modules/data/DataModule.h"
#include "Modules/prepare/PrepareModule.h"
#include "Modules/core/CoreModule.h"
#include "Modules/output/OutputModule.h"

#include <iostream>

int main() {
    std::cout << "Program started\n";

    DataModule data;
    PrepareModule prep;
    CoreModule core;
    OutputModule output;

    std::vector<Observation> obs;
    std::vector<SatelliteCatalogEntry> catalog;
    Observer observer{0.0, 0.0, 0.0};

    std::cout << "Before prepare\n";
    auto prepared = prep.process(obs);

    std::cout << "Before core\n";
    auto results = core.identifySatellites(prepared, catalog, observer);

    std::cout << "Before output\n";
    output.print(results);

    std::cout << "Finished\n";

    return 0;
}