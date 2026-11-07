#ifndef APP_TASKS_H
#define APP_TASKS_H

#define TASK_ID_SENSOR 0
#define TASK_ID_CONTROL 1
#define TASK_ID_SERIAL 2

void app_tasks_init(void);
void app_tasks_run(void);

#endif