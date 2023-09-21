#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/gpio.h"

#include "esp_wifi.h"

static TaskHandle_t ISR = NULL;

#define BUTTON_OFF 4
#define BUTTON_ON 18
#define LED 22

bool statusLed;

static esp_mqtt_client_handle_t client;

static const char *TAG = "IoT Node Led";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/IOT/LIGHT1", "node connected to mqtt", 0, 1, 0); //sub topic for light
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/IOT/LIGHT1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/IOT/LIGHT1", "node subcribed", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        int lenth_data = event->data_len;

        char *controlLed;
        char status[4];
        controlLed = (char *) event->data;
        printf("%.*s\n",lenth_data, controlLed);
        sprintf(status, "%.*s\n",lenth_data, controlLed);
        if(!strncmp(status, "ON", 2))
        {
            gpio_set_level(LED, 1);
            statusLed = 1;
            printf("Led on!\n");
        }

        if(!strncmp(status, "OFF", 3))
        {
            gpio_set_level(LED, 0);
            statusLed = 0;
            printf("Led off!\n");
        }

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void buttonTask(void* arg) //control led from button
{
    uint32_t io_num;

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(io_num == BUTTON_ON)
            {
                if(!statusLed)
                {
                    gpio_set_level(LED, 1);
                    statusLed = 1;
                    printf("Led on!\n");
                    esp_mqtt_client_publish(client, "/IOT/LIGHT1", "denledon", 0, 1, 0);
                }
            }
            if(io_num == BUTTON_OFF)
            {
                if(statusLed)
                {
                    gpio_set_level(LED, 0);
                    statusLed = 0;
                    printf("Led off!\n");
                    esp_mqtt_client_publish(client, "/IOT/LIGHT1", "denledoff", 0, 1, 0);
                }
            }
        }
    }
}


void mqtt_app_start(void) //start mqtt
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://test.mosquitto.org:1883",
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    //set up led gpio

    gpio_pad_select_gpio(LED);
	gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, 0);
    statusLed = 0;

    //set up button off to turn off warning

    gpio_pad_select_gpio(BUTTON_OFF);
	gpio_set_direction(BUTTON_OFF, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_OFF, GPIO_PULLUP_ONLY);
    gpio_intr_enable(BUTTON_OFF);
    gpio_set_intr_type(BUTTON_OFF, GPIO_INTR_NEGEDGE);

    //set up button on to turn off warning

    gpio_pad_select_gpio(BUTTON_ON);
	gpio_set_direction(BUTTON_ON, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_ON, GPIO_PULLUP_ONLY);
    gpio_intr_enable(BUTTON_ON);
    gpio_set_intr_type(BUTTON_ON, GPIO_INTR_NEGEDGE);

    gpio_evt_queue = xQueueCreate(20, sizeof(uint32_t));
    //start button task
    xTaskCreate(buttonTask, "button_task_example", 2048, NULL, 10, NULL);
    //gpio_install_isr_service(NULL);
    
    gpio_isr_handler_add(BUTTON_OFF, gpio_isr_handler, (void*) BUTTON_OFF);
    gpio_isr_handler_add(BUTTON_ON, gpio_isr_handler, (void*) BUTTON_ON);
}