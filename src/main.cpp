#include "Modules/data/DataModule/DataModule.h"
#include "Modules/data/CelestrakRegistry/CelestrakRegistry.h"

#include "Modules/prepare/PrepareModule.h"

#include "Modules/core/CoreModule.h"

#include "Modules/output/OutputModule.h"


#include <iostream>

int main() {
    std::cout << "Program started" << std::endl;

    DataModule data;
    PrepareModule prep;
    CoreModule core;
    OutputModule output;

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

    /// --- 3. ВХОДНЫЕ ДАННЫЕ (пока заглушка) ---
    std::vector<Observation> observations;

    // TODO: потом сюда чтение файла наблюдений
    observations.push_back({0.0, 30.0, 2451545.0, 0.1});
    observations.push_back({10.0, 35.0, 2451545.0, 0.2});

    Observer observer{55.0, 37.0, 0.2};

    MeasurementSigma sigma{
        0.1, 0.1,
        0.01, 0.01
    };

    /// --- 4. PREPARE ---
    PreparedData prepared;

    try {
        prepared = prep.process(
            observations,
            catalog,
            observer,
            sigma
        );
    }
    catch (const std::exception& e) {
        std::cout << "[Prepare ERROR] " << e.what() << std::endl;
        return 1;
    }

    /// --- 5. CORE ---
    auto results = core.identifySatellites(
        prepared.observations,
        prepared.catalog,
        prepared.observer
    );

    /// --- 6. OUTPUT ---
    output.print(results);

    std::cout << "Finished" << std::endl;

    return 0;
}