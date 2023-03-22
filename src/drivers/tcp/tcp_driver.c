
// FreeRTOS includes
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

// Standard includes
#include <stdio.h>

// Pico includes
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

// Driver includes
#include "tcp_driver.h"

/*typedef struct TCP_CLIENT_T_
{
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool complete;
    int run_count;
    bool connected;
} TCP_CLIENT_T;*/

/**
 * @brief Initializes the CYW43 Wi-Fi module in STA (station) mode and connects to a Wi-Fi network.
 * 
 * @param pvParameters Unused parameter (required by FreeRTOS API).
 * @return pdPASS if the Wi-Fi connection is successful, pdFAIL otherwise.
 * 
 * This function initializes the CYW43 Wi-Fi module in STA mode and attempts to connect to a Wi-Fi network
 * using the SSID and password defined by the WIFI_SSID and WIFI_PASSWORD compile time variables, respectively.
 * 
 * If the connection is successful, this function prints a message to stdout indicating that the connection
 * was successful and returns pdPASS. If the connection fails, this function prints an error message to stdout
 * and returns pdFAIL.
 * 
 * Note that this function assumes that the Wi-Fi module has already been initialized with the correct country code
 * using the cyw43_arch_init_with_country function. This function also enables power management for the Wi-Fi module
 * using the cyw43_wifi_pm function.
 */
BaseType_t xInitSTA(void *pvParameters)
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
    {
        printf("<xInitSTA> CYW43 ARCH: failed to initialise\n");
        return pdFAIL;
    }

    cyw43_arch_enable_sta_mode();

    cyw43_wifi_pm(&cyw43_state, 0xa11140);

    printf("<xInitSTA> Connecting to WiFi...\n");
    printf("<xInitSTA> SSID: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("<xInitSTA> Wireless connection failed.\n");
        return pdFAIL;
    }
    else
    {
        printf("<xInitSTA> Wireless connected.\n");
        return pdPASS;
    }
}

/*static TCP_CLIENT_T *tcp_client_init(void)
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
*/