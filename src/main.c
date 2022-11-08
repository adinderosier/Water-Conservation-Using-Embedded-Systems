#include <FreeRTOS.h>
#include <task.h>
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


void uart_rx_isr()
{
    while ( uart_is_readable( UART_ID ) )
    {
        uint8_t ch = uart_getc( UART_ID );

        if ( uart_is_writable( UART_ID ) )
        {
            uart_putc( UART_ID, ch );
            printf( "Received UART data VIA ISR: %c\n", ch );
        }
    }
}


int sta_init( void *pvParameters )
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
    {
        printf("CYW43 ARCH: failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    cyw43_wifi_pm(&cyw43_state, 0xa11140);

    printf("Connecting to WiFi...\n");
    printf("SSID: %s\n", WIFI_SSID);
    printf("Password: %s\n", WIFI_PASSWORD);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Wireless connection failed.\n");
        return 1;
    }
    else
    {
        printf("Wireless connected.\n");
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

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, uart_rx_isr);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    // Delay to allow for the USB serial to be ready
    sleep_ms( 5000 );

    printf( "Initialising WiFi...\n" );
    sta_init( NULL );

    printf("Starting FreeRTOS...\n");
    printf( "Starting Heartbeat Task...\n" );
    xTaskCreate( heartbeat_task, "Heartbeat Task", 256, NULL, 1, NULL );

    vTaskStartScheduler();

    while(1){
    };
}
