
// FreeRTOS includes
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stream_buffer.h>

// Standard includes
#include <stdio.h>

// Pico includes
#include "pico/stdlib.h"
#include "pico/stdio.h"

// Driver includes
#include "drivers/uart/uart_driver.h"
#include "drivers/tcp/tcp_driver.h"

// Project includes
#include "pico_tasks.h"

#define HEARTBEAT_MS 500

QueueHandle_t xQueueUART = NULL;
StreamBufferHandle_t xStreamBufferTCP = NULL;

int main()
{
    // Setup the USB as as a serial port
    stdio_usb_init();

    // Initialize the UART settings
    vInitUART(NULL);

    // Delay to allow for the USB serial to be ready
    sleep_ms(5000);

    printf("<main> Initialising WiFi...\n");
    if(xInitSTA(NULL) != pdPASS)
    {
        printf("<main> WiFi failed to initialise!\n");
        exit(1);
    }

    printf("<main> Starting FreeRTOS...\n");

    xTaskCreate(vTaskHeartbeat, "Heartbeat Task", configMINIMAL_STACK_SIZE, (void *)HEARTBEAT_MS, 1, NULL);
    xTaskCreate(vTaskUART, "UART Task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTaskTCP, "TCP Task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Set up a queue for transferring command from the
    // UART interrupt handler to the UART task.
    xQueueUART = xQueueCreate(80, sizeof(char));

    xStreamBufferTCP = xStreamBufferCreate(MAX_RX_STR_LEN, 4);

    vTaskStartScheduler();

    for (;;)
    {
    };
}
