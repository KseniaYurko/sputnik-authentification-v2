#include "SGP4Wrapper.h"

#include "Modules/core/sgp4/Tle.h"
#include "Modules/core/sgp4/SGP4.h"
#include "Modules/core/sgp4/DateTime.h"

#include <cmath>
#include <cstdint>

using namespace libsgp4;

namespace {

DateTime dateTimeFromJulian(double jd) {
    constexpr double julianOffset = 1721425.5;
    constexpr double ticksPerDay = 86400000000.0;

    const auto ticks = static_cast<int64_t>(
        std::llround((jd - julianOffset) * ticksPerDay)
    );

    return DateTime(ticks);
}

} // namespace

/**
 * @brief Преобразование JD → минуты от эпохи TLE
 */
static double jdToMinutesSinceEpoch(const Tle& tle, double jd) {
    DateTime epoch = tle.Epoch();          
    DateTime current = dateTimeFromJulian(jd);

    return (current - epoch).TotalMinutes();
}

/**
 * @brief Расчёт позиции спутника
 */
Eci SGP4Wrapper::propagateEci(const Tle& tle, double jd) {
    SGP4 sgp4(tle);

    double minutes = jdToMinutesSinceEpoch(tle, jd);

    return sgp4.FindPosition(minutes);
}

/**
 * @brief Расчёт позиции спутника
 */
SatPosition SGP4Wrapper::propagate(const Tle& tle, double jd) {
    Eci eci = propagateEci(tle, jd);

    SatPosition pos;
    pos.x = eci.Position().x;
    pos.y = eci.Position().y;
    pos.z = eci.Position().z;

    return pos;
}
