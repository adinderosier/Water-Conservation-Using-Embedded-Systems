/**
 * @file uart_driver.h
 *
 * @brief Header file for UART driver functions.
 *
 * This header file contains function prototypes and macros for UART driver
 * functions, including initialization and interrupt handling.
 */

#ifndef UART_DRIVER_H_
#define UART_DRIVER_H_

#define UART_ID uart0
#define BAUD_RATE PICO_DEFAULT_UART_BAUD_RATE
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define MAX_RX_STR_LEN 32

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
void vInitUART(void *pvParameters);

/**
 * @brief Interrupt service routine for UART receive.
 *
 * This ISR is triggered when the UART receives a byte of data. It reads the data
 * from the UART buffer and sends it to a queue for processing in a task.
 *
 * @return None.
 */
void __attribute__((interrupt)) ISR_UART_RX(void);

#endif /* UART_DRIVER_H_ */