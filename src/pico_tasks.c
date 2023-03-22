/**
 * @file pico_tasks.c
 * @brief Implementation file containing the functions for tasks in the Pico embedded system.
 */

// FreeRTOS includes
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

// Standard includes
#include <stdio.h>

// Pico includes
//#include "pico/stdlib.h"
//#include "pico/stdio.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
//#include "lwip/pbuf.h"
//#include "lwip/tcp.h"

// Driver includes
#include "drivers/uart/uart_driver.h"

// Project includes
#include "pico_tasks.h"

// Queue handles
extern QueueHandle_t xQueueTCP; // Queue handle for TCP messages (defined elsewhere)
extern QueueHandle_t xQueueUART; // Queue handle for UART messages (defined elsewhere)

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
void vTaskHeartbeat(void *pvParameters)
{
    // Constant delay for parameter time
    const TickType_t xDelay = pdMS_TO_TICKS((int)pvParameters);

    // Get the current tick count
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // LED state
    uint8_t xLEDState = pdFALSE;

    for (;;)
    {
        // Toggle onboard LED
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, (xLEDState = !xLEDState));

        // Delay for parameter time
        xTaskDelayUntil(&xLastWakeTime, xDelay);
    }
}

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
void vTaskUART(__unused void *pvParameters)
{
    // Assert if the queue handle is NULL
    configASSERT(xQueueUART == NULL);

    // Constant delay for task
    const TickType_t xDelay = pdMS_TO_TICKS(1000);

    // Get the current tick count
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    // Used to calculate average flow
    double xAverageFlow = 0;

    // Used to clear the queue and total volume on device
    BaseType_t xClearFlag = pdTRUE;

    for (;;)
    {
        // Check if there is any data in the UART queue
        if (uxQueueMessagesWaiting(xQueueUART) > 0)
        {
            // Allocate memory for buffer to hold incoming data
            char *xQueueBuffer = calloc(MAX_RX_STR_LEN, sizeof(char));
            uint8_t xQueueBufferIndex = 0;
            char cIn;

            // Loop until a null character is found
            do
            {
                // Attempt to receive a character from the UART queue
                if (xQueueReceive(xQueueUART, &cIn, (TickType_t)0) == pdPASS)
                {
                    // Add the received character to the buffer
                    xQueueBuffer[xQueueBufferIndex++] = cIn;
                }
            } while (cIn != '\0');

             // Extract the total volume and flow rate from the buffer
            char *xTotalVolume = strtok_r(xQueueBuffer, ",", &xQueueBuffer);
            char *xFlow = strtok_r(NULL, ",", &xQueueBuffer);

            // Check if the total volume is greater than 0 and the flow rate is 0
            if (atof(xFlow) == 0 && atof(xTotalVolume) > 0 && xClearFlag == pdFALSE)
            {
                if( xAverageFlow > 0)
                {
                    printf("<vTaskUART> Volume: %s, Average Flow: %.2f\n", xTotalVolume, xAverageFlow);
                }
                // Clear the queue and reset the average flow
                xQueueReset(xQueueUART);
                xAverageFlow = 0;
                
                // Set the clear flag
                xClearFlag = pdTRUE;
            }
            // Check if the flow rate is greater than 0 and the clear flag is false
            else if (atof(xFlow) > 0 && xClearFlag == pdFALSE)
            {
                // Calculate the average flow
                xAverageFlow = (xAverageFlow + atof(xFlow)) / 2;
            }

            // Check if the total volume is greater than 0 and the clear flag is true
            if (atof(xTotalVolume) > 0 && xClearFlag == pdTRUE)
            {
                // Send a clear command to the device
                uart_puts(UART_ID, "clear\r");
            }
            // Check if the total volume is 0 and the clear flag is true
            else if (atof(xTotalVolume) == 0 && xClearFlag == pdTRUE)
            {
                // Clear the clear flag
                xClearFlag = pdFALSE;
            }

            // Free all allocated memory
            free(xQueueBuffer);
        }
        // Delay for 500ms
        xTaskDelayUntil(&xLastWakeTime, xDelay);
    }
}

/*void database_task(void *pvParameters)
{
    TCP_CLIENT_T *state = tcp_client_init();

    if (!state)
    {
        return;
    }

    if (!tcp_client_open(state))
    {
        tcp_result(state, -1);
        return;
    }
    while (!state->complete)
    {
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for WiFi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        sleep_ms(1);
    }
    free(state);
}*/