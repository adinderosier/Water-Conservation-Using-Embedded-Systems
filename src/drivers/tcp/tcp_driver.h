
#ifndef TCP_DRIVER_H_
#define TCP_DRIVER_H_

#define BUF_SIZE 2048
#define MAX_ITERATIONS 10
#define TCP_PORT 27017
#define POLL_TIME_S 5

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
BaseType_t xInitSTA(void *pvParameters);

#endif /* TCP_DRIVER_H_ */