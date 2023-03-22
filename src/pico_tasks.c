
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
extern QueueHandle_t xQueueTCP;
extern QueueHandle_t xQueueUART;

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

void vTaskUART(void *pvParameters)
{
    // Assert if the queue handle is NULL
    printf("Checking queue handle...\n");
    configASSERT(xQueueUART == NULL);

    // Constant delay for task
    const TickType_t xDelay = pdMS_TO_TICKS(1000);

    // Get the current tick count
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    // Used to calculate average flow
    double xAverageFlow = 0;

    BaseType_t xClearFlag = pdTRUE;

    for (;;)
    {
        if (uxQueueMessagesWaiting(xQueueUART) > 0)
        {
            char *xQueueBuffer = calloc(MAX_RX_STR_LEN, sizeof(char));
            uint8_t xQueueBufferIndex = 0;
            char cIn;
            do
            {
                if (xQueueReceive(xQueueUART, &cIn, (TickType_t)0) == pdPASS)
                {
                    xQueueBuffer[xQueueBufferIndex++] = cIn;
                }
            } while (cIn != '\0');

            char *xTotalVolume = strtok_r(xQueueBuffer, ",", &xQueueBuffer);
            char *xFlow = strtok_r(NULL, ",", &xQueueBuffer);

            if (atof(xFlow) == 0 && atof(xTotalVolume) > 0 && xClearFlag == pdFALSE)
            {
                if( xAverageFlow > 0)
                {
                    printf("<vTaskUART> Volume: %s, Average Flow: %.2f\n", xTotalVolume, xAverageFlow);
                }
                xQueueReset(xQueueUART);
                xClearFlag = pdTRUE;
                xAverageFlow = 0;
            }
            else if (atof(xFlow) > 0 && xClearFlag == pdFALSE)
            {
                xAverageFlow = (xAverageFlow + atof(xFlow)) / 2;
            }

            if (atof(xTotalVolume) > 0 && xClearFlag == pdTRUE)
            {
                uart_puts(UART_ID, "clear\r");
            }
            else if (atof(xTotalVolume) == 0 && xClearFlag == pdTRUE)
            {
                xClearFlag = pdFALSE;
            }

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