#include "CoreModule.h"
#include "Modules/core/sgp4/CoordTopocentric.h"
#include "Modules/core/sgp4/DateTime.h"
#include "Modules/core/sgp4/Observer.h"
#include "Modules/core/sgp4/SGP4.h"
#include "Modules/core/sgp4/Util.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

size_t progressStep(size_t total) {
    if (total <= 100) return total + 1;
    return std::max<size_t>(100, total / 20);
}

constexpr double kCatalogMatchPValueThreshold = 1e-3;
constexpr double kMinRateDtSeconds = 1.0e-6;

libsgp4::DateTime dateTimeFromJulian(double jd) {
    constexpr double julianOffset = 1721425.5;
    constexpr double ticksPerDay = 86400000000.0;

    const auto ticks = static_cast<int64_t>(
        std::llround((jd - julianOffset) * ticksPerDay)
    );

    return libsgp4::DateTime(ticks);
}

double regularizedGammaP(double a, double x) {
    constexpr int maxIterations = 100;
    constexpr double epsilon = 3.0e-14;

    if (x <= 0.0) {
        return 0.0;
    }

    double sum = 1.0 / a;
    double term = sum;
    double ap = a;

    for (int n = 1; n <= maxIterations; ++n) {
        ++ap;
        term *= x / ap;
        sum += term;

        if (std::abs(term) < std::abs(sum) * epsilon) {
            return sum * std::exp(-x + a * std::log(x) - std::lgamma(a));
        }
    }

    return sum * std::exp(-x + a * std::log(x) - std::lgamma(a));
}

double regularizedGammaQ(double a, double x) {
    constexpr int maxIterations = 100;
    constexpr double epsilon = 3.0e-14;
    constexpr double fpMin = 1.0e-300;

    if (x <= 0.0) {
        return 1.0;
    }

    if (x < a + 1.0) {
        return 1.0 - regularizedGammaP(a, x);
    }

    double b = x + 1.0 - a;
    double c = 1.0 / fpMin;
    double d = 1.0 / std::max(b, fpMin);
    double h = d;

    for (int i = 1; i <= maxIterations; ++i) {
        const double an = -static_cast<double>(i) *
            (static_cast<double>(i) - a);
        b += 2.0;
        d = an * d + b;
        if (std::abs(d) < fpMin) {
            d = fpMin;
        }

        c = b + an / c;
        if (std::abs(c) < fpMin) {
            c = fpMin;
        }

        d = 1.0 / d;
        const double delta = d * c;
        h *= delta;

        if (std::abs(delta - 1.0) < epsilon) {
            break;
        }
    }

    return std::exp(-x + a * std::log(x) - std::lgamma(a)) * h;
}

double chiSquareSurvival(double chiSquared, int degreesOfFreedom) {
    if (degreesOfFreedom <= 0) {
        return 0.0;
    }

    return regularizedGammaQ(
        0.5 * static_cast<double>(degreesOfFreedom),
        0.5 * chiSquared
    );
}

double angularRateDegPerSecond(double newerDeg, double olderDeg, double dtSeconds) {
    return libsgp4::Util::WrapNegPos180(newerDeg - olderDeg) / dtSeconds;
}

double elevationRateDegPerSecond(double newerDeg, double olderDeg, double dtSeconds) {
    return (newerDeg - olderDeg) / dtSeconds;
}

double effectiveRateSigma(
    double configuredSigmaRate,
    double angularSigmaDeg,
    double dtSeconds
) {
    if (configuredSigmaRate > 0.0) {
        return configuredSigmaRate;
    }

    return std::sqrt(2.0) * angularSigmaDeg / dtSeconds;
}

std::string cacheKey(const SatelliteCatalogEntry& sat) {
    return std::to_string(sat.norad_id) + "|" + sat.tle_line1 + "|" + sat.tle_line2;
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

bool hasValidTleShape(const SatelliteCatalogEntry& sat, std::string& reason) {
    const std::string& l1 = sat.tle_line1;
    const std::string& l2 = sat.tle_line2;

    if (l1.size() != 69 || l2.size() != 69 || l1.empty() || l2.empty() ||
        l1[0] != '1' || l2[0] != '2') {
        std::ostringstream out;
        out << "invalid TLE format"
            << " | l1=" << l1.size()
            << " | l2=" << l2.size();
        reason = out.str();
        return false;
    }

    return true;
}

double maxAbsDaysFromEpoch(
    const libsgp4::DateTime& epoch,
    const std::vector<libsgp4::DateTime>& observationTimes
) {
    double maxDays = 0.0;
    for (const auto& time : observationTimes) {
        const double days = std::abs((time - epoch).TotalDays());
        maxDays = std::max(maxDays, days);
    }
    return maxDays;
}

double estimatedHorizonReachDeg(const libsgp4::Tle& tle) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double earthRadiusKm = 6378.137;
    constexpr double earthMuKm3Sec2 = 398600.4418;
    constexpr double secondsPerDay = 86400.0;

    const double meanMotionRevPerDay = tle.MeanMotion();
    if (meanMotionRevPerDay <= 0.0) {
        return 0.0;
    }

    const double meanMotionRadPerSec =
        meanMotionRevPerDay * 2.0 * pi / secondsPerDay;
    const double semiMajorAxisKm = std::cbrt(
        earthMuKm3Sec2 / (meanMotionRadPerSec * meanMotionRadPerSec)
    );

    if (semiMajorAxisKm <= earthRadiusKm) {
        return 0.0;
    }

    return libsgp4::Util::RadiansToDegrees(
        std::acos(earthRadiusKm / semiMajorAxisKm)
    );
}

bool canBeVisibleFromLatitude(
    double inclinationDeg,
    const libsgp4::Tle& tle,
    double observerLatDeg
) {
    constexpr double safetyMarginDeg = 5.0;

    const double orbitMaxLatitudeDeg = inclinationDeg <= 90.0
        ? inclinationDeg
        : 180.0 - inclinationDeg;

    const double visibleLatitudeReachDeg =
        orbitMaxLatitudeDeg + estimatedHorizonReachDeg(tle) + safetyMarginDeg;

    return std::abs(observerLatDeg) <= visibleLatitudeReachDeg;
}

void normalizePosteriorProbabilities(std::vector<MatchResult>& results) {
    if (results.empty()) {
        return;
    }

    const auto best = std::min_element(results.begin(), results.end());
    const double minChi2 = best->chi_squared;
    double posteriorNorm = 0.0;

    for (auto& r : results) {
        // Uniform prior over valid catalog candidates; Gaussian constants
        // cancel because all candidates are evaluated on the same data.
        r.probability = std::exp(-0.5 * (r.chi_squared - minChi2));
        posteriorNorm += r.probability;
    }

    if (posteriorNorm > std::numeric_limits<double>::min()) {
        for (auto& r : results) {
            r.probability = 100.0 * r.probability / posteriorNorm;
        }
    }
}

void keepTopN(std::vector<MatchResult>& results, int topN) {
    if (topN <= 0 || results.size() <= static_cast<size_t>(topN)) {
        std::sort(results.begin(), results.end());
        return;
    }

    const auto nth = results.begin() + topN;
    std::nth_element(results.begin(), nth, results.end());
    results.resize(static_cast<size_t>(topN));
    std::sort(results.begin(), results.end());
}

} // namespace

CoreModule::CompiledSatellite::CompiledSatellite(
    const SatelliteCatalogEntry& source,
    libsgp4::Tle tle
)
    : source(source)
    , tle(std::move(tle))
    , epoch(this->tle.Epoch())
    , inclination_deg(this->tle.Inclination(true))
{
}

std::vector<MatchResult> CoreModule::identifySatellites(
    const PreparedData& data,
    int topN,
    bool verbose
) {
    std::vector<MatchResult> results;

    const auto& observations = data.observations;
    const auto& catalog = data.catalog;
    const auto& observer = data.observer;
    const auto& sigma = data.sigma;

    if (catalog.empty() || observations.empty()) {
        if (verbose) {
            std::cout << "[Core ERROR] empty catalog or observations\n";
        }
        return results;
    }

    if (observer.lat_deg < -90.0 || observer.lat_deg > 90.0 ||
        observer.lon_deg < -180.0 || observer.lon_deg > 180.0) {
        if (verbose) {
            std::cout << "[Core ERROR] invalid observer coordinates\n";
        }
        return results;
    }

    if (sigma.sigmaAz_deg <= 0.0 || sigma.sigmaEl_deg <= 0.0) {
        if (verbose) {
            std::cout << "[Core ERROR] invalid angular measurement sigma\n";
        }
        return results;
    }

    if (sigma.sigmaAzRate_degps < 0.0 || sigma.sigmaElRate_degps < 0.0) {
        if (verbose) {
            std::cout << "[Core ERROR] invalid angular-rate measurement sigma\n";
        }
        return results;
    }

    std::vector<libsgp4::DateTime> observationTimes;
    observationTimes.reserve(observations.size());

    std::vector<double> observedAzDeg;
    std::vector<double> observedElDeg;
    observedAzDeg.reserve(observations.size());
    observedElDeg.reserve(observations.size());

    for (const auto& obs : observations) {
        const double jd = obs.time_jd + obs.time_jdFr;
        observationTimes.push_back(dateTimeFromJulian(jd));
        observedAzDeg.push_back(obs.azimuth);
        observedElDeg.push_back(obs.elevation);
    }

    std::vector<double> intervalSeconds;
    std::vector<double> observedAzRateDegps;
    std::vector<double> observedElRateDegps;
    if (observationTimes.size() >= 2) {
        intervalSeconds.reserve(observationTimes.size() - 1);
        observedAzRateDegps.reserve(observationTimes.size() - 1);
        observedElRateDegps.reserve(observationTimes.size() - 1);

        for (size_t i = 1; i < observationTimes.size(); ++i) {
            const double dtSeconds =
                (observationTimes[i] - observationTimes[i - 1]).TotalSeconds();
            if (dtSeconds <= kMinRateDtSeconds) {
                if (verbose) {
                    std::cout << "[Core ERROR] observations must have increasing times\n";
                }
                return results;
            }

            intervalSeconds.push_back(dtSeconds);
            observedAzRateDegps.push_back(angularRateDegPerSecond(
                observedAzDeg[i],
                observedAzDeg[i - 1],
                dtSeconds
            ));
            observedElRateDegps.push_back(elevationRateDegPerSecond(
                observedElDeg[i],
                observedElDeg[i - 1],
                dtSeconds
            ));
        }
    }

    int badTleCount = 0;
    int staleTleCount = 0;
    int geometryFilteredCount = 0;
    int sgp4ErrorCount = 0;
    std::vector<std::string> badTleSamples;
    std::vector<std::string> staleTleSamples;
    std::vector<std::string> geometryFilteredSamples;
    std::vector<std::string> sgp4ErrorSamples;

    const size_t total = catalog.size();
    const size_t step = progressStep(total);

    if (verbose) {
        std::cout << "[Core] preparing catalog and comparing observations...\n";
        std::cout << "[Core] satellites: " << total
                  << " | observations: " << observations.size()
                  << std::endl;
    }

    constexpr double maxTleAgeDays = 30.0;

    std::vector<std::shared_ptr<const CompiledSatellite>> preparedCatalog;
    preparedCatalog.reserve(catalog.size());

    for (const auto& sat : catalog) {
        std::string invalidReason;
        if (!hasValidTleShape(sat, invalidReason)) {
            addDiagnosticSample(badTleSamples, sat, invalidReason);
            ++badTleCount;
            continue;
        }

        try {
            const std::string key = cacheKey(sat);
            auto cached = compiledCache_.find(key);
            std::shared_ptr<const CompiledSatellite> compiled;

            if (cached != compiledCache_.end()) {
                compiled = cached->second;
            }
            else {
                auto created = std::make_shared<CompiledSatellite>(
                    sat,
                    libsgp4::Tle(sat.name, sat.tle_line1, sat.tle_line2)
                );
                compiledCache_.emplace(key, created);
                compiled = created;
            }

            const double maxAgeDays = maxAbsDaysFromEpoch(
                compiled->epoch,
                observationTimes
            );
            if (maxAgeDays > maxTleAgeDays) {
                std::ostringstream reason;
                reason << "TLE epoch too far from observations"
                       << " | max age days: " << maxAgeDays;
                addDiagnosticSample(staleTleSamples, sat, reason.str());
                ++staleTleCount;
                continue;
            }

            if (!canBeVisibleFromLatitude(
                    compiled->inclination_deg,
                    compiled->tle,
                    observer.lat_deg
                )) {
                std::ostringstream reason;
                reason << "orbit inclination/horizon cannot reach observer latitude"
                       << " | inclination: " << compiled->inclination_deg
                       << " | observer lat: " << observer.lat_deg;
                addDiagnosticSample(geometryFilteredSamples, sat, reason.str());
                ++geometryFilteredCount;
                continue;
            }

            preparedCatalog.push_back(compiled);
        }
        catch (const std::exception& e) {
            addDiagnosticSample(sgp4ErrorSamples, sat, e.what());
            ++sgp4ErrorCount;
            continue;
        }
    }

    if (verbose) {
        std::cout << "[Core] prepared candidates: "
                  << preparedCatalog.size()
                  << " / " << total << std::endl;
    }

    const double invSigmaAz2 = 1.0 / (sigma.sigmaAz_deg * sigma.sigmaAz_deg);
    const double invSigmaEl2 = 1.0 / (sigma.sigmaEl_deg * sigma.sigmaEl_deg);
    const int degreesOfFreedom =
        static_cast<int>(
            2 * observationTimes.size() + 2 * intervalSeconds.size()
        );

    std::vector<MatchResult> resultSlots(preparedCatalog.size());
    std::vector<unsigned char> hasResult(preparedCatalog.size(), 0);

    #if defined(SPUTNIK_HAS_OPENMP)
    #pragma omp parallel for schedule(dynamic) reduction(+:sgp4ErrorCount)
    #endif
    for (int idx = 0; idx < static_cast<int>(preparedCatalog.size()); ++idx) {
        const auto& sat = *preparedCatalog[static_cast<size_t>(idx)];

        try {
            libsgp4::SGP4 propagator(sat.tle);
            libsgp4::Observer localObserver(
                observer.lat_deg,
                observer.lon_deg,
                observer.alt_km
            );

            double chi2 = 0.0;
            std::vector<double> predictedAzSeriesDeg;
            std::vector<double> predictedElSeriesDeg;
            predictedAzSeriesDeg.reserve(observationTimes.size());
            predictedElSeriesDeg.reserve(observationTimes.size());

            for (size_t obsIdx = 0; obsIdx < observationTimes.size(); ++obsIdx) {
                const double minutes =
                    (observationTimes[obsIdx] - sat.epoch).TotalMinutes();

                const libsgp4::Eci eci = propagator.FindPosition(minutes);
                const libsgp4::CoordTopocentric look =
                    localObserver.GetLookAngle(eci);

                const double predictedAzDeg =
                    libsgp4::Util::RadiansToDegrees(look.azimuth);
                const double predictedElDeg =
                    libsgp4::Util::RadiansToDegrees(look.elevation);

                predictedAzSeriesDeg.push_back(predictedAzDeg);
                predictedElSeriesDeg.push_back(predictedElDeg);

                const double dAz = libsgp4::Util::WrapNegPos180(
                    predictedAzDeg - observedAzDeg[obsIdx]
                );
                const double dEl = predictedElDeg - observedElDeg[obsIdx];

                chi2 += (dAz * dAz) * invSigmaAz2
                      + (dEl * dEl) * invSigmaEl2;
            }

            for (size_t rateIdx = 0; rateIdx < intervalSeconds.size(); ++rateIdx) {
                const double dtSeconds = intervalSeconds[rateIdx];
                const double predictedAzRate = angularRateDegPerSecond(
                    predictedAzSeriesDeg[rateIdx + 1],
                    predictedAzSeriesDeg[rateIdx],
                    dtSeconds
                );
                const double predictedElRate = elevationRateDegPerSecond(
                    predictedElSeriesDeg[rateIdx + 1],
                    predictedElSeriesDeg[rateIdx],
                    dtSeconds
                );

                const double sigmaAzRate = effectiveRateSigma(
                    sigma.sigmaAzRate_degps,
                    sigma.sigmaAz_deg,
                    dtSeconds
                );
                const double sigmaElRate = effectiveRateSigma(
                    sigma.sigmaElRate_degps,
                    sigma.sigmaEl_deg,
                    dtSeconds
                );

                const double dAzRate =
                    predictedAzRate - observedAzRateDegps[rateIdx];
                const double dElRate =
                    predictedElRate - observedElRateDegps[rateIdx];

                chi2 += (dAzRate * dAzRate) / (sigmaAzRate * sigmaAzRate)
                      + (dElRate * dElRate) / (sigmaElRate * sigmaElRate);
            }

            MatchResult r;
            r.satellite_name = sat.source.name;
            r.norad_id = sat.source.norad_id;
            r.chi_squared = chi2;
            r.probability = 0.0;
            r.reduced_chi_squared =
                chi2 / static_cast<double>(degreesOfFreedom);
            r.fit_p_value = chiSquareSurvival(chi2, degreesOfFreedom);
            r.catalog_match =
                r.fit_p_value >= kCatalogMatchPValueThreshold;

            resultSlots[static_cast<size_t>(idx)] = r;
            hasResult[static_cast<size_t>(idx)] = 1;
        }
        catch (const std::exception& e) {
            #if defined(SPUTNIK_HAS_OPENMP)
            #pragma omp critical(core_sgp4_samples)
            #endif
            {
                addDiagnosticSample(sgp4ErrorSamples, sat.source, e.what());
            }
            ++sgp4ErrorCount;
        }
    }

    results.reserve(preparedCatalog.size());
    for (size_t i = 0; i < resultSlots.size(); ++i) {
        if (hasResult[i]) {
            results.push_back(resultSlots[i]);
        }
    }

    normalizePosteriorProbabilities(results);
    keepTopN(results, topN);

    if (verbose) {
        std::cout << "[Core] SGP4 angular and angular-rate residuals used\n";
        std::cout << "[Core] skipped bad TLE: " << badTleCount
                  << " | stale TLE: " << staleTleCount
                  << " | geometry filtered: " << geometryFilteredCount
                  << " | skipped SGP4 errors: " << sgp4ErrorCount
                  << std::endl;

        for (const auto& sample : badTleSamples) {
            std::cout << "[Core][bad TLE sample] " << sample << std::endl;
        }

        for (const auto& sample : staleTleSamples) {
            std::cout << "[Core][stale TLE sample] " << sample << std::endl;
        }

        for (const auto& sample : geometryFilteredSamples) {
            std::cout << "[Core][geometry filter sample] " << sample << std::endl;
        }

        for (const auto& sample : sgp4ErrorSamples) {
            std::cout << "[Core][SGP4 error sample] " << sample << std::endl;
        }
    }

    return results;
}
