#include "RAV.h"
#include "freertos/event_groups.h"

TYPE_FLASH_INFO flash_data = {0}; // Global variable to hold flash data
TYPE_FLASH_NODES main_flash_nodes = {0}; // Global variable to hold flash nodes
EventGroupHandle_t wifi_event_group;
uint8_t WIFI_CONNECTED_BIT = BIT0, WIFI_NOT_CONNECTED_BIT = BIT1, ESP_NOW_ALERT_READY_BIT = BIT2, ESP_NOW_ALERT_IN_PROGRESS=BIT3;

void wifi_init(uint8_t mode){
    ESP_LOGI("WiFi", "Initializing WiFi...");
    esp_netif_init();
    esp_event_loop_create_default();
    if(mode==0)
        esp_netif_create_default_wifi_sta();
    else
        esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    if(mode==0){
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }else{
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        wifi_config_t wifi_config = {
        .ap = {
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 1,
            .beacon_interval = 100,
            .channel = ESP_NOW_CHANNEL,
        }
    };
        // Set the SSID for the AP
        snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "RAV-%"PRIu16, PIN);
        
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        // Start the WiFi

    }
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE)); // Ensure the channel is set to 5
    ESP_LOGI("WiFi", "WiFi initialized successfully");
}



void task_try_wifi(void *arguments){
    nvs_handle_t try_wifi_handle;
    TYPE_FLASH_INFO flash_data_try = {0};
    size_t size;
    esp_err_t ret = nvs_open("data", NVS_READWRITE, &try_wifi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS handle: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }
    size = sizeof(flash_data_try);
    ret = nvs_get_blob(try_wifi_handle, "data", &flash_data_try, &size);
    nvs_close(try_wifi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("NVS", "Failed to get data from NVS: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }
    do{
        vTaskDelay(30000/ portTICK_PERIOD_MS); // Wait for 30 seconds before trying to connect again
        ESP_LOGI("WiFi", "Trying to connect to WiFi: %s", flash_data_try.wifi_details.ssid);
        wifi_init(0); // Initialize WiFi in STA mode
        esp_wifi_connect();
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_NOT_CONNECTED_BIT, pdFALSE, pdFALSE, 15000 / portTICK_PERIOD_MS);
        if(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
            ESP_LOGI("WiFi", "Reconnected to WiFi: %s", flash_data_try.wifi_details.ssid);
            gpiosWifiEnabled();
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            vTaskDelete(NULL); // Exit the task if connected
        }
    }while(1);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    uint8_t channel1, channel2;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE("WiFi", "Connection failed or disconnected.");
        // Set the not connected bit in the event group
        gpiosWifiDisabled();
        xEventGroupSetBits(wifi_event_group, WIFI_NOT_CONNECTED_BIT);
        xTaskCreate(task_try_wifi, "task_try_wifi", 4096, NULL, 5, NULL); // Start the task to try reconnecting
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "Obtained IP:" IPSTR, IP2STR(&event->ip_info.ip));
        // Set the connected bit in the event group
        ESP_ERROR_CHECK(esp_wifi_get_channel(&channel1, &channel2));
        ESP_LOGI("WiFi", "Current channel: %d, Secondary channel: %d", channel1, channel2);
        gpiosWifiEnabled();
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void try_login(const char *ssid, const char *password) {
    ESP_LOGI("WiFi", "Attempting to connect to SSID: %s", ssid);

    // 2) prepara la estructura con SSID y password
    wifi_config_t wifi_config = { 0 };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)-1);

    // 3) aplica la configuración STA
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    // 4) registra tus handlers ANTES de arrancar
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    // 5) arranca la interfaz si no está ya arrancada
    ESP_LOGI("WiFi", "Starting WiFi and connecting...");
    ESP_ERROR_CHECK(esp_wifi_start());      // si ya está arrancada, retorna OK
    ESP_ERROR_CHECK(esp_wifi_connect());
}


void app_main(void){
    size_t size;
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
    ESP_LOGI("NVS", "NVS handle opened successfully");
            size = sizeof(flash_data);
            nvs_get_blob(my_handle, "data", &flash_data, &size);
            nvs_close(my_handle);
            ESP_LOGI("NVS", "Data loaded from NVS: Alias: %s, SSID: %s, Password: %s, Network Name: %s, Device Type: %d : %s, Pin: %d",
                 flash_data.alias,
                 flash_data.wifi_details.ssid,
                 flash_data.wifi_details.password,
                 flash_data.nomRed,
                 flash_data.device_type,
                 flash_data.device_type == DISP_COLABORADOR ? "Colaborador" :
                 flash_data.device_type == DISP_USUARIO ? "Usuario" : "Baliza",
                 flash_data.pin);
            if(flash_data.alr_init == false){
                ESP_LOGI("NVS", "First time configuration");
                first_time();
                do{
                    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
                }while(true);
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
        nvs_open("node_info", NVS_READWRITE, &my_handle);
        size = sizeof(main_flash_nodes);
        ret = nvs_get_blob(my_handle, "node_info", &main_flash_nodes, &size);
        if(ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI("NVS", "Node info not found, initializing...");
            main_flash_nodes.active_nodes = 0; // Initialize active nodes to 0
            nvs_set_blob(my_handle, "node_info", &main_flash_nodes, sizeof(main_flash_nodes));
            nvs_commit(my_handle);
        }
        nvs_close(my_handle);
        ESP_LOGI("NVS", "Node info loaded from NVS: Active Nodes: %d", main_flash_nodes.active_nodes);
        for(uint8_t x=0;x<main_flash_nodes.active_nodes;x++){
            ESP_LOGI("NVS", "Node %d: MAC: %02x:%02x:%02x:%02x:%02x:%02x, State: %s",
                     x+1,
                     main_flash_nodes.nodes[x].mac[0], main_flash_nodes.nodes[x].mac[1],
                     main_flash_nodes.nodes[x].mac[2], main_flash_nodes.nodes[x].mac[3],
                     main_flash_nodes.nodes[x].mac[4], main_flash_nodes.nodes[x].mac[5],
                     main_flash_nodes.nodes[x].state==STATE_ACTIVO ? "Activo" :
                     main_flash_nodes.nodes[x].state==STATE_INACTIVO ? "Inactivo" : "Desvinculado");
        }
        wifi_init(0);
        xTaskCreate(esp_now_task, "esp_now_task", ESP_NOW_STACK_SIZE, NULL, 5, NULL);
        if(flash_data.device_type == DISP_BALIZA){
            ESP_LOGI("WiFi", "Device type is Baliza, no WiFi connection needed");
            gpiosWifiDisabled();
        }else {
                try_login(flash_data.wifi_details.ssid, flash_data.wifi_details.password);
                xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT| WIFI_NOT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
                if(xEventGroupGetBits(wifi_event_group) & WIFI_NOT_CONNECTED_BIT){
                    ESP_LOGE("WiFi", "Failed to connect to WiFi: %s", flash_data.wifi_details.ssid);
                    gpiosWifiDisabled();
                }else{
                    ESP_LOGI("WiFi", "Connected to WiFi: %s", flash_data.wifi_details.ssid);
                    gpiosWifiEnabled();

                }
    }
    esp_now_init_peers();
    ESP_LOGI("Main", "Application started successfully");
    xEventGroupSetBits(wifi_event_group, ESP_NOW_ALERT_READY_BIT); // Set the ESP-NOW alert ready bit
    if((xEventGroupGetBits(wifi_event_group)) & WIFI_CONNECTED_BIT){
        xTaskCreate(task_sock, "task_sock", 8192, NULL, 5, NULL); // Start the socket task if WiFi is connected
    }
}