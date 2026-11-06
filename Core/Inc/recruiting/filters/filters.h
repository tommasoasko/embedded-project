#ifndef FILTERS_H
#define FILTERS_H

#define FILTER_WINDOW_SIZE 150

#include "filters-api.h"

/** @brief Structure containing the context for the potentiometer moving average filter */
struct MovingAverageFilterPot
{
    uint16_t buffer[FILTER_WINDOW_SIZE];
    uint16_t index;
    uint32_t sum;
    uint16_t count;
};

/** @brief Structure containing the context for the NTC thermistor moving average filter */
struct MovingAverageFilterNTC
{
    uint16_t buffer[FILTER_WINDOW_SIZE];
    uint16_t index;
    uint32_t sum;
    uint16_t count;
};

/** @brief Enumerated type for the filter types */
enum FilterType
{
    NO_FILTER,
    MOVING_AVERAGE_FILTER,
    RANDOM_NOISE_FILTER,
};

#endif // FILTERS_H