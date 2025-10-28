#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_system.h"

#define WIFI_SSID      "casa"          // <-- tu SSID
#define WIFI_PASS      "8857640388"    // <-- tu password

// -------- Adafruit IO ----------
#define AIO_USERNAME   "Jplarios12"
#define AIO_KEY        "aio_tNgy15olNnTcs36nUzX1UfT8FIAZ"
#define AIO_FEED       "temp"

// SIN TLS (puerto 1883):
#define AIO_URI        "mqtt://io.adafruit.com:1883"
// Con TLS sería: "mqtts://io.adafruit.com:8883" + NTP + certificado raíz

static const char *TAG = "MQTT_APP";
static esp_mqtt_client_handle_t g_client = NULL;
static bool mqtt_started = false;

// ---------- Publisher ----------
static void publisher_task(void *arg) {
    char topic[128];
    char payload[64];
    snprintf(topic, sizeof(topic), "%s/feeds/%s", AIO_USERNAME, AIO_FEED);

    float temp = 24.0f;
    while (1) {
        temp += 0.1f;
        if (temp > 30.0f) temp = 24.0f;
        snprintf(payload, sizeof(payload), "%.2f", temp);
        int msg_id = esp_mqtt_client_publish(g_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "PUB -> topic='%s' payload='%s' msg_id=%d", topic, payload, msg_id);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ---------- MQTT events ----------
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            xTaskCreatePinnedToCore(publisher_task, "publisher_task", 4096, NULL, 5, NULL, tskNO_AFFINITY);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT published msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT event error");
            break;
        default:
            break;
    }
}

static void mqtt_start(void) {
    if (mqtt_started) return;
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = AIO_URI,
        .credentials.username = AIO_USERNAME,
        .credentials.authentication.password = AIO_KEY,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .session.keepalive = 30,
        .network.timeout_ms = 30000,
    };
    g_client = esp_mqtt_client_init(&cfg);
    ESP_ERROR_CHECK(g_client ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(g_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(g_client));
    mqtt_started = true;
    ESP_LOGI(TAG, "MQTT client started (URI=%s)", AIO_URI);
}

// ---------- Wi-Fi ----------
static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data) {
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "STA got IP: " IPSTR " / gw " IPSTR,
             IP2STR(&e->ip_info.ip), IP2STR(&e->ip_info.gw));
    // arrancar MQTT sólo cuando ya tenemos IP/DNS
    mqtt_start();
}

static void wifi_init_sta(void) {
    ESP_LOGI(TAG, "Init NVS/WiFi STA");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_cfg = {0};
    strncpy((char*)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, WIFI_PASS, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // handler: cuando obtengamos IP, arrancamos MQTT
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    (void)netif; // evitar warning si no se usa netif directamente
}

// ---------- app_main ----------
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();
    // Nota: NO arrancamos MQTT aquí; se hace en on_got_ip()
}

