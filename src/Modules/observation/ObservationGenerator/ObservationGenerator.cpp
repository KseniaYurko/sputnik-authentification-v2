#include "ObservationGenerator.h"

#include "Modules/core/sgp4/CoordTopocentric.h"
#include "Modules/core/sgp4/DateTime.h"
#include "Modules/core/sgp4/Observer.h"
#include "Modules/core/sgp4/SGP4.h"
#include "Modules/core/sgp4/Tle.h"
#include "Modules/core/sgp4/Util.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr double MOSCOW_LAT_DEG = 55.7558;
constexpr double MOSCOW_LON_DEG = 37.6173;
constexpr double MOSCOW_ALT_KM = 0.2;

constexpr double SAMPLE_STEP_SECONDS = 2.0;
constexpr double SCAN_STEP_SECONDS = 30.0;
constexpr double SCAN_WINDOW_HOURS = 48.0;
constexpr double MIN_ELEVATION_DEG = 5.0;

size_t progressStep(size_t total) {
    if (total <= 100) return total + 1;
    return std::max<size_t>(100, total / 20);
}

void addDiagnosticSample(
    std::vector<std::string>& samples,
    const SatelliteCatalogEntry& sat,
    const std::string& reason
) {
    constexpr size_t maxSamples = 5;
    if (samples.size() >= maxSamples) {
        return;
    }

    std::ostringstream out;
    out << sat.name << " | NORAD: " << sat.norad_id
        << " | " << reason;
    samples.push_back(out.str());
}

Observation makeObservation(
    const libsgp4::SGP4& propagator,
    const libsgp4::DateTime& epoch,
    const libsgp4::DateTime& dt,
    libsgp4::Observer& observer
) {
    const double minutes = (dt - epoch).TotalMinutes();
    libsgp4::Eci eci = propagator.FindPosition(minutes);
    libsgp4::CoordTopocentric look = observer.GetLookAngle(eci);

    const double jd = dt.ToJulian();
    const double jdWhole = std::floor(jd);

    Observation obs;
    obs.azimuth = libsgp4::Util::RadiansToDegrees(look.azimuth);
    obs.elevation = libsgp4::Util::RadiansToDegrees(look.elevation);
    obs.time_jd = jdWhole;
    obs.time_jdFr = jd - jdWhole;

    return obs;
}

bool tryBuildVisibleSeries(
    const libsgp4::SGP4& propagator,
    const libsgp4::DateTime& epoch,
    const libsgp4::DateTime& start,
    int count,
    std::vector<Observation>& observations
) {
    libsgp4::Observer observer(
        MOSCOW_LAT_DEG,
        MOSCOW_LON_DEG,
        MOSCOW_ALT_KM
    );

    std::vector<Observation> candidate;
    candidate.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        libsgp4::DateTime dt =
            start.AddSeconds(SAMPLE_STEP_SECONDS * static_cast<double>(i));

        Observation obs = makeObservation(propagator, epoch, dt, observer);

        if (obs.elevation <= MIN_ELEVATION_DEG) {
            return false;
        }

        candidate.push_back(obs);
    }

    observations = candidate;
    return true;
}

} // namespace

GeneratedObservationSet ObservationGenerator::generateIdealMoscowPass(
    const std::vector<SatelliteCatalogEntry>& catalog,
    int count
) const {
    if (catalog.empty()) {
        throw std::runtime_error("Cannot generate observations: empty catalog");
    }

    if (count < 2) {
        throw std::runtime_error("Cannot generate observations: count must be >= 2");
    }

    const int scanSteps = static_cast<int>(
        SCAN_WINDOW_HOURS * 3600.0 / SCAN_STEP_SECONDS
    );

    const size_t total = catalog.size();
    const size_t step = progressStep(total);

    std::cout << "[Generator] searching visible Moscow pass...\n";
    std::cout << "[Generator] catalog size: " << total
              << " | requested observations: " << count << "\n";

    size_t scanned = 0;
    int skippedByException = 0;
    std::vector<std::string> errorSamples;

    for (const auto& sat : catalog) {
        ++scanned;

        if (scanned % step == 0 || scanned == total) {
            std::cout << "[Generator] scanned: " << scanned
                      << " / " << total << std::endl;
        }

        try {
            libsgp4::Tle tle(sat.name, sat.tle_line1, sat.tle_line2);
            libsgp4::SGP4 propagator(tle);
            const libsgp4::DateTime epoch = tle.Epoch();

            for (int step = 0; step <= scanSteps; ++step) {
                libsgp4::DateTime start =
                    epoch.AddSeconds(SCAN_STEP_SECONDS * static_cast<double>(step));

                std::vector<Observation> observations;
                if (tryBuildVisibleSeries(
                        propagator,
                        epoch,
                        start,
                        count,
                        observations
                    )) {
                    GeneratedObservationSet result;
                    result.target = sat;
                    result.observer = {
                        MOSCOW_LAT_DEG,
                        MOSCOW_LON_DEG,
                        MOSCOW_ALT_KM
                    };
                    result.observations = observations;

                    std::cout << "[Generator] found target: "
                              << sat.name
                              << " | NORAD: " << sat.norad_id
                              << std::endl;

                    return result;
                }
            }
        }
        catch (const std::exception& e) {
            ++skippedByException;
            addDiagnosticSample(errorSamples, sat, e.what());
            continue;
        }
    }

    std::cout << "[Generator] skipped by TLE/SGP4 errors: "
              << skippedByException << std::endl;
    for (const auto& sample : errorSamples) {
        std::cout << "[Generator][error sample] "
                  << sample << std::endl;
    }

    throw std::runtime_error(
        "Cannot generate observations: no visible Moscow pass found"
    );
}
