#include "RAV.h"
#include "freertos/event_groups.h"

TYPE_FLASH_INFO flash_data = {0}; // Global variable to hold flash data
EventGroupHandle_t wifi_event_group;
uint8_t WIFI_CONNECTED_BIT = BIT0, WIFI_NOT_CONNECTED_BIT = BIT1;

void wifi_init(void){
    ESP_LOGI("WiFi", "Initializing WiFi...");
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_LOGI("WiFi", "WiFi initialized successfully");
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE("WiFi", "Connection failed or disconnected.");
        // Set the not connected bit in the event group
        xEventGroupSetBits(wifi_event_group, WIFI_NOT_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "Obtained IP:" IPSTR, IP2STR(&event->ip_info.ip));
        // Set the connected bit in the event group
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void try_login(const char *ssid, const char *password) {
    ESP_LOGI("WiFi", "Attempting to connect to SSID: %s", ssid);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    
    ESP_LOGI("WiFi", "Connecting to WiFi...");
    esp_wifi_connect();
    ESP_LOGI("WiFi", "Waiting for connection...");
}

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    gpiosInit();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(ret);
    nvs_handle_t my_handle;
    ret = nvs_open("data", NVS_READWRITE, &my_handle);
    if( ret != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS handle: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("NVS", "NVS handle opened successfully");
    if(ret == ESP_ERR_NVS_NOT_FOUND){
        ESP_LOGI("NVS", "First time configuration");
        first_time();   
    }else{
        size_t size = sizeof(flash_data);
        ret = nvs_get_blob(my_handle, "data", NULL, &size);
        if(ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI("NVS", "Data not found, initializing...");
           first_time();
            do{
                vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
            }while(true);
        }else{
            ret = nvs_get_blob(my_handle, "data", &flash_data, &size);
            nvs_close(my_handle);
            if(flash_data.alr_init == false){
                ESP_LOGI("NVS", "First time configuration");
                first_time();
                do{
                    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
                }while(true);
            }
        }
        ESP_LOGI("NVS", "Data loaded from NVS: Alias: %s, SSID: %s, Password: %s, Network Name: %s, Device Type: %d : %s, Pin: %d",
                 flash_data.alias,
                 flash_data.wifi_details.ssid,
                 flash_data.wifi_details.password,
                 flash_data.nomRed,
                 flash_data.device_type,
                 flash_data.device_type == DISP_COLABORADOR ? "Colaborador" :
                 flash_data.device_type == DISP_USUARIO ? "Usuario" : "Baliza",
                 flash_data.pin);
        wifi_init();
        if(flash_data.device_type == DISP_BALIZA)
            ESP_LOGI("WiFi", "Device type is Baliza, no WiFi connection needed");
        else {
                try_login(flash_data.wifi_details.ssid, flash_data.wifi_details.password);
                xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT| WIFI_NOT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
                if(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT){
                    ESP_LOGI("WiFi", "Connected to WiFi: %s", flash_data.wifi_details.ssid);
                    gpiosWifiEnabled();
                }else{
                    ESP_LOGE("WiFi", "Failed to connect to WiFi: %s", flash_data.wifi_details.ssid);
                    gpiosWifiDisabled();
                }    
        }

    }

    do{
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
    }while(true);

}