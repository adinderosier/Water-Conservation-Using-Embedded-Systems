/**
 * @file pico_tasks.h
 * @brief Header file containing declarations for tasks in the Pico W embedded system.
 */

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

/**
 * @brief This function is a task that processes incoming data from a UART queue and calculates the average flow.
 *
 * This task receives data from a queue and processes it to calculate the average flow. The incoming data is expected 
 * to be in the format "total volume,flow" and is separated by a comma. The task then sends a 
 * "clear" command to the device, which resets the total volume then it clears the queue and clears the average flow. 
 *
 * If there is no data in the queue, the task waits for a fixed delay of 1 second before checking the queue again. 
 * 
 * @param pvParameters Unused parameter (required by FreeRTOS API).
 *
 * @return None.
 */
void vTaskUART(__unused void *pvParameters);

#endif /* PICO_TASKS_H_ */