#ifndef FILTERS_API_H
#define FILTERS_API_H

#include <stdint.h>
#include <stdlib.h>

typedef struct MovingAverageFilter_s MovingAverageFilter_t;

void Moving_Average_Filter_Init(MovingAverageFilter_t *f);
uint16_t Moving_Average_Filter_Apply(MovingAverageFilter_t *f, uint16_t new_sample);
uint16_t Random_Noise_Filter_Apply(uint16_t new_sample, uint16_t noise_level);

#endif