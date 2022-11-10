#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define UART_ID uart0
#define BAUD_RATE PICO_DEFAULT_UART_BAUD_RATE
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define MAX_RX_STR_LEN 32

QueueHandle_t uart_queue = NULL;

void heartbeat_task(void *pvParameters)
{
    while (true)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(100);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(100);
    }
}

void uart_task(void *pvParameters)
{
    // Set up a queue for transferring command from the 
    // UART interrupt handler to the UART task.
    uart_queue = xQueueCreate(80, sizeof(char));

    if (uart_queue == NULL)
    {
        printf("<uart_task> Failed to create queue...\n");
    }

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    // Setup the UART module to continuous mode with a period of 30s
    uart_puts(UART_ID, "c,30");

    vTaskDelay(100);

    // Wait for a response from the UART module
    if (uart_queue != NULL)
    {
        char *queue_buffer = calloc(MAX_RX_STR_LEN, sizeof(char));
        uint8_t queue_buffer_index = 0;
        char cIn;
        do
        {
            if (xQueueReceive(uart_queue, &cIn, (TickType_t)10) == pdPASS)
            {
                queue_buffer[queue_buffer_index++] = cIn;
            }
        } while (cIn != '\0');

        printf("<uart_task> Received: %s\n", queue_buffer);
        free(queue_buffer);
    }

    while (true)
    {
        if(uxQueueMessagesWaiting(uart_queue) > 0)
        {
            char *queue_buffer = calloc(MAX_RX_STR_LEN, sizeof(char));
            uint8_t queue_buffer_index = 0;
            char cIn;
            do
            {
                if (xQueueReceive(uart_queue, &cIn, (TickType_t)10) == pdPASS)
                {
                    queue_buffer[queue_buffer_index++] = cIn;
                }
            } while (cIn != '\0');

            printf("<uart_task> Received: %s\n", queue_buffer);
            free(queue_buffer);
        }
    }
}

void uart_rx_isr()
{
    BaseType_t xHigherPriorityTaskWoken;

    /* We have not woken a task at the start of the ISR. */
    xHigherPriorityTaskWoken = pdFALSE;

    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);

        if (ch == '\r')
        {
            char toSend = '\0';
            xQueueSendFromISR(uart_queue, &toSend, &xHigherPriorityTaskWoken);
        }
        else
        {
            xQueueSendFromISR(uart_queue, &ch, &xHigherPriorityTaskWoken);
        }

        if (uart_is_writable(UART_ID))
        {
            if (ch == '\r')
            {
                uart_putc(UART_ID, '\r');
            }
            else
            {
                uart_putc(UART_ID, ch);
            }
        }
    }
}

int sta_init(void *pvParameters)
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
    {
        printf("<sta_init> CYW43 ARCH: failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    cyw43_wifi_pm(&cyw43_state, 0xa11140);

    printf("<sta_init> Connecting to WiFi...\n");
    printf("<sta_init> SSID: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("<sta_init> Wireless connection failed.\n");
        return 1;
    }
    else
    {
        printf("<sta_init> Wireless connected.\n");
    }
}

void initialize_uart(void *pvParameters)
{
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, uart_rx_isr);
    irq_set_enabled(UART_IRQ, true);
}

int main()
{
    // Setup the USB as as a serial port
    stdio_usb_init();

    // Initialize the UART settings
    initialize_uart(NULL);

    // Delay to allow for the USB serial to be ready
    sleep_ms(5000);

    printf("<main> Initialising WiFi...\n");
    sta_init(NULL);

    printf("<main> Starting FreeRTOS...\n");

    xTaskCreate(heartbeat_task, "Heartbeat Task", 256, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 256, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1)
    {
    };
}
