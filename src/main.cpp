#include "Modules/app/ConsoleInterface/ConsoleInterface.h"

#include "Modules/data/DataModule/DataModule.h"

#include "Modules/experiments/ExperimentRunner/ExperimentRunner.h"

#include "Modules/observation/ObservationGenerator/ObservationGenerator.h"

#include "Modules/prepare/PrepareModule.h"

#include "Modules/core/CoreModule.h"

#include "Modules/output/OutputModule.h"


#include <iostream>
#include <vector>

int main() {
    ConsoleInterface console;
    DataModule data;
    ExperimentRunner experiments;
    ObservationGenerator generator;
    PrepareModule prep;
    CoreModule core;
    OutputModule output;

    console.printBanner();

    /// --- 1. ОБНОВЛЕНИЕ ДАННЫХ ---
    bool updateMode = console.promptUpdateCatalogs();

    if (updateMode) {

        auto groups = console.promptCatalogSelection("Catalogs to update");

        console.printSelectedGroups(groups);
        data.updateSelectedGroups(groups);
    }

    /// --- 2. ЗАГРУЗКА ВЫБРАННЫХ КАТАЛОГОВ ---
    auto selectedGroups = console.promptCatalogSelection("Catalogs for analysis");

    console.printSelectedGroups(selectedGroups);
    console.ensureLocalCatalogs(data, selectedGroups);
    auto catalog = data.loadSelectedCatalogs(selectedGroups);

    std::cout << "Total satellites for analysis: "
              << catalog.size() << std::endl;

    /// --- 3. ВХОДНЫЕ ДАННЫЕ ---
    GeneratedObservationSet generated;

    try {
        generated = generator.generateIdealMoscowPass(catalog, 10);
    }
    catch (const std::exception& e) {
        std::cout << "[Generator ERROR] " << e.what() << std::endl;
        return 1;
    }

    std::vector<Observation> observations = generated.observations;
    Observer observer = generated.observer;

    std::cout << "[Generator] Target: "
              << generated.target.name
              << " | NORAD: " << generated.target.norad_id
              << " | observations: " << observations.size()
              << std::endl;

    MeasurementSigma sigma{
        0.1, 0.1,
        0.0, 0.0
    };

    /// --- 4. ПОДГОТОВКА ДАННЫХ ДЛЯ ЯДРА ---
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

    /// --- 5. ЯДРО ---
    auto results = core.identifySatellites(prepared, 10);

    /// --- 6. ВЫВОД РЕЗУЛЬТАТОВ ---
    output.print(results);

    if (!results.empty()) {
        const auto& best = results.front();
        std::cout << "[Decision] "
                  << (best.catalog_match
                      ? "H_in accepted: best catalog candidate is compatible"
                      : "observed object is likely outside selected catalog")
                  << " | alpha: 0.001"
                  << " | best fit p-value: " << best.fit_p_value
                  << "\n";
    }

    bool targetFound = false;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].norad_id == generated.target.norad_id) {
            std::cout << "[Check] target rank: " << (i + 1)
                      << " | chi^2: " << results[i].chi_squared
                      << " | reduced chi^2: "
                      << results[i].reduced_chi_squared
                      << " | probability: " << results[i].probability
                      << "%"
                      << " | fit p-value: " << results[i].fit_p_value
                      << "\n";
            targetFound = true;
            break;
        }
    }

    if (!targetFound) {
        std::cout << "[Check] target is outside printed top results\n";
    }

    if (console.promptRunExperiments()) {
        try {
            experiments.runDefaultSuite(prepared, generated.target);
        }
        catch (const std::exception& e) {
            std::cout << "[Experiments ERROR] " << e.what() << std::endl;
            return 1;
        }
    }

    std::cout << "Finished" << std::endl;

    return 0;
}
