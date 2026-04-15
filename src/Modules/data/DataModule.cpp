#include "DataModule.h"
#include "CelestrakFetcher.h"
#include "TLEParser.h"

/**
 * ЗАГЛУШКА!!! ВСТАВИТЬ CatalogLoader
 */
std::vector<SatelliteCatalogEntry> DataModule::loadCatalog(const std::string& path) {
    return {};
}

std::vector<Observation> DataModule::loadObservations(const std::string& path) {
    return {};
}

std::vector<SatelliteCatalogEntry>
DataModule::loadCatalogFromCelestrak(const std::string& groupName) {

    std::string content = downloadCelestrakTLE(groupName);
    return parseTLE(content);
}