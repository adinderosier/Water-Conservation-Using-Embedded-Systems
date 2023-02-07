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

#define BUF_SIZE 2048
#define MAX_ITERATIONS 10
#define TCP_PORT 27017
#define POLL_TIME_S 5

QueueHandle_t uart_queue = NULL;
QueueHandle_t tcp_queue = NULL;

typedef struct TCP_CLIENT_T_
{
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool complete;
    int run_count;
    bool connected;
} TCP_CLIENT_T;

static TCP_CLIENT_T *tcp_client_init(void)
{
    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state)
    {
        printf("<tcp_client_init> Failed to allocate state\n");
        return NULL;
    }
    ip4addr_aton(DATABASE_SERVER_IP, &state->remote_addr);
    return state;
}

static err_t tcp_client_close(void *arg)
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL)
    {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK)
        {
            printf("<tcp_client_close> Close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    return err;
}

// Called with results of operation
static err_t tcp_result(void *arg, int status)
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    if (status == 0)
    {
        printf("test success\n");
    }
    else
    {
        printf("test failed %d\n", status);
    }
    state->complete = true;
    return tcp_client_close(arg);
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    if (err != ERR_OK)
    {
        printf("<tcp_client_connected> Connect failed %d\n", err);
        return ERR_TIMEOUT;
    }
    state->connected = true;
    printf("<tcp_client_connected> Waiting for buffer from server\n");
    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err)
{
    if (err != ERR_ABRT)
    {
        printf("<tcp_client_err> %d\n", err);
        tcp_result(arg, err);
    }
}

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb)
{
    printf("<tcp_client_poll>\n");
    return tcp_result(arg, -1); // no response is an error?
}

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)arg;
    printf("<tcp_client_sent> %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE)
    {

        state->run_count++;
        if (state->run_count >= MAX_ITERATIONS)
        {
            tcp_result(arg, 0);
            return ERR_OK;
        }

        // We should receive a new buffer from the server
        state->buffer_len = 0;
        state->sent_len = 0;
        printf("<tcp_client_sent> Waiting for buffer from server\n");
    }

    return ERR_OK;
}

static bool tcp_client_open(void *pvParameters)
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T *)pvParameters;
    printf("<tcp_client_open> Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), TCP_PORT);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
    if (!state->tcp_pcb)
    {
        printf("<tcp_client_open> Failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_poll(state->tcp_pcb, tcp_client_poll, POLL_TIME_S * 2);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_err(state->tcp_pcb, tcp_client_err);

    state->buffer_len = 0;

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, tcp_client_connected);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

void heartbeat_task(void *pvParameters)
{
    const TickType_t xDelay = 500 / portTICK_PERIOD_MS;    
    
    while (true)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(xDelay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(xDelay);
    }
}

void database_task(void *pvParameters)
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

    // Setup the UART module to continuous mode with a period of 1s
    uart_puts(UART_ID, "c,1");
    uart_puts(UART_ID, "Clear");

    vTaskDelay(100);

    while (true)
    {
        if (uxQueueMessagesWaiting(uart_queue) > 0)
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

            char *volume = strtok_r(queue_buffer, ",", &queue_buffer);
            char *flow = strtok_r(queue_buffer, ",", &queue_buffer);

            if (atof(flow) == 0 && atof(volume) > 0)
            {
                //char *database_buffer = calloc(MAX_RX_STR_LEN, sizeof(char));
                printf("Volume: %s, Flow: %s\n", volume, flow);
                //sprintf(database_buffer, "Volume: %s, Flow: %s\n", volume, flow);
                //xQueueSendToBack(tcp_queue, &database_buffer, 0);
                uart_puts(UART_ID, "Clear");
                //free(database_buffer);
            }

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
    xTaskCreate(uart_task, "UART Task", 256, NULL, 1, NULL);
    //xTaskCreate(database_task, "Database Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    
    while (1)
    {
    };
}
