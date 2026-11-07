#include "timebase-api.h"
#include "tasks-api.h"
#include "Core/Inc/recruiting/sensor-task/sensor-task.h"
#include "Core/Inc/recruiting/control-task/control-task.h"
#include "Core/Inc/recruiting/serial-task/serial-task.h"
#include "Core/Inc/recruiting/bsp/bsp-interface.h"
#include "Core/Inc/recruiting/app-tasks/app-tasks.h"

static struct TimebaseHandler tb_handler;
static struct TasksHandler ts_handler;
static TaskList app_task_list;

/**
 * @brief Initialize the application tasks
 */
void app_tasks_init(void)
{
    timebase_api_init(&tb_handler, 10U); // 10ms resolution
    timebase_set_enable(&tb_handler, true);

    sensor_task_init();
    control_task_init();
    serial_task_init();

    app_task_list[0].task_id = TASK_ID_SENSOR;
    app_task_list[0].function = sensor_task_routine;
    app_task_list[0].interval = 1U;
    app_task_list[0].state = TASKS_STATE_ENABLED;
    app_task_list[0].start = 0U;
    app_task_list[0].repeats = 0;

    app_task_list[1].task_id = TASK_ID_CONTROL;
    app_task_list[1].function = control_task_routine;
    app_task_list[1].interval = 1U;
    app_task_list[1].state = TASKS_STATE_ENABLED;
    app_task_list[1].start = 0U;
    app_task_list[1].repeats = 0;

    app_task_list[2].task_id = TASK_ID_SERIAL;
    app_task_list[2].function = serial_task_routine;
    app_task_list[2].interval = 10U;
    app_task_list[2].state = TASKS_STATE_ENABLED;
    app_task_list[2].start = 0U;
    app_task_list[2].repeats = 0;

    tasks_api_init(&ts_handler, app_task_list, 3, timebase_get_tick(&tb_handler));
}

/**
 * @brief Run the application tasks
 */
void app_tasks_run(void)
{
    static uint32_t last_tick = 0;
    uint32_t current_ms = bsp_get_time_ms();

    // increment timebase every 10ms
    if (current_ms - last_tick >= 10)
    {
        timebase_inc_tick(&tb_handler);
        last_tick = current_ms;
    }

    // scheduler dispatches tasks based on priority heap
    tasks_api_routine(&ts_handler, timebase_get_tick(&tb_handler));
}