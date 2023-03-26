
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
//#include "hardware/gpio.h"
//#include "hardware/uart.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

// Driver includes
#include "tcp_driver.h"

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
BaseType_t xInitSTA(__unused void *pvParameters)
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

/**
 * @brief Initializes a TCP client tcp_client and returns a pointer to the tcp_client.
 *
 * This function allocates memory for the TCP client tcp_client and initializes its remote address.
 * It returns a pointer to the allocated tcp_client. If the memory allocation fails, the function returns NULL.
 *
 * @param pvParameters Unused parameter (required by FreeRTOS API).
 *
 * @return A pointer to the initialized TCP client tcp_client or NULL if memory allocation fails.
 */
TCP_CLIENT_T *xInitTCPClient(__unused void *pvParameters)
{
    // Allocate memory for the TCP client tcp_client
    TCP_CLIENT_T *tcp_client = calloc(1, sizeof(TCP_CLIENT_T));
    
    // Check if memory allocation was successful
    if (!tcp_client)
    {
        // Memory allocation failed
        printf("<xInitTCPClient> Failed to allocate tcp_client\n");
        return NULL;
    }

    // Initialize the TCP client remote address
    ip4addr_aton(CONTROLLER_IP, &tcp_client->remote_addr);
    
    // return the TCP client struct
    return tcp_client;
}

err_t xTCPClientClose(void *arg)
{
    TCP_CLIENT_T *tcp_client = (TCP_CLIENT_T *)arg;
    err_t err = ERR_OK;
    if (tcp_client->tcp_pcb != NULL)
    {
        tcp_arg(tcp_client->tcp_pcb, NULL);
        tcp_poll(tcp_client->tcp_pcb, NULL, 0);
        tcp_sent(tcp_client->tcp_pcb, NULL);
        tcp_recv(tcp_client->tcp_pcb, NULL);
        tcp_err(tcp_client->tcp_pcb, NULL);
        err = tcp_close(tcp_client->tcp_pcb);
        if (err != ERR_OK)
        {
            printf("<xTCPClientClose> Close failed %d, calling abort\n", err);
            tcp_abort(tcp_client->tcp_pcb);
            err = ERR_ABRT;
        }
        tcp_client->tcp_pcb = NULL;
    }
    return err;
}

err_t xTCPClientConnectedCallback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    TCP_CLIENT_T *tcp_client = (TCP_CLIENT_T *)arg;
    if (err != ERR_OK)
    {
        printf("<xTCPClientConnectedCallback> Connection failed %d\n", err);
        return ERR_TIMEOUT;
    }
    printf("<xTCPClientConnectedCallback> Connected to IP: %s\n", ip4addr_ntoa(&tcp_client->remote_addr));
    tcp_client->connected = true;
    return ERR_OK;
}

void vTCPClientErrCallback(void *arg, err_t err)
{
    printf("<xTCPClientErrCallback> %d\n", err);
    xTCPClientClose(arg);
}

err_t xTCPClientPollCallback(void *arg, struct tcp_pcb *tpcb)
{
    printf("<xTCPClientPollCallback>\n");
    return ERR_OK;
}

err_t xTCPClientSentCallback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    TCP_CLIENT_T *tcp_client = (TCP_CLIENT_T *)arg;
    printf("<xTCPClientSentCallback> %u\n", len);
    tcp_client->sent_len -= len;
    return ERR_OK;
}

err_t xTCPClientRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    
    if (!p) {
        printf("<xTCPClientRecvCallback> Connection closed\n");
        xTCPClientClose(arg);
        return err;
    } 

    cyw43_arch_lwip_check();

    uint8_t *buf = p->payload;

    buf[p->tot_len]='\0';

    printf("<xTCPClientRecvCallback> Received data: %s\n", buf);
    
    tcp_recved(tpcb, p->tot_len);

    pbuf_free(p);

    return err;
}


BaseType_t xTCPClientOpen(void *pvParameters)
{
    // Cast the void pointer to a TCP_CLIENT_T pointer
    TCP_CLIENT_T *tcp_client = (TCP_CLIENT_T *)pvParameters;

    printf("<xTCPClientOpen> Connecting to %s port %u\n", ip4addr_ntoa(&tcp_client->remote_addr), TCP_PORT);
    tcp_client->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&tcp_client->remote_addr));
    if (!tcp_client->tcp_pcb)
    {
        printf("<xTCPClientOpen> Failed to create pcb\n");
        return pdFALSE;
    }

    tcp_client->connected=false;
    tcp_client->sent_len=0;

    // Set the TCP client tcp_client as the argument to the TCP callbacks
    tcp_arg(tcp_client->tcp_pcb, tcp_client);

    // Set the TCP poll callback
    tcp_poll(tcp_client->tcp_pcb, xTCPClientPollCallback, POLL_TIME_S * 2);
    
    // Set the TCP sent callback
    tcp_sent(tcp_client->tcp_pcb, xTCPClientSentCallback);
    
    // Set the TCP received callback
    tcp_recv(tcp_client->tcp_pcb, xTCPClientRecvCallback);

    // Set the TCP error callback
    tcp_err(tcp_client->tcp_pcb, vTCPClientErrCallback);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(tcp_client->tcp_pcb, &tcp_client->remote_addr, TCP_PORT, xTCPClientConnectedCallback);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}