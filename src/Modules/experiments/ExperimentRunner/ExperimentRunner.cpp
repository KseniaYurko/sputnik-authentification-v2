#include "ExperimentRunner.h"

#include "ProjectConfig.h"
#include "Modules/core/sgp4/CoordTopocentric.h"
#include "Modules/core/sgp4/DateTime.h"
#include "Modules/core/sgp4/Observer.h"
#include "Modules/core/sgp4/SGP4.h"
#include "Modules/core/sgp4/Tle.h"
#include "Modules/core/sgp4/Util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr double EXPERIMENT_TIME_BUDGET_SECONDS = 480.0;
constexpr size_t MAX_NOISE_CATALOG_SIZE = 250;
constexpr size_t EXPERIMENT_OBSERVATION_LIMIT = 10;
constexpr int NOISE_TRIALS = 5;
constexpr int RUNTIME_REPEATS = 1;
constexpr size_t TARGET_SAMPLE_COUNT = 5;
constexpr size_t ORBIT_TYPE_TARGETS_PER_CLASS = 3;
constexpr size_t ORBIT_TYPE_CATALOG_CAP = 500;
constexpr double ORBIT_TYPE_SCAN_WINDOW_HOURS = 72.0;
constexpr double ORBIT_TYPE_SCAN_STEP_SECONDS = 120.0;
constexpr double ORBIT_TYPE_SAMPLE_STEP_SECONDS = 2.0;
constexpr double ORBIT_TYPE_MIN_ELEVATION_DEG = 3.0;

const std::vector<double>& stressNoiseSigmas() {
    static const std::vector<double> sigmas = [] {
        std::vector<double> values;
        values.reserve(201);
        for (int i = 0; i <= 200; ++i) {
            values.push_back(static_cast<double>(i) / 100.0);
        }
        return values;
    }();
    return sigmas;
}

void limitObservations(PreparedData& data);

enum class OrbitType {
    Leo,
    Meo,
    Geo,
    Polar,
    Elliptical,
    Other
};

std::string orbitTypeName(OrbitType type) {
    switch (type) {
        case OrbitType::Leo: return "LEO";
        case OrbitType::Meo: return "MEO";
        case OrbitType::Geo: return "GEO";
        case OrbitType::Polar: return "Polar";
        case OrbitType::Elliptical: return "Elliptical";
        case OrbitType::Other: return "Other";
    }

    return "Other";
}

std::filesystem::path experimentsRoot() {
    return std::filesystem::path(kProjectRootPath) / "experiments";
}

std::filesystem::path resultsRoot() {
    return experimentsRoot() / "results";
}

std::string pathForWrite(const std::filesystem::path& path) {
    return path.generic_string();
}

std::filesystem::path plotScriptPath() {
    return experimentsRoot() / "plots" / "plot_experiments.py";
}

void ensureResultsDir() {
    std::filesystem::create_directories(resultsRoot());
}

double medianObservationStepSeconds(const std::vector<Observation>& observations) {
    if (observations.size() < 2) {
        return 1.0;
    }

    std::vector<double> steps;
    steps.reserve(observations.size() - 1);

    for (size_t i = 1; i < observations.size(); ++i) {
        const double newer = observations[i].time_jd + observations[i].time_jdFr;
        const double older = observations[i - 1].time_jd + observations[i - 1].time_jdFr;
        const double seconds = (newer - older) * 86400.0;
        if (seconds > 0.0) {
            steps.push_back(seconds);
        }
    }

    if (steps.empty()) {
        return 1.0;
    }

    const auto middle = steps.begin() + static_cast<std::ptrdiff_t>(steps.size() / 2);
    std::nth_element(steps.begin(), middle, steps.end());
    return *middle;
}

PreparedData withNoisyObservations(
    const PreparedData& source,
    double sigmaDeg,
    std::mt19937& rng
) {
    PreparedData noisy = source;

    std::normal_distribution<double> noise(0.0, sigmaDeg);

    for (auto& obs : noisy.observations) {
        obs.azimuth += noise(rng);
        obs.elevation += noise(rng);
    }

    const double effectiveSigma = std::max(sigmaDeg, 0.001);
    const double medianDtSeconds = medianObservationStepSeconds(noisy.observations);
    const double effectiveRateSigma =
        std::sqrt(2.0) * effectiveSigma / medianDtSeconds;

    noisy.sigma.sigmaAz_deg = effectiveSigma;
    noisy.sigma.sigmaEl_deg = effectiveSigma;
    noisy.sigma.sigmaAzRate_degps = effectiveRateSigma;
    noisy.sigma.sigmaElRate_degps = effectiveRateSigma;

    return noisy;
}

libsgp4::DateTime dateTimeFromJulian(double jd) {
    constexpr double julianOffset = 1721425.5;
    constexpr double ticksPerDay = 86400000000.0;

    const auto ticks = static_cast<int64_t>(
        std::llround((jd - julianOffset) * ticksPerDay)
    );

    return libsgp4::DateTime(ticks);
}

Observation predictObservation(
    const SatelliteCatalogEntry& target,
    const Observer& observer,
    const Observation& timeSource
) {
    const double jd = timeSource.time_jd + timeSource.time_jdFr;
    const auto time = dateTimeFromJulian(jd);
    const libsgp4::Tle tle(target.name, target.tle_line1, target.tle_line2);
    const libsgp4::SGP4 propagator(tle);
    libsgp4::Observer localObserver(
        observer.lat_deg,
        observer.lon_deg,
        observer.alt_km
    );

    const double minutes = (time - tle.Epoch()).TotalMinutes();
    const libsgp4::Eci eci = propagator.FindPosition(minutes);
    const libsgp4::CoordTopocentric look = localObserver.GetLookAngle(eci);

    Observation obs = timeSource;
    obs.azimuth = libsgp4::Util::RadiansToDegrees(look.azimuth);
    obs.elevation = libsgp4::Util::RadiansToDegrees(look.elevation);
    return obs;
}

PreparedData preparedForSyntheticTarget(
    const PreparedData& source,
    const SatelliteCatalogEntry& target
) {
    PreparedData result = source;
    limitObservations(result);

    for (auto& obs : result.observations) {
        obs = predictObservation(target, result.observer, obs);
    }

    return result;
}

OrbitType classifyOrbit(const libsgp4::Tle& tle) {
    const double meanMotion = tle.MeanMotion();
    const double inclination = tle.Inclination(true);
    const double eccentricity = tle.Eccentricity();

    if (eccentricity >= 0.10) {
        return OrbitType::Elliptical;
    }

    if (meanMotion >= 0.90 && meanMotion <= 1.10) {
        return OrbitType::Geo;
    }

    if (inclination >= 80.0 && inclination <= 110.0 && meanMotion >= 10.0) {
        return OrbitType::Polar;
    }

    if (meanMotion >= 11.0) {
        return OrbitType::Leo;
    }

    if (meanMotion >= 1.8 && meanMotion <= 5.5) {
        return OrbitType::Meo;
    }

    return OrbitType::Other;
}

bool buildVisibleSyntheticPrepared(
    const PreparedData& source,
    const SatelliteCatalogEntry& target,
    PreparedData& output
) {
    const libsgp4::Tle tle(target.name, target.tle_line1, target.tle_line2);
    const libsgp4::SGP4 propagator(tle);
    const libsgp4::DateTime epoch = tle.Epoch();
    const int count = static_cast<int>(
        std::min<size_t>(EXPERIMENT_OBSERVATION_LIMIT, source.observations.size())
    );

    if (count < 2) {
        return false;
    }

    const int scanSteps = static_cast<int>(
        ORBIT_TYPE_SCAN_WINDOW_HOURS * 3600.0 / ORBIT_TYPE_SCAN_STEP_SECONDS
    );

    for (int scan = 0; scan <= scanSteps; ++scan) {
        PreparedData candidate = source;
        candidate.catalog = source.catalog;
        candidate.observations.clear();
        candidate.observations.reserve(static_cast<size_t>(count));

        bool visible = true;
        for (int i = 0; i < count; ++i) {
            const libsgp4::DateTime dt = epoch.AddSeconds(
                ORBIT_TYPE_SCAN_STEP_SECONDS * static_cast<double>(scan) +
                ORBIT_TYPE_SAMPLE_STEP_SECONDS * static_cast<double>(i)
            );

            libsgp4::Observer localObserver(
                source.observer.lat_deg,
                source.observer.lon_deg,
                source.observer.alt_km
            );

            const double minutes = (dt - epoch).TotalMinutes();
            const libsgp4::Eci eci = propagator.FindPosition(minutes);
            const libsgp4::CoordTopocentric look =
                localObserver.GetLookAngle(eci);

            const double elevationDeg =
                libsgp4::Util::RadiansToDegrees(look.elevation);
            if (elevationDeg <= ORBIT_TYPE_MIN_ELEVATION_DEG) {
                visible = false;
                break;
            }

            const double jd = dt.ToJulian();
            const double jdWhole = std::floor(jd);

            Observation obs;
            obs.azimuth = libsgp4::Util::RadiansToDegrees(look.azimuth);
            obs.elevation = elevationDeg;
            obs.time_jd = jdWhole;
            obs.time_jdFr = jd - jdWhole;
            candidate.observations.push_back(obs);
        }

        if (visible && candidate.observations.size() == static_cast<size_t>(count)) {
            output = candidate;
            return true;
        }
    }

    return false;
}

struct TargetStats {
    int rank = -1;
    double targetChi2 = std::numeric_limits<double>::quiet_NaN();
    double bestChi2 = std::numeric_limits<double>::quiet_NaN();
    double bestGapChi2 = std::numeric_limits<double>::quiet_NaN();
    int top1Norad = 0;
    bool top1Correct = false;
    bool top3ContainsTarget = false;
    bool top10ContainsTarget = false;
    bool equivalentBest = false;
    int ambiguousBestCount = 0;
};

TargetStats inspectTarget(
    const std::vector<MatchResult>& results,
    const SatelliteCatalogEntry& target
) {
    TargetStats stats;

    if (results.empty()) {
        return stats;
    }

    stats.bestChi2 = results.front().chi_squared;
    stats.top1Norad = results.front().norad_id;
    stats.top1Correct = stats.top1Norad == target.norad_id;

    constexpr double equivalentEps = 1e-9;
    stats.ambiguousBestCount = 0;
    for (const auto& r : results) {
        if (std::abs(r.chi_squared - stats.bestChi2) <= equivalentEps) {
            ++stats.ambiguousBestCount;
        }
        else {
            break;
        }
    }

    if (results.size() >= 2) {
        stats.bestGapChi2 = results[1].chi_squared - results[0].chi_squared;
    }

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].norad_id == target.norad_id) {
            stats.rank = static_cast<int>(i + 1);
            stats.targetChi2 = results[i].chi_squared;
            stats.equivalentBest =
                std::abs(stats.targetChi2 - stats.bestChi2) <= equivalentEps;
            stats.top3ContainsTarget = stats.rank <= 3;
            stats.top10ContainsTarget = stats.rank <= 10;
            return stats;
        }
    }

    return stats;
}

std::vector<size_t> benchmarkSizes(size_t total) {
    const std::vector<size_t> requested = {
        10, 25, 50, 100, total
    };

    std::set<size_t> unique;
    for (size_t n : requested) {
        if (n > 0 && n <= total) {
            unique.insert(n);
        }
    }

    return {unique.begin(), unique.end()};
}

std::vector<SatelliteCatalogEntry> cappedCatalogWithTarget(
    const std::vector<SatelliteCatalogEntry>& catalog,
    const SatelliteCatalogEntry& target,
    size_t cap
) {
    if (catalog.size() <= cap) {
        return catalog;
    }

    std::vector<SatelliteCatalogEntry> result;
    result.reserve(cap);

    bool hasTarget = false;
    for (const auto& sat : catalog) {
        if (sat.norad_id == target.norad_id) {
            hasTarget = true;
            break;
        }
    }

    if (hasTarget) {
        result.push_back(target);
    }

    for (const auto& sat : catalog) {
        if (result.size() >= cap) {
            break;
        }

        if (sat.norad_id == target.norad_id) {
            continue;
        }

        result.push_back(sat);
    }

    return result;
}

std::unordered_map<int, SatelliteCatalogEntry> indexByNorad(
    const std::vector<SatelliteCatalogEntry>& catalog
) {
    std::unordered_map<int, SatelliteCatalogEntry> index;
    index.reserve(catalog.size());
    for (const auto& sat : catalog) {
        index.emplace(sat.norad_id, sat);
    }
    return index;
}

std::vector<SatelliteCatalogEntry> catalogFromRankedResults(
    const std::vector<SatelliteCatalogEntry>& fullCatalog,
    const std::vector<MatchResult>& ranked,
    const SatelliteCatalogEntry& target,
    size_t cap,
    bool includeTarget
) {
    const auto byNorad = indexByNorad(fullCatalog);
    std::vector<SatelliteCatalogEntry> result;
    result.reserve(cap);

    if (includeTarget) {
        result.push_back(target);
    }

    for (const auto& match : ranked) {
        if (result.size() >= cap) {
            break;
        }

        if (match.norad_id == target.norad_id) {
            continue;
        }

        const auto found = byNorad.find(match.norad_id);
        if (found != byNorad.end()) {
            result.push_back(found->second);
        }
    }

    return result;
}

std::vector<SatelliteCatalogEntry> hardNegativeCatalog(
    const PreparedData& targetData,
    const SatelliteCatalogEntry& target,
    size_t cap,
    bool includeTarget
) {
    CoreModule core;
    auto ranked = core.identifySatellites(targetData, static_cast<int>(cap + 10), false);
    return catalogFromRankedResults(
        targetData.catalog,
        ranked,
        target,
        cap,
        includeTarget
    );
}

std::vector<SatelliteCatalogEntry> withoutTarget(
    const std::vector<SatelliteCatalogEntry>& catalog,
    const SatelliteCatalogEntry& target
) {
    std::vector<SatelliteCatalogEntry> result;
    result.reserve(catalog.size());

    for (const auto& sat : catalog) {
        if (sat.norad_id != target.norad_id) {
            result.push_back(sat);
        }
    }

    return result;
}

void limitObservations(PreparedData& data) {
    if (data.observations.size() > EXPERIMENT_OBSERVATION_LIMIT) {
        data.observations.resize(EXPERIMENT_OBSERVATION_LIMIT);
    }
}

std::vector<SatelliteCatalogEntry> chooseExperimentTargets(
    const PreparedData& prepared,
    const SatelliteCatalogEntry& primaryTarget
) {
    std::vector<SatelliteCatalogEntry> targets;
    targets.push_back(primaryTarget);

    if (prepared.catalog.empty()) {
        return targets;
    }

    const size_t step = std::max<size_t>(
        1,
        prepared.catalog.size() / (TARGET_SAMPLE_COUNT * 3)
    );

    for (size_t i = 0;
         i < prepared.catalog.size() && targets.size() < TARGET_SAMPLE_COUNT;
         i += step) {
        const auto& candidate = prepared.catalog[i];

        if (candidate.norad_id == primaryTarget.norad_id) {
            continue;
        }

        try {
            (void)preparedForSyntheticTarget(prepared, candidate);
            targets.push_back(candidate);
        }
        catch (const std::exception&) {
            continue;
        }
    }

    return targets;
}

long long elapsedMs(
    const std::chrono::steady_clock::time_point& start,
    const std::chrono::steady_clock::time_point& end
) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    ).count();
}

long long elapsedUs(
    const std::chrono::steady_clock::time_point& start,
    const std::chrono::steady_clock::time_point& end
) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        end - start
    ).count();
}

double secondsSince(const std::chrono::steady_clock::time_point& start) {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start).count();
}

void printEstimatedWork(
    const char* label,
    size_t runs,
    size_t candidates,
    size_t observations
) {
    const unsigned long long sgp4Calls =
        static_cast<unsigned long long>(runs) *
        static_cast<unsigned long long>(candidates) *
        static_cast<unsigned long long>(observations);

    std::cout << "[Experiments][" << label << "] runs: " << runs
              << " | candidates/run: " << candidates
              << " | observations/run: " << observations
              << " | estimated SGP4 calls: " << sgp4Calls
              << std::endl;
}

void runPlotScript() {
    const auto script = plotScriptPath();

    if (!std::filesystem::exists(script)) {
        std::cout << "[Experiments] plot script not found: "
                  << script << std::endl;
        return;
    }

    std::cout << "[Experiments] building plots...\n";

    const std::string command =
        "python \"" + script.string() + "\"";

    const int code = std::system(command.c_str());

    if (code != 0) {
        std::cout << "[Experiments] plot generation failed"
                  << " | exit code: " << code << "\n";
        std::cout << "[Experiments] run manually: "
                  << command << std::endl;
    }
}

bool timeBudgetReached(
    const std::chrono::steady_clock::time_point& start,
    const char* label
) {
    const double elapsed = secondsSince(start);
    if (elapsed < EXPERIMENT_TIME_BUDGET_SECONDS) {
        return false;
    }

    std::cout << "[Experiments][" << label
              << "] time budget reached after "
              << elapsed << "s, stopping this experiment\n";
    return true;
}

} // namespace

void ExperimentRunner::runDefaultSuite(
    const PreparedData& prepared,
    const SatelliteCatalogEntry& target
) const {
    ensureResultsDir();

    std::cout << "[Experiments] output directory: "
              << resultsRoot() << std::endl;
    std::cout << "[Experiments] mode: fast diploma run"
              << " | total budget: "
              << EXPERIMENT_TIME_BUDGET_SECONDS << "s\n";

    const auto suiteStart = std::chrono::steady_clock::now();

    runNoiseRobustness(prepared, target, suiteStart);
    runCatalogPresenceDetection(prepared, target, suiteStart);
    runOrbitTypeRobustness(prepared, suiteStart);
    runRuntimeByCatalogSize(prepared, suiteStart);
    runPlotScript();

    std::cout << "[Experiments] done\n";
}

void ExperimentRunner::runNoiseRobustness(
    const PreparedData& prepared,
    const SatelliteCatalogEntry& target,
    const std::chrono::steady_clock::time_point& suiteStart
) const {
    const auto path = resultsRoot() / "noise_robustness.csv";
    std::ofstream out(pathForWrite(path));

    if (!out) {
        throw std::runtime_error("Cannot write noise robustness CSV");
    }

    out << "target_norad,catalog_mode,catalog_size,sigma_deg,trial,"
        << "observation_count,target_rank,target_chi2,best_chi2,"
        << "best_gap_chi2,top1_norad,top1_correct,"
        << "top3_contains_target,top10_contains_target,"
        << "equivalent_best,ambiguous_best_count\n";

    const auto& sigmas = stressNoiseSigmas();
    const int trials = NOISE_TRIALS;
    const auto targets = chooseExperimentTargets(prepared, target);
    const std::vector<std::string> catalogModes = {"prefix", "hard"};

    const size_t totalRuns =
        sigmas.size() *
        static_cast<size_t>(trials) *
        targets.size() *
        catalogModes.size();

    CoreModule core;
    std::mt19937 rng(42);

    std::cout << "[Experiments] noise robustness...\n";
    std::cout << "[Experiments][Noise] targets: "
              << targets.size()
              << " | catalog cap: " << MAX_NOISE_CATALOG_SIZE
              << " / " << prepared.catalog.size() << " satellites\n";

    size_t completedRuns = 0;

    for (const auto& experimentTarget : targets) {
        PreparedData targetData;
        try {
            targetData = preparedForSyntheticTarget(prepared, experimentTarget);
        }
        catch (const std::exception& e) {
            std::cout << "[Experiments][Noise] skip target NORAD "
                      << experimentTarget.norad_id
                      << " | " << e.what() << std::endl;
            continue;
        }

        std::map<std::string, std::vector<SatelliteCatalogEntry>> catalogs;
        catalogs["prefix"] = cappedCatalogWithTarget(
            prepared.catalog,
            experimentTarget,
            MAX_NOISE_CATALOG_SIZE
        );
        catalogs["hard"] = hardNegativeCatalog(
            targetData,
            experimentTarget,
            MAX_NOISE_CATALOG_SIZE,
            true
        );

        for (const auto& mode : catalogModes) {
            const auto catalogIt = catalogs.find(mode);
            if (catalogIt == catalogs.end() || catalogIt->second.empty()) {
                continue;
            }

            PreparedData experimentData = targetData;
            experimentData.catalog = catalogIt->second;

            printEstimatedWork(
                "Noise",
                sigmas.size() * static_cast<size_t>(trials),
                experimentData.catalog.size(),
                experimentData.observations.size()
            );

            for (double sigma : sigmas) {
                for (int trial = 1; trial <= trials; ++trial) {
                    if (timeBudgetReached(suiteStart, "Noise")) {
                        std::cout << "[Experiments] wrote partial: "
                                  << path << std::endl;
                        return;
                    }

                    const auto runStart = std::chrono::steady_clock::now();

                    PreparedData noisy = withNoisyObservations(
                        experimentData,
                        sigma,
                        rng
                    );
                    auto results = core.identifySatellites(noisy, -1, false);
                    TargetStats stats = inspectTarget(results, experimentTarget);

                    ++completedRuns;

                    out << experimentTarget.norad_id << ','
                        << mode << ','
                        << experimentData.catalog.size() << ','
                        << sigma << ','
                        << trial << ','
                        << experimentData.observations.size() << ','
                        << stats.rank << ','
                        << stats.targetChi2 << ','
                        << stats.bestChi2 << ','
                        << stats.bestGapChi2 << ','
                        << stats.top1Norad << ','
                        << (stats.top1Correct ? 1 : 0) << ','
                        << (stats.top3ContainsTarget ? 1 : 0) << ','
                        << (stats.top10ContainsTarget ? 1 : 0) << ','
                        << (stats.equivalentBest ? 1 : 0)
                        << ','
                        << stats.ambiguousBestCount
                        << '\n';
                    out.flush();

                    const double runSec = secondsSince(runStart);
                    const double elapsedSec = secondsSince(suiteStart);
                    const double avgSec =
                        elapsedSec / static_cast<double>(completedRuns);
                    const double etaSec = avgSec *
                        static_cast<double>(
                            completedRuns < totalRuns
                                ? totalRuns - completedRuns
                                : 0
                        );

                    std::cout << "[Experiments][Noise] target "
                              << experimentTarget.norad_id
                              << " | mode " << mode
                              << " | sigma " << sigma
                              << " | trial " << trial << "/" << trials
                              << " | run " << completedRuns << "/" << totalRuns
                              << " | run time " << runSec << "s"
                              << " | elapsed " << elapsedSec << "s"
                              << " | ETA " << etaSec << "s"
                              << std::endl;
                }

                std::cout << "[Experiments] noise sigma done: "
                          << sigma << " deg"
                          << " | target " << experimentTarget.norad_id
                          << " | mode " << mode << "\n";
            }
        }
    }

    std::cout << "[Experiments] wrote: " << path << std::endl;
}

void ExperimentRunner::runCatalogPresenceDetection(
    const PreparedData& prepared,
    const SatelliteCatalogEntry& target,
    const std::chrono::steady_clock::time_point& suiteStart
) const {
    const auto path = resultsRoot() / "catalog_presence_detection.csv";
    std::ofstream out(pathForWrite(path));

    if (!out) {
        throw std::runtime_error("Cannot write catalog presence CSV");
    }

    out << "scenario,target_norad,sigma_deg,trial,catalog_size,"
        << "observation_count,best_norad,best_chi2,best_fit_p_value,"
        << "best_catalog_match,target_rank,correct_decision\n";

    const auto& sigmas = stressNoiseSigmas();
    const auto targets = chooseExperimentTargets(prepared, target);
    const size_t totalRuns =
        sigmas.size() *
        static_cast<size_t>(NOISE_TRIALS) *
        targets.size() *
        3ULL;

    CoreModule core;
    std::mt19937 rng(4242);
    size_t completedRuns = 0;

    std::cout << "[Experiments] catalog presence detection...\n";
    std::cout << "[Experiments][CatalogPresence] targets: "
              << targets.size()
              << " | catalog cap: " << MAX_NOISE_CATALOG_SIZE
              << " / " << prepared.catalog.size() << " satellites\n";

    for (const auto& experimentTarget : targets) {
        PreparedData targetData;
        try {
            targetData = preparedForSyntheticTarget(prepared, experimentTarget);
        }
        catch (const std::exception& e) {
            std::cout << "[Experiments][CatalogPresence] skip target NORAD "
                      << experimentTarget.norad_id
                      << " | " << e.what() << std::endl;
            continue;
        }

        PreparedData knownBase = targetData;
        knownBase.catalog = hardNegativeCatalog(
            targetData,
            experimentTarget,
            MAX_NOISE_CATALOG_SIZE,
            true
        );

        PreparedData unknownRandomBase = targetData;
        unknownRandomBase.catalog = withoutTarget(
            cappedCatalogWithTarget(
                prepared.catalog,
                experimentTarget,
                MAX_NOISE_CATALOG_SIZE
            ),
            experimentTarget
        );

        PreparedData unknownHardBase = targetData;
        unknownHardBase.catalog = hardNegativeCatalog(
            targetData,
            experimentTarget,
            MAX_NOISE_CATALOG_SIZE,
            false
        );

        printEstimatedWork(
            "CatalogPresence",
            sigmas.size() * static_cast<size_t>(NOISE_TRIALS) * 3ULL,
            knownBase.catalog.size(),
            knownBase.observations.size()
        );

        for (double sigma : sigmas) {
            for (int trial = 1; trial <= NOISE_TRIALS; ++trial) {
                if (timeBudgetReached(suiteStart, "CatalogPresence")) {
                    std::cout << "[Experiments] wrote partial: "
                              << path << std::endl;
                    return;
                }

                std::vector<std::pair<std::string, PreparedData>> scenarios;
                scenarios.push_back({
                    "known_hard",
                    withNoisyObservations(knownBase, sigma, rng)
                });
                scenarios.push_back({
                    "unknown_random",
                    withNoisyObservations(unknownRandomBase, sigma, rng)
                });
                scenarios.push_back({
                    "unknown_hard",
                    withNoisyObservations(unknownHardBase, sigma, rng)
                });

                for (auto& scenario : scenarios) {
                    const bool targetInCatalog =
                        scenario.first == "known_hard";
                    auto results = core.identifySatellites(
                        scenario.second,
                        -1,
                        false
                    );
                    TargetStats stats = inspectTarget(results, experimentTarget);

                    const bool bestCatalogMatch =
                        !results.empty() && results.front().catalog_match;
                    const bool correctDecision = targetInCatalog
                        ? bestCatalogMatch
                        : !bestCatalogMatch;

                    ++completedRuns;

                    out << scenario.first << ','
                        << experimentTarget.norad_id << ','
                        << sigma << ','
                        << trial << ','
                        << scenario.second.catalog.size() << ','
                        << scenario.second.observations.size() << ','
                        << (results.empty() ? 0 : results.front().norad_id) << ','
                        << (results.empty()
                            ? std::numeric_limits<double>::quiet_NaN()
                            : results.front().chi_squared) << ','
                        << (results.empty()
                            ? std::numeric_limits<double>::quiet_NaN()
                            : results.front().fit_p_value) << ','
                        << (bestCatalogMatch ? 1 : 0) << ','
                        << stats.rank << ','
                        << (correctDecision ? 1 : 0)
                        << '\n';
                    out.flush();

                    std::cout << "[Experiments][CatalogPresence] "
                              << scenario.first
                              << " | target " << experimentTarget.norad_id
                              << " | sigma " << sigma
                              << " | trial " << trial << "/" << NOISE_TRIALS
                              << " | run " << completedRuns << "/" << totalRuns
                              << " | decision "
                              << (bestCatalogMatch ? "catalog" : "unknown")
                              << " | correct " << (correctDecision ? 1 : 0)
                              << std::endl;
                }
            }

            std::cout << "[Experiments] catalog presence sigma done: "
                      << sigma << " deg"
                      << " | target " << experimentTarget.norad_id << "\n";
        }
    }

    std::cout << "[Experiments] wrote: " << path << std::endl;
}

void ExperimentRunner::runOrbitTypeRobustness(
    const PreparedData& prepared,
    const std::chrono::steady_clock::time_point& suiteStart
) const {
    const auto path = resultsRoot() / "orbit_type_robustness.csv";
    std::ofstream out(pathForWrite(path));

    if (!out) {
        throw std::runtime_error("Cannot write orbit type robustness CSV");
    }

    out << "orbit_type,target_norad,target_name,mean_motion_rev_day,"
        << "inclination_deg,eccentricity,catalog_size,sigma_deg,trial,"
        << "observation_count,target_rank,target_chi2,best_chi2,"
        << "best_fit_p_value,best_gap_chi2,top1_norad,top1_correct,"
        << "top3_contains_target,top10_contains_target,runtime_us,"
        << "runtime_ms\n";

    std::map<OrbitType, std::vector<SatelliteCatalogEntry>> byType;

    for (const auto& sat : prepared.catalog) {
        try {
            libsgp4::Tle tle(sat.name, sat.tle_line1, sat.tle_line2);
            const OrbitType type = classifyOrbit(tle);
            if (type == OrbitType::Other) {
                continue;
            }

            byType[type].push_back(sat);
        }
        catch (const std::exception&) {
            continue;
        }
    }

    const std::vector<OrbitType> typeOrder = {
        OrbitType::Leo,
        OrbitType::Meo,
        OrbitType::Geo,
        OrbitType::Polar,
        OrbitType::Elliptical
    };

    const auto& sigmas = stressNoiseSigmas();
    std::mt19937 rng(98765);
    CoreModule core;

    size_t plannedTargets = 0;
    for (const auto type : typeOrder) {
        plannedTargets += std::min(
            ORBIT_TYPE_TARGETS_PER_CLASS,
            byType[type].size()
        );
    }

    const size_t totalRuns =
        plannedTargets * sigmas.size() * static_cast<size_t>(NOISE_TRIALS);

    std::cout << "[Experiments] orbit type robustness...\n";
    std::cout << "[Experiments][OrbitType] planned targets: "
              << plannedTargets
              << " | catalog cap: " << ORBIT_TYPE_CATALOG_CAP
              << " | runs: " << totalRuns << std::endl;

    size_t completedRuns = 0;

    for (const auto type : typeOrder) {
        auto candidates = byType[type];
        if (candidates.empty()) {
            std::cout << "[Experiments][OrbitType] no candidates for "
                      << orbitTypeName(type) << std::endl;
            continue;
        }

        size_t selectedForType = 0;
        for (const auto& target : candidates) {
            if (selectedForType >= ORBIT_TYPE_TARGETS_PER_CLASS) {
                break;
            }

            if (timeBudgetReached(suiteStart, "OrbitType")) {
                std::cout << "[Experiments] wrote partial: "
                          << path << std::endl;
                return;
            }

            PreparedData targetData;
            try {
                if (!buildVisibleSyntheticPrepared(prepared, target, targetData)) {
                    continue;
                }
            }
            catch (const std::exception& e) {
                std::cout << "[Experiments][OrbitType] skip target NORAD "
                          << target.norad_id
                          << " | " << e.what() << std::endl;
                continue;
            }

            targetData.catalog = hardNegativeCatalog(
                targetData,
                target,
                ORBIT_TYPE_CATALOG_CAP,
                true
            );

            if (targetData.catalog.empty()) {
                continue;
            }

            const libsgp4::Tle targetTle(
                target.name,
                target.tle_line1,
                target.tle_line2
            );

            ++selectedForType;

            printEstimatedWork(
                "OrbitType",
                sigmas.size() * static_cast<size_t>(NOISE_TRIALS),
                targetData.catalog.size(),
                targetData.observations.size()
            );

            for (double sigma : sigmas) {
                for (int trial = 1; trial <= NOISE_TRIALS; ++trial) {
                    if (timeBudgetReached(suiteStart, "OrbitType")) {
                        std::cout << "[Experiments] wrote partial: "
                                  << path << std::endl;
                        return;
                    }

                    PreparedData noisy = withNoisyObservations(
                        targetData,
                        sigma,
                        rng
                    );

                    const auto runStart = std::chrono::steady_clock::now();
                    auto results = core.identifySatellites(noisy, -1, false);
                    const auto runEnd = std::chrono::steady_clock::now();
                    const long long us = elapsedUs(runStart, runEnd);

                    TargetStats stats = inspectTarget(results, target);
                    const double bestPValue = results.empty()
                        ? std::numeric_limits<double>::quiet_NaN()
                        : results.front().fit_p_value;

                    ++completedRuns;

                    out << orbitTypeName(type) << ','
                        << target.norad_id << ','
                        << '"' << target.name << '"' << ','
                        << targetTle.MeanMotion() << ','
                        << targetTle.Inclination(true) << ','
                        << targetTle.Eccentricity() << ','
                        << noisy.catalog.size() << ','
                        << sigma << ','
                        << trial << ','
                        << noisy.observations.size() << ','
                        << stats.rank << ','
                        << stats.targetChi2 << ','
                        << stats.bestChi2 << ','
                        << bestPValue << ','
                        << stats.bestGapChi2 << ','
                        << stats.top1Norad << ','
                        << (stats.top1Correct ? 1 : 0) << ','
                        << (stats.top3ContainsTarget ? 1 : 0) << ','
                        << (stats.top10ContainsTarget ? 1 : 0) << ','
                        << us << ','
                        << (static_cast<double>(us) / 1000.0)
                        << '\n';
                    out.flush();

                    std::cout << "[Experiments][OrbitType] "
                              << orbitTypeName(type)
                              << " | target " << target.norad_id
                              << " | sigma " << sigma
                              << " | trial " << trial << "/" << NOISE_TRIALS
                              << " | run " << completedRuns << "/" << totalRuns
                              << " | rank " << stats.rank
                              << " | top1 " << (stats.top1Correct ? 1 : 0)
                              << " | runtime "
                              << (static_cast<double>(us) / 1000.0)
                              << "ms"
                              << std::endl;
                }
            }
        }

        std::cout << "[Experiments][OrbitType] selected "
                  << selectedForType << " targets for "
                  << orbitTypeName(type) << std::endl;
    }

    std::cout << "[Experiments] wrote: " << path << std::endl;
}

void ExperimentRunner::runRuntimeByCatalogSize(
    const PreparedData& prepared,
    const std::chrono::steady_clock::time_point& suiteStart
) const {
    const auto path = resultsRoot() / "runtime_catalog_size.csv";
    std::ofstream out(pathForWrite(path));

    if (!out) {
        throw std::runtime_error("Cannot write runtime CSV");
    }

    out << "catalog_size,repeat,observations,runtime_us,runtime_ms\n";

    CoreModule core;

    const auto sizes = benchmarkSizes(prepared.catalog.size());
    const int repeats = RUNTIME_REPEATS;
    size_t totalRuns = sizes.size() * repeats;

    std::cout << "[Experiments] runtime by catalog size...\n";
    std::cout << "[Experiments][Runtime] sizes: " << sizes.size()
              << " | repeats: " << repeats << std::endl;

    size_t completedRuns = 0;

    for (size_t size : sizes) {
        PreparedData slice = prepared;
        limitObservations(slice);
        slice.catalog.assign(
            prepared.catalog.begin(),
            prepared.catalog.begin() + static_cast<std::ptrdiff_t>(size)
        );

        if (timeBudgetReached(suiteStart, "Runtime")) {
            std::cout << "[Experiments] wrote partial: "
                      << path << std::endl;
            return;
        }

        printEstimatedWork(
            "Runtime",
            repeats,
            size,
            slice.observations.size()
        );

        for (int repeat = 1; repeat <= repeats; ++repeat) {
            if (timeBudgetReached(suiteStart, "Runtime")) {
                std::cout << "[Experiments] wrote partial: "
                          << path << std::endl;
                return;
            }

            const auto start = std::chrono::steady_clock::now();
            auto results = core.identifySatellites(slice, 10, false);
            const auto end = std::chrono::steady_clock::now();

            (void)results;
            const long long us = elapsedUs(start, end);
            ++completedRuns;

            out << size << ','
                << repeat << ','
                << slice.observations.size() << ','
                << us << ','
                << (static_cast<double>(us) / 1000.0)
                << '\n';
            out.flush();

            const double elapsedSec = secondsSince(suiteStart);
            const double avgSec = elapsedSec / static_cast<double>(completedRuns);
            const double etaSec =
                avgSec * static_cast<double>(totalRuns - completedRuns);

            std::cout << "[Experiments][Runtime] size " << size
                      << " | repeat " << repeat << "/" << repeats
                      << " | run " << completedRuns << "/" << totalRuns
                      << " | runtime " << (static_cast<double>(us) / 1000.0)
                      << "ms"
                      << " | elapsed " << elapsedSec << "s"
                      << " | ETA " << etaSec << "s"
                      << std::endl;
        }

        std::cout << "[Experiments] runtime size done: "
                  << size << " satellites\n";
    }

    std::cout << "[Experiments] wrote: " << path << std::endl;
}
