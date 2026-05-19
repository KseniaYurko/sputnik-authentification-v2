#include "ConsoleInterface.h"

#include <cctype>
#include <filesystem>
#include <iostream>

namespace {

std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;

    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;

    return s.substr(b, e - b);
}

std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

void ConsoleInterface::printBanner() const {
    std::cout << "SputnikAuthV2\n";
    std::cout << "Type 'list' at catalog prompt to show available catalogs\n\n";
}

bool ConsoleInterface::promptUpdateCatalogs() const {
    std::cout << "Update catalogs? [y/N]: ";

    std::string input;
    std::getline(std::cin, input);
    std::cout << "\n";

    const std::string value = lower(trim(input));
    return value == "y" || value == "yes" || value == "1";
}

bool ConsoleInterface::promptRunExperiments() const {
    std::cout << "Run diploma experiments and write CSV? [y/N]: ";

    std::string input;
    std::getline(std::cin, input);
    std::cout << "\n";

    const std::string value = lower(trim(input));
    return value == "y" || value == "yes" || value == "1";
}

std::vector<CelestrakGroup> ConsoleInterface::promptCatalogSelection(
    const std::string& prompt,
    const std::string& defaultName
) const {
    while (true) {
        std::cout << prompt << " [" << defaultName << "]: ";

        std::string input;
        std::getline(std::cin, input);
        std::cout << "\n";

        const std::string command = lower(trim(input));
        if (command == "help" || command == "list" || command == "?") {
            printCelestrakMenu();
            continue;
        }

        std::string resolvedInput = input.empty()
            ? defaultName
            : expandCatalogAlias(input);

        auto groups = resolveUserSelection(resolvedInput);

        if (!groups.empty()) {
            return groups;
        }

        std::cout << "[Input] Unknown catalog: " << input << "\n";
        std::cout << "[Input] Type 'list' to show available catalogs\n";
    }
}

void ConsoleInterface::printSelectedGroups(
    const std::vector<CelestrakGroup>& groups
) const {
    std::cout << "[Data] selected groups: " << groups.size() << "\n";

    for (const auto& group : groups) {
        std::cout << "  - " << group.display_name
                  << " (" << group.group_slug << ")\n";
    }
}

void ConsoleInterface::ensureLocalCatalogs(
    DataModule& data,
    const std::vector<CelestrakGroup>& groups
) const {
    std::vector<CelestrakGroup> missing;

    for (const auto& group : groups) {
        if (!std::filesystem::exists(DataModule::getCatalogPath(group))) {
            missing.push_back(group);
        }
    }

    if (missing.empty()) {
        std::cout << "[Data] local cache: OK\n";
        return;
    }

    std::cout << "[Data] missing local catalogs: " << missing.size() << "\n";
    std::cout << "[Data] downloading missing catalogs...\n";

    data.updateSelectedGroups(missing);
}

std::string ConsoleInterface::expandCatalogAlias(
    const std::string& input
) const {
    const std::string value = lower(trim(input));

    if (value.empty()) return "Space Stations";
    if (value == "all") return "all";

    if (value == "station" || value == "stations" || value == "iss") {
        return "Space Stations";
    }

    if (value == "starlink") return "Starlink";
    if (value == "gps") return "GPS Operational";
    if (value == "gnss") return "GNSS";
    if (value == "nav" || value == "navigation") return "Navigation Satellites";
    if (value == "weather") return "Weather";
    if (value == "active") return "Active Satellites";

    return input;
}
