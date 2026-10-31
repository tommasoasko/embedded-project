#ifndef FILTERS_H
#define FILTERS_H

#define FILTER_WINDOW_SIZE 150

#include "filters-api.h"

struct MovingAverageFilterPot_s {
    uint16_t buffer[FILTER_WINDOW_SIZE];
    uint16_t index;
    uint32_t sum;
    uint16_t count;
};

struct MovingAverageFilterNTC_s {
    uint16_t buffer[FILTER_WINDOW_SIZE];
    uint16_t index;
    uint32_t sum;
    uint16_t count;
};

typedef enum {
	NO_FILTER,
	MOVING_AVERAGE_FILTER,
	RANDOM_NOISE_FILTER,
} FilterType_t;

#endif // FILTERS_H