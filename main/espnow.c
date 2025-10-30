#include "RAV.h"
#include "freertos/queue.h"

#define ESP_NOW_QUEUE_SIZE 6

QueueHandle_t esp_now_queue;
char buffer_now[105]={0};
TYPE_FLASH_INFO flash_data_now = {0};
char local[100];

void esp_now_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI("ESP-NOW", "Send success to MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
        ESP_LOGE("ESP-NOW", "Send failure to MAC: %02x:%02x:%02x:%02x:%02x:%02x, status: %d",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5], status);
    }
}

void esp_now_receive_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    // ESP_LOGI("ESP-NOW", "Data received "MACSTR": %s", MAC2STR(esp_now_info->src_addr), data);
    memcpy(local, data, data_len);
    local[data_len] = '\0'; // Null-terminate the string for safe printing

    if(strncmp(local, "RAV", 3) == 0) {
        ESP_LOGI("ESP-NOW", "Received RAV data: %s", (char *)local);
        TYPE_PAYLOAD *payload = (TYPE_PAYLOAD *)malloc(sizeof(TYPE_PAYLOAD));
        sscanf((char *)local, "RAV:%"PRIu8":%"PRIu8":%31[^:]:%63[^:]",&(payload->command_type), &(payload->command), payload->red, payload->info);
        memcpy(payload->mac, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
        if (xQueueSend(esp_now_queue, &payload, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE("ESP-NOW", "Failed to send payload to queue");
            free(payload); // Free memory if sending to queue fails
        } else {
            ESP_LOGI("ESP-NOW", "Payload sent to queue successfully");
        }
    }
}

void esp_now_register_peer(const uint8_t *peer_addr) {
        esp_now_peer_info_t peer_info = {
            .channel = ESP_NOW_CHANNEL, // Use the same channel as the AP
            .encrypt = false,
            .ifidx = ESP_IF_WIFI_STA, // Use the station interface
        };
    
    memcpy(peer_info.peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
    esp_err_t ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to add peer: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("ESP-NOW", "Peer added successfully: "MACSTR, MAC2STR(peer_addr));
    }
}

void esp_now_send_data(const uint8_t *peer_addr, const uint8_t *data, int data_len) {
    esp_err_t ret = esp_now_send(peer_addr, data, data_len);
    if (ret != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to send data: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("ESP-NOW", "Data sent successfully to "MACSTR, MAC2STR(peer_addr));
    }
}

void esp_now_send_data_to_all_peers(const uint8_t *data, int data_len) {
    nvs_handle_t nvs_handle;
    nvs_open("node_info", NVS_READONLY, &nvs_handle);
    TYPE_FLASH_NODES node_info = {0};
    size_t size = sizeof(node_info);
    nvs_get_blob(nvs_handle, "node_info", &node_info, &size);
    nvs_close(nvs_handle);
    ESP_LOGI("ESP-NOW", "Sending data to all peers, active nodes: %d", node_info.active_nodes);
    for (uint8_t i = 0; i < node_info.active_nodes; i++) {
        if (node_info.nodes[i].state == STATE_ACTIVO) {
            esp_now_send_data(node_info.nodes[i].mac, data, data_len);
            ESP_LOGI("ESP-NOW", "Data sent to peer: "MACSTR, MAC2STR(node_info.nodes[i].mac));
        } else {
            ESP_LOGI("ESP-NOW", "Skipping inactive peer: "MACSTR, MAC2STR(node_info.nodes[i].mac));
        }
    }
}

void esp_now_remove_peer(const uint8_t *peer_addr) {
    esp_err_t ret = esp_now_del_peer(peer_addr);
    if (ret != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to remove peer: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("ESP-NOW", "Peer removed successfully: "MACSTR, MAC2STR(peer_addr));
    }
}

void esp_now_init_peers(void){
    nvs_handle_t nvs_handle_init_peer;
    TYPE_FLASH_NODES node_info = {0};
    esp_err_t err = nvs_open("node_info", NVS_READONLY, &nvs_handle_init_peer);
    if (err != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to open NVS handle for node_info: %s", esp_err_to_name(err));
        return;
    }
    size_t size = sizeof(node_info);
    err = nvs_get_blob(nvs_handle_init_peer, "node_info", &node_info, &size);
    nvs_close(nvs_handle_init_peer);
    if (err != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to get node info from NVS: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI("ESP-NOW", "Initializing peers from NVS, active nodes: %d", node_info.active_nodes);
    for (uint8_t i = 0; i < node_info.active_nodes; i++) {
        if (node_info.nodes[i].state == STATE_ACTIVO) {
            esp_now_register_peer(node_info.nodes[i].mac);
            ESP_LOGI("ESP-NOW", "Peer initialized: "MACSTR, MAC2STR(node_info.nodes[i].mac));
        } else {
            ESP_LOGI("ESP-NOW", "Skipping inactive peer: "MACSTR, MAC2STR(node_info.nodes[i].mac));
        }
    }
}

void esp_now_task(void *pvArgs){
    bool found = false;
    TYPE_PAYLOAD *data= NULL;
    esp_now_queue = xQueueCreate(ESP_NOW_QUEUE_SIZE, sizeof(TYPE_PAYLOAD *));
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(esp_now_receive_cb);
    esp_now_register_send_cb(esp_now_send_cb);
    ESP_LOGI("ESP-NOW", "ESP-NOW initialized successfully");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("data", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE("ESP-NOW", "Failed to open NVS handle: %s", esp_err_to_name(err));
        return;
    }else if(err == ESP_ERR_NVS_NOT_FOUND){
        memset(&flash_data_now, 0, sizeof(flash_data_now));
    }else{
        size_t size = sizeof(flash_data_now);
        err = nvs_get_blob(nvs_handle, "data", &flash_data_now, &size);
        nvs_close(nvs_handle);
    }
    do{
        ESP_LOGI("ESP-NOW", "Waiting for data in esp_now_queue...");
        xQueueReceive(esp_now_queue, &data, portMAX_DELAY);
        memset(buffer_now, 0, sizeof(buffer_now));
        switch(data->command_type)
        {
            case ESP_NOW_SEND:
                switch(data->command){
                case SEND_SCAN:
                    ESP_LOGI("ESP-NOW", "Received scan command");
                    // Handle scan command
                    // You can implement the logic to scan for available networks here
                    if(flash_data_now.alr_init == false){
                        ESP_LOGI("ESP-NOW", "First time configuration, not responding!");
                        continue; // Skip further processing for first time configuration
                    }
                    esp_now_register_peer(data->mac); // Register the peer for sending response
                    sprintf(buffer_now, "RAV:%d:%d:%s:%s", ESP_NOW_RESPONSE, RESPONSE_DATA, flash_data_now.nomRed, flash_data_now.alias);
                    esp_now_send_data((const uint8_t *)data->mac, (const uint8_t *)buffer_now, strlen(buffer_now));
                    esp_now_remove_peer(data->mac);
                    break;

                case SEND_PAIR:
                    TYPE_FLASH_NODES flash_nodes = {0};
                    found=false;
                    if(strcmp(data->red, flash_data_now.nomRed) != 0){
                        ESP_LOGE("ESP-NOW", "Network name mismatch, expected: %s, received: %s", flash_data_now.nomRed, data->red);
                        continue; // Skip processing if network names do not match
                    }
                    ESP_LOGI("ESP-NOW", "Received pair command for network: %s with mac: %02x:%02x:%02x:%02x:%02x:%02x",
                            data->red,
                            data->mac[0], data->mac[1], data->mac[2],
                            data->mac[3], data->mac[4], data->mac[5]);
                    err = nvs_open("node_info", NVS_READWRITE, &nvs_handle);
                    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                        ESP_LOGE("ESP-NOW", "Failed to open NVS handle for node_info: %s", esp_err_to_name(err));
                        continue; // Skip processing if NVS handle cannot be opened
                    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
                        flash_nodes.active_nodes = 0; // Initialize active nodes to 0
                    } else {
                        size_t size = sizeof(flash_nodes);
                        err = nvs_get_blob(nvs_handle, "node_info", &flash_nodes, &size);
                        if (err != ESP_OK) {
                            ESP_LOGE("ESP-NOW", "Failed to get node info from NVS: %s", esp_err_to_name(err));
                            nvs_close(nvs_handle);
                            continue; // Skip processing if node info cannot be retrieved
                        }
                    }
                    if(flash_nodes.active_nodes >= 20){
                        ESP_LOGE("ESP-NOW", "Maximum number of nodes reached, cannot pair more nodes");
                        nvs_close(nvs_handle);
                        continue; // Skip processing if maximum nodes reached
                    }
                    for(uint8_t i = 0; i < flash_nodes.active_nodes; i++) {
                        if((memcmp(flash_nodes.nodes[i].mac, data->mac, ESP_NOW_ETH_ALEN) == 0)&&(flash_nodes.nodes[i].state == STATE_DESVINCULADO)) {
                            flash_nodes.nodes[i].state = STATE_ACTIVO; // Set state to active
                            found = true;
                            ESP_LOGI("ESP-NOW", "Node with MAC: %02x:%02x:%02x:%02x:%02x:%02x already exists, updating state to active",
                                    data->mac[0], data->mac[1], data->mac[2],
                                    data->mac[3], data->mac[4], data->mac[5]);
                            break;
                        }
                    }
                    if(found==false){
                        for(uint8_t i = 0; i < 20; i++) {
                            if(flash_nodes.nodes[i].state == STATE_DESVINCULADO || flash_nodes.nodes[i].state == STATE_INACTIVO) {
                                memcpy(flash_nodes.nodes[i].mac, data->mac, ESP_NOW_ETH_ALEN);
                                flash_nodes.nodes[i].state = STATE_ACTIVO; // Set state to active
                                found = true;
                                ESP_LOGI("ESP-NOW", "Node paired successfully with MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                                        data->mac[0], data->mac[1], data->mac[2],
                                        data->mac[3], data->mac[4], data->mac[5]);
                                break;
                            }
                        }
                        if(found == false){
                            memcpy(flash_nodes.nodes[flash_nodes.active_nodes].mac, data->mac, ESP_NOW_ETH_ALEN);
                            flash_nodes.nodes[flash_nodes.active_nodes].state = STATE_ACTIVO; // Set state to active
                        }
                        flash_nodes.active_nodes++; // Increment active nodes count
                    }
                    err = nvs_set_blob(nvs_handle, "node_info", &flash_nodes, sizeof(flash_nodes));
                    if (err != ESP_OK) {
                        ESP_LOGE("ESP-NOW", "Failed to set node info in NVS: %s", esp_err_to_name(err));
                        nvs_close(nvs_handle);
                        continue; // Skip processing if node info cannot be saved
                    }
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                    ESP_LOGI("ESP-NOW", "Node paired successfully, total active nodes: %d", flash_nodes.active_nodes);
                    break;
                case SEND_UNPAIR:
                    found = false;
                    ESP_LOGI("ESP-NOW", "Received unpair command for mac: %02x:%02x:%02x:%02x:%02x:%02x",
                            data->mac[0], data->mac[1], data->mac[2],
                            data->mac[3], data->mac[4], data->mac[5]);
                    err = nvs_open("node_info", NVS_READWRITE, &nvs_handle);
                    if (err != ESP_OK) {
                        ESP_LOGE("ESP-NOW", "Failed to open NVS handle for node_info: %s", esp_err_to_name(err));
                        continue; // Skip processing if NVS handle cannot be opened
                    }
                    TYPE_FLASH_NODES flash_nodes_unpair = {0};
                    size_t size = sizeof(flash_nodes_unpair);
                    err = nvs_get_blob(nvs_handle, "node_info", &flash_nodes_unpair, &size);
                    if (err != ESP_OK) {
                        ESP_LOGE("ESP-NOW", "Failed to get node info from NVS: %s", esp_err_to_name(err));
                        nvs_close(nvs_handle);
                        continue; // Skip processing if node info cannot be retrieved
                    }
                    for(uint8_t i = 0; i < flash_nodes_unpair.active_nodes; i++) {
                        if(memcmp(flash_nodes_unpair.nodes[i].mac, data->mac, ESP_NOW_ETH_ALEN) == 0) {
                            flash_nodes_unpair.nodes[i].state = STATE_DESVINCULADO; // Set state to inactive
                            found = true;
                            esp_now_remove_peer(data->mac); 
                            break;
                        }
                    }
                    if(found==false){
                        ESP_LOGW("ESP-NOW", "Node with MAC: %02x:%02x:%02x:%02x:%02x:%02x not found for unpairing",
                                data->mac[0], data->mac[1], data->mac[2],
                                data->mac[3], data->mac[4], data->mac[5]);
                    } else {
                        nvs_set_blob(nvs_handle, "node_info", &flash_nodes_unpair, sizeof(flash_nodes_unpair));
                        nvs_commit(nvs_handle);
                        ESP_LOGI("ESP-NOW", "Node unpaired successfully, total active nodes: %d", flash_nodes_unpair.active_nodes);
                    }
                    break;
                case SEND_ALERT:
                    ESP_LOGI("ESP-NOW", "Received alert command for network: %s", data->red);
                    if(flash_data_now.alr_init == false){
                        ESP_LOGI("ESP-NOW", "First time configuration, not responding!");
                        continue; // Skip further processing for first time configuration
                    }
                    if(strcmp(data->red, flash_data_now.nomRed) != 0){
                        ESP_LOGE("ESP-NOW", "Network name mismatch, expected: %s, received: %s", flash_data_now.nomRed, data->red);
                        continue; // Skip processing if network names do not match
                    }
                    xEventGroupSetBits(wifi_event_group, ESP_NOW_ALERT_IN_PROGRESS);
                    xTaskCreate(gpiosAnimationAlert, "gpiosAnimationAlert", BUTTON_LOGIC_STACK_SIZE, NULL, 5, NULL);
                    break;
                default:
                    ESP_LOGW("ESP-NOW", "Unknown command received: %d", data->command);
                    continue; // Skip further processing for unknown commands
            }
            break;
        case ESP_NOW_RESPONSE:
            switch(data->command){
                case RESPONSE_ACK:
                    ESP_LOGI("ESP-NOW", "Received ACK response from: %02x:%02x:%02x:%02x:%02x:%02x",
                            data->mac[0], data->mac[1], data->mac[2],
                            data->mac[3], data->mac[4], data->mac[5]);
                    break;
                case RESPONSE_NACK:
                    ESP_LOGE("ESP-NOW", "Received NACK response from: %02x:%02x:%02x:%02x:%02x:%02x",
                            data->mac[0], data->mac[1], data->mac[2],
                            data->mac[3], data->mac[4], data->mac[5]);
                    break;
                case RESPONSE_DATA:
                    ESP_LOGI("ESP-NOW", "Received DATA response from: %02x:%02x:%02x:%02x:%02x:%02x, Data: %s",
                            data->mac[0], data->mac[1], data->mac[2],
                            data->mac[3], data->mac[4], data->mac[5],
                            data->info);
                    push_data_to_dynamic_list(data->red, data->info, data->mac);
                    break;
                default:
                    ESP_LOGW("ESP-NOW", "Unknown command received: %d", data->command);
            }
            break;
        default:
            ESP_LOGI("ESP NOW", "Unknown command type received: %d", data->command_type);
            break;
        }
        free(data); 
    }while(true);
}