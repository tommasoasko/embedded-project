#include "Core/Inc/recruiting/filters/filters.h"

/** @brief Initialize the moving average filter for the potentiometer
 *
 * @param f Pointer to the moving average filter structure
 */
void moving_average_filter_pot_init(MovingAverageFilterPot_s *f)
{
    f->index = 0;
    f->sum = 0;
    f->count = 0;

    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        f->buffer[i] = 0;
    }
}

/** @brief Initialize the moving average filter for the NTC thermistor
 *
 * @param f Pointer to the moving average filter structure
 */
void moving_average_filter_ntc_init(MovingAverageFilterNTC_s *f)
{
    f->index = 0;
    f->sum = 0;
    f->count = 0;

    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        f->buffer[i] = 0;
    }
}

/** @brief Apply a moving average filter to a new sample of the potentiometer
 *
 * @param f Pointer to the moving average filter structure
 * @param new_sample The new sample to filter
 * @return The filtered sample
 */
uint16_t moving_average_filter_pot_apply(MovingAverageFilterPot_s *f, uint16_t new_sample)
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

/** @brief Apply a moving average filter to a new sample of the NTC thermistor
 *
 * @param f Pointer to the moving average filter structure
 * @param new_sample The new sample to filter
 * @return The filtered sample
 */
uint16_t moving_average_filter_ntc_apply(MovingAverageFilterNTC_s *f, uint16_t new_sample)
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

/** @brief Apply a random noise filter to a new sample
 *
 * @param new_sample The new sample to filter
 * @param noise_level The maximum level of noise to apply
 * @return The filtered sample
 */
uint16_t random_noise_filter_apply(uint16_t new_sample, uint16_t noise_level)
{

    int16_t noise = (int16_t)(rand() % (2 * noise_level + 1)) - noise_level; // random noise in range [-noise_level, noise_level]
    int16_t new_value = new_sample + noise;

    if (new_value < 0)
        new_value = 0;
    if (new_value > UINT16_MAX)
        new_value = UINT16_MAX;

    new_sample = (uint16_t)new_value;

    return new_sample;
}