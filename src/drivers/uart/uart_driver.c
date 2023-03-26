/**
 * @file uart_driver.c
 *
 * @brief Source file for UART driver functions.
 *
 * This source file contains implementations for UART driver functions, including
 * initialization and interrupt handling. It includes the required FreeRTOS and
 * Pico headers, as well as a project-specific header for defining the UART ID,
 * baud rate, and pin assignments.
 */

// FreeRTOS includes
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

// Pico includes
#include "hardware/gpio.h"
#include "hardware/uart.h"

// Driver includes
#include "uart_driver.h"

// Queue handles
extern QueueHandle_t xQueueUART;

/**
 * @brief Initialize UART and set up RX interrupt.
 *
 * This function initializes the UART with the specified baud rate and sets the
 * TX and RX pins using the GPIO function select. It also sets up a RX interrupt
 * to handle incoming data from the UART. The interrupt handler is selected based
 * on the UART being used and is enabled after being set up.
 *
 * @param pvParameters Unused parameter (required by FreeRTOS API).
 *
 * @return None.
 */
void vInitUART(__unused void *pvParameters)
{
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // See datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, ISR_UART_RX);
    irq_set_enabled(UART_IRQ, pdTRUE);
}

/**
 * @brief Interrupt service routine for UART receive.
 *
 * This ISR is triggered when the UART receives a byte of data. It reads the data
 * from the UART buffer and sends it to a queue for processing in a task.
 *
 * @return None.
 */
void __attribute__((interrupt)) ISR_UART_RX(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Read data from UART buffer and send to queue
    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);

        // Replace '\r' with '\0'
        char toSend = (ch == '\r') ? '\0' : ch;

        xQueueSendFromISR(xQueueUART, (void *)&toSend, &xHigherPriorityTaskWoken);
    }

    // Yield to a higher priority task if one was unblocked
    if (xHigherPriorityTaskWoken)
    {
        taskYIELD();
    }
}