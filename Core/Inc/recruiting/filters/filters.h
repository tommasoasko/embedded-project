#ifndef FILTERS_H
#define FILTERS_H

#define FILTER_WINDOW_SIZE 150

#include "filters-api.h"

struct MovingAverageFilter_s {
    uint16_t buffer[FILTER_WINDOW_SIZE];
    uint16_t index;
    uint32_t sum;
    uint16_t count;
};

#endif // FILTERS_H