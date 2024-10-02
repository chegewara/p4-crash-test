/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* DESCRIPTION:
 * This example demonstrates using ESP32-S2/S3 as a USB network device. It initializes WiFi in station mode,
 * connects and bridges the WiFi and USB networks, so the USB device acts as a standard network interface that
 * acquires an IP address from the AP/router which the WiFi station connects to.
 */

#include <stdio.h>
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/tcpip.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_check.h"
#include "nvs_flash.h"

#include "esp_netif.h"
#include "esp_event.h"
// #include "tinyusb.h"
// #include "tinyusb_net.h"
#include "dhcpserver/dhcpserver_options.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_mac.h"

#include "esp_netif.h"

#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/esp_netif_net_stack.h"
#include "lwip/esp_pbuf_ref.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"

static const char *TAG = "USB_NCM";

/**
 *  In this scenario of configuring WiFi, we setup USB-Ethernet to create a virtual network and run DHCP server,
 *  so it could assign an IP address to the PC
 *
 *           ESP32               PC
 *      |    lwip MAC=...01   |                        eth NIC MAC=...02
 *      | <DHCP server>   usb | <->  [ USB-NCM device acting as eth-NIC ]
 *      | <HTTP server>       |
 *      | (wifi-provisioning) |
 *
 *  From the PC's NIC perspective the board acts as a separate network with it's own IP and MAC address,
 *  but the virtual ethernet NIC has also it's own IP and MAC address (configured via tinyusb_net_init()).
 *  That's why we need to create the virtual network with *different* MAC address.
 *  Here, we use two different OUI range MAC addresses.
 */
#include "usb_netif.h"

#include "esp_http_server.h"
#include "dns_server.h"
static httpd_handle_t s_web_server = NULL;

static esp_err_t http_get_handler(httpd_req_t *req)
{
    printf("\t %s\n", req->uri);

    const char page[] = "";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page, sizeof(page));
    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_get_handler,
};

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&s_web_server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(s_web_server, &root);
    }
}
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iperf_cmd.h"
#include "esp_console.h"

static void server_task(void *p)
{
    usb_ip_init_default_config();

    // dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("esp32-usb.server" /* name */, CONFIG_USB_NETIF_IF_KEY /* USB netif ID */);
    // start_dns_server(&config);

    // set the minimum lease time
    // uint32_t lease_opt = 3; // n * 30s
    // esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, IP_ADDRESS_LEASE_TIME, &lease_opt, sizeof(lease_opt));

    // start_webserver();

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = "iperf>";
    // init console REPL environment
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    app_register_iperf_commands();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    while (1)
    {
        vTaskDelay(1000);
    }
}

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());


    xTaskCreatePinnedToCore(server_task, "server", 10000, NULL, 1, NULL, 0);


    ESP_LOGI(TAG, "USB NCM and WiFi initialized and started");
    return;
}
