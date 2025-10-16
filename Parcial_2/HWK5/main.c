#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define WIFI_SSID      "juan pablo's Galaxy A52"
#define WIFI_PASS      "tuxl5077"
#define SERVER_IP      "192.168.51.1"
#define SERVER_PORT    5000
#define CAESAR_SHIFT   3                 // debe coincidir con el del servidor

static const char *TAG = "ESP_CLIENT";

static void caesar(char *s, int k, int decrypt) {
    if (!s) return;
    k = ((k % 26) + 26) % 26;
    if (decrypt) k = 26 - k;
    for (char *p = s; *p; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') *p = 'a' + ((c - 'a' + k) % 26);
        else if (c >= 'A' && c <= 'Z') *p = 'A' + ((c - 'A' + k) % 26);
    }
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wc = {0};
    strcpy((char*)wc.sta.ssid, WIFI_SSID);
    strcpy((char*)wc.sta.password, WIFI_PASS);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "Connecting WiFi to SSID=%s ...", WIFI_SSID);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();

    // Espera IP por DHCP
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (s < 0) { ESP_LOGE(TAG, "socket fail"); vTaskDelay(pdMS_TO_TICKS(2000)); continue; }

        struct sockaddr_in dest = {0};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &dest.sin_addr);
        ESP_LOGI(TAG, "Connecting to %s:%d ...", SERVER_IP, SERVER_PORT);
        if (connect(s, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            ESP_LOGE(TAG, "connect fail");
            close(s);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        const char *msg = "Hello from ESP32!\n";
        send(s, msg, strlen(msg), 0);

        char rx[1024];
        int n = recv(s, rx, sizeof(rx)-1, 0);
        if (n > 0) {
            rx[n] = '\0';
            ESP_LOGI(TAG, "Encrypted from server: %s", rx);
            caesar(rx, CAESAR_SHIFT, /*decrypt=*/1);
            ESP_LOGI(TAG, "Decrypted on ESP32:  %s", rx);
        } else {
            ESP_LOGW(TAG, "No data");
        }
        close(s);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
