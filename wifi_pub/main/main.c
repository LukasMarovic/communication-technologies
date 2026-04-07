#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "mqtt_client.h"

#define BUTTON_GPIO 0
#define WIFI_SSID // ADD
#define WIFI_PASS // ADD
#define BROKER_IP // ADD

static const char *TAG = "example";

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

esp_mqtt_client_handle_t client;

void button_task(void *pvParameters)
{
    int last_state = 1;

    while (1) {
        int state = gpio_get_level(BUTTON_GPIO);

        if (last_state == 1 && state == 0) {
            printf("Button pressed!\n");

            esp_mqtt_client_publish(
                client,
                "esp/button",
                "pressed",
                0,
                1,
                0
            );
        }

        last_state = state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WI-FI CONNECT\n");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WI-FI RETRY\n");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WI-FI SUCCESS\n");
    }
}

void setup_wifi(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };


    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);
}



void mqtt_app_start(void)
{    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_IP
    };

    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_err_t result = esp_mqtt_client_start(client);
    if (result == ESP_OK) ESP_LOGI(TAG, "Connected to broker\n");
}


void app_main(void)
{
    setup_wifi();
    button_init();

    mqtt_app_start();

    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}       