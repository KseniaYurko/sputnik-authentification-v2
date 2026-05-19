#pragma once

#include "Modules/data/CelestrakRegistry/CelestrakRegistry.h"
#include "Modules/data/DataModule/DataModule.h"

#include <string>
#include <vector>

/**
 * @brief Консольный интерфейс приложения
 *
 * Содержит prompts, алиасы каталогов, служебный вывод и проверку локального
 * кэша. Алгоритмический pipeline остается в main.cpp.
 */
class ConsoleInterface {
public:
    void printBanner() const;

    bool promptUpdateCatalogs() const;

    bool promptRunExperiments() const;

    std::vector<CelestrakGroup> promptCatalogSelection(
        const std::string& prompt,
        const std::string& defaultName = "Space Stations"
    ) const;

    void printSelectedGroups(const std::vector<CelestrakGroup>& groups) const;

    void ensureLocalCatalogs(
        DataModule& data,
        const std::vector<CelestrakGroup>& groups
    ) const;

private:
    std::string expandCatalogAlias(const std::string& input) const;
};
