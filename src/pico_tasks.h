

#ifndef PICO_TASKS_H_
#define PICO_TASKS_H_

/**
 * @brief Task that toggles an LED at a regular interval.
 *
 * This task toggles the onboard LED at a regular interval, specified by the
 * parameter passed to the task. The task uses xTaskDelayUntil to delay until
 * the next execution time.
 *
 * @param pvParameters A pointer to the delay time in milliseconds.
 *
 * @return None.
 */
void vTaskHeartbeat(void *pvParameters);


void vTaskUART(void *pvParameters);

#endif // PICO_TASKS_H_