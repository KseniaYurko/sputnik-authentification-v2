#include "PrepareModule.h"

/**
 * Пока просто возвращает входные данные
 */
std::vector<Observation> PrepareModule::process(
    const std::vector<Observation>& input) {
    return input;
}