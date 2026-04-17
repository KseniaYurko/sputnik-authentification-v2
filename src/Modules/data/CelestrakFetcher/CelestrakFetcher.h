#pragma once
#include <string>

/**
 * @brief Скачать TLE с Celestrak и вернуть как строку
 */
std::string downloadCelestrakTLE(const std::string& group);