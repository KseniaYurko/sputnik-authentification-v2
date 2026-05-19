#pragma once

#include "Modules/core/sgp4/Eci.h"
#include "Modules/core/sgp4/Tle.h"

/**
 * @brief Результат распространения орбиты
 */
struct SatPosition {
    double x;
    double y;
    double z;
};

/**
 * @brief Обёртка над SGP4
 */
class SGP4Wrapper {
public:
    /**
     * @brief Рассчитать ECI-состояние спутника по TLE и времени
     * @param tle TLE спутника
     * @param jd юлианская дата
     */
    static libsgp4::Eci propagateEci(const libsgp4::Tle& tle, double jd);

    /**
     * @brief Рассчитать позицию спутника по TLE и времени
     * @param tle TLE спутника
     * @param jd юлианская дата
     */
    static SatPosition propagate(const libsgp4::Tle& tle, double jd);
};
