#include "Core/Inc/recruiting/filters/filters.h"

void Moving_Average_Filter_Pot_Init(MovingAverageFilterPot_t *f)
{
    f->index = 0;
    f->sum = 0;
    f->count = 0;

    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        f->buffer[i] = 0;
    }
}

void Moving_Average_Filter_NTC_Init(MovingAverageFilterNTC_t *f)
{
    f->index = 0;
    f->sum = 0;
    f->count = 0;

    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        f->buffer[i] = 0;
    }
}

uint16_t Moving_Average_Filter_Pot_Apply(MovingAverageFilterPot_t *f, uint16_t new_sample)
{
    f->sum -= f->buffer[f->index];

    f->buffer[f->index] = new_sample;
    f->sum += new_sample;

    f->index = (f->index + 1) % FILTER_WINDOW_SIZE;

    if (f->count < FILTER_WINDOW_SIZE)
    {
        f->count++;
        return (uint16_t)(f->sum / f->count);
    }

    return (uint16_t)(f->sum / FILTER_WINDOW_SIZE);
}

uint16_t Moving_Average_Filter_NTC_Apply(MovingAverageFilterNTC_t *f, uint16_t new_sample)
{
    f->sum -= f->buffer[f->index];

    f->buffer[f->index] = new_sample;
    f->sum += new_sample;

    f->index = (f->index + 1) % FILTER_WINDOW_SIZE;

    if (f->count < FILTER_WINDOW_SIZE)
    {
        f->count++;
        return (uint16_t)(f->sum / f->count);
    }

    return (uint16_t)(f->sum / FILTER_WINDOW_SIZE);
}

uint16_t Random_Noise_Filter_Apply(uint16_t new_sample, uint16_t noise_level)
{

    int16_t noise = (rand() % (2 * noise_level + 1)) - noise_level; // Random noise in range [-noise_level, noise_level]
    int16_t new_value = (int32_t)new_sample + noise;

    if (new_value < 0)
        new_value = 0;
    if (new_value > UINT16_MAX)
        new_value = UINT16_MAX;

    new_sample = (uint16_t)new_value;

    return new_sample;
}