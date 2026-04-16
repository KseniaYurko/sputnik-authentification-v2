#include "Modules/data/DataModule.h"
#include "Modules/data/CelestrakRegistry.h"

#include "Modules/prepare/PrepareModule.h"

#include "Modules/core/CoreModule.h"

#include "Modules/output/OutputModule.h"


#include <iostream>

int main() {
    std::cout << "Program started" << std::endl;

    DataModule data;

    /// --- 1. UPDATE ---
    std::cout << "Update catalogs? (1 = yes, 0 = no): ";
    int updateMode;
    std::cin >> updateMode;
    std::cin.ignore();

    if (updateMode == 1) {

        std::cout << "\nSelect catalogs to update:\n";
        printCelestrakMenu();

        std::string input;
        std::getline(std::cin, input);

        auto groups = resolveUserSelection(input);

        data.updateSelectedGroups(groups);
    }

    /// --- 2. LOAD / ANALYSIS ---
    std::cout << "\nSelect catalogs for analysis:\n";
    printCelestrakMenu();

    std::string input2;
    std::getline(std::cin, input2);

    auto selectedGroups = resolveUserSelection(input2);

    auto catalog = data.loadSelectedCatalogs(selectedGroups);

    std::cout << "Total satellites for analysis: "
              << catalog.size() << std::endl;

    PrepareModule prep;
    CoreModule core;
    OutputModule output;

    std::vector<Observation> obs;
    Observer observer{0.0, 0.0, 0.0};

    std::cout << "Before prepare" << std::endl;
    auto prepared = prep.process(obs);

    std::cout << "Before core" << std::endl;
    auto results = core.identifySatellites(prepared, catalog, observer);

    std::cout << "Before output" << std::endl;
    output.print(results);

    std::cout << "Finished" << std::endl;

    return 0;
}