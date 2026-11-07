#ifndef SERIAL_TASK_H
#define SERIAL_TASK_H

#include <stdint.h>

void serial_task_init(void);
void serial_task_routine(uint8_t task_id);

#endif /* SERIAL_TASK_H */