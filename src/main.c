#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"


#define UART_ID uart0
#define BAUD_RATE PICO_DEFAULT_UART_BAUD_RATE
#define UART_TX_PIN 0
#define UART_RX_PIN 1


void heartbeat_task( void *pvParameters )
{    
    while ( true )
    {
        cyw43_arch_gpio_put( CYW43_WL_GPIO_LED_PIN, 1 );
        vTaskDelay( 100 );
        cyw43_arch_gpio_put( CYW43_WL_GPIO_LED_PIN, 0 );
        vTaskDelay( 100 );
    }
}


int main()
{
    // Setup the USB as as a serial port
    stdio_usb_init();

    // Set up our UART with the required speed.
    uart_init( UART_ID, BAUD_RATE );

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function( UART_TX_PIN, GPIO_FUNC_UART );
    gpio_set_function( UART_RX_PIN, GPIO_FUNC_UART );

    // Delay to allow for the USB serial to be ready
    sleep_ms( 5000 );

    if ( cyw43_arch_init_with_country( CYW43_COUNTRY_USA ) ){ printf("WiFi init failed"); }

    printf( "Starting FreeRTOS...\n" );
    sleep_ms( 1000 );

    printf( "Starting Heartbeat Task...\n" );
    xTaskCreate( heartbeat_task, "Heartbeat Task", 256, NULL, 1, NULL );

    vTaskStartScheduler();

    while(1){
    };
}
