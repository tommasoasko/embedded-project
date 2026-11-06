#ifndef FILTERS_API_H
#define FILTERS_API_H

#include <stdint.h>
#include <stdlib.h>

typedef struct MovingAverageFilterPot MovingAverageFilterPot_s;
typedef struct MovingAverageFilterNTC MovingAverageFilterNTC_s;

void moving_average_filter_pot_init(MovingAverageFilterPot_s *f);
void moving_average_filter_ntc_init(MovingAverageFilterNTC_s *f);
uint16_t moving_average_filter_pot_apply(MovingAverageFilterPot_s *f, uint16_t new_sample);
uint16_t moving_average_filter_ntc_apply(MovingAverageFilterNTC_s *f, uint16_t new_sample);
uint16_t random_noise_filter_apply(uint16_t new_sample, uint16_t noise_level);

#endif