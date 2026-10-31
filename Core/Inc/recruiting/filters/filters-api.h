#ifndef FILTERS_API_H
#define FILTERS_API_H

#include <stdint.h>
#include <stdlib.h>

typedef struct MovingAverageFilterPot_s MovingAverageFilterPot_t;
typedef struct MovingAverageFilterNTC_s MovingAverageFilterNTC_t;

void Moving_Average_Filter_Pot_Init(MovingAverageFilterPot_t *f);
void Moving_Average_Filter_NTC_Init(MovingAverageFilterNTC_t *f);
uint16_t Moving_Average_Filter_Pot_Apply(MovingAverageFilterPot_t *f, uint16_t new_sample);
uint16_t Moving_Average_Filter_NTC_Apply(MovingAverageFilterNTC_t *f, uint16_t new_sample);
uint16_t Random_Noise_Filter_Apply(uint16_t new_sample, uint16_t noise_level);

#endif