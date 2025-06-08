#include "RAV.h"
#include <esp_http_server.h>
#include <esp_spiffs.h>

#define INDEX_HTML_PATH "/spiffs/register.html"

#define WIFI_OPTIONS_START "<div class=\"wifi-option\" onclick=\"selectWifiNetwork('"
#define WIFI_NO_OPTIONS "<div class=\"wifi-option\">No networks found</div>"
#define ESPNOW_OPTIONS_START "<option value=\""
#define ESPNOW_NO_OPTIONS "<option value=\"\" disabled>No networks found</option>"
#define RESPONSE_SIZE 4096

TYPE_DYNAMIC_LIST *dynamic_list = NULL; // Pointer to the dynamic list of networks
TYPE_DYNAMIC_LIST *dynamic_list_tail = NULL; // Pointer to the tail of the dynamic list

void init_spiffs(){
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK){
        ESP_LOGE("SPIFFS", "Error al inicializar SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK){
        ESP_LOGE("SPIFFS", "Error al obtener información de SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI("SPIFFS", "SPIFFS - Total: %d, Usado: %d", total, used);
    }
}

char *getAPS(){
    // Initialize the response string
    char *response = (char *)malloc(RESPONSE_SIZE);
    memset(response, 0, RESPONSE_SIZE); // Initialize the response string to avoid garbage values
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    // Start the WiFi scan
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_LOGI("WiFi Scan", "Scanning for available networks...");
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI("WiFi Scan", "Found %d networks", ap_count);
    wifi_ap_record_t ap_records[ap_count];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    // Build the response string with available networks
    response[0] = '\0'; // Initialize the response string
    for (int i = 0; i < ap_count; i++) {
        char ssid[33];
        snprintf(ssid, sizeof(ssid), "%s", ap_records[i].ssid);
        if(strlen(ssid) == 0){
            ESP_LOGW("WiFi Scan", "SSID is empty for network %d, skipping...", i);
            continue; // Skip networks with empty SSID
        }
        ESP_LOGI("WiFi Scan", "Network %d: SSID: %s, RSSI: %d", i + 1, ssid, ap_records[i].rssi);
        // Append the WiFi option HTML to the response
        char wifi_option[256];
        snprintf(wifi_option, sizeof(wifi_option), WIFI_OPTIONS_START "%s')\">%s</div>", ssid, ssid);
        strncat(response, wifi_option, RESPONSE_SIZE - strlen(response) - 1);
        ESP_LOGI("WiFi Scan", "Response: %s", response);
    }
    if (ap_count == 0) {
        // If no networks found, add a message
        ESP_LOGW("WiFi Scan", "No networks found, adding no options message");
        snprintf(response, strlen(WIFI_NO_OPTIONS)+1, WIFI_NO_OPTIONS);
    }
    ESP_LOGI("WiFi Scan", "Available networks response built successfully");
    return response; // Return the response string with available networks
}

char *getNetworks() {
    // Initialize the response string
    char *response = (char *)malloc(RESPONSE_SIZE);
    memset(response, 0, RESPONSE_SIZE); // Initialize the response string to avoid garbage values
    response[0] = '\0'; // Initialize the response string
    TYPE_DYNAMIC_LIST *current_node = dynamic_list;
    while (current_node != NULL) {
        // Append the network information to the response
        char network_info[256];
        snprintf(network_info, sizeof(network_info), ESPNOW_OPTIONS_START "%s : %s\">%s : %s</option>", 
                 current_node->network_info.red, current_node->network_info.alias,
                 current_node->network_info.red, current_node->network_info.alias);
        strncat(response, network_info, RESPONSE_SIZE - strlen(response) - 1);
    }
    if (dynamic_list == NULL) {
        // If no networks found, add a message
        ESP_LOGW("Dynamic List", "No networks found in the dynamic list");
        snprintf(response, strlen(ESPNOW_NO_OPTIONS)+1, ESPNOW_NO_OPTIONS);
    }
    ESP_LOGI("Dynamic List", "Available networks response built successfully");
    return response; // Return the response string with available networks
}

static void init_web_page(httpd_req_t *req) {
    // Read the HTML file from SPIFFS
    FILE* f = fopen(INDEX_HTML_PATH, "r");

    char *response = (char *)malloc(RESPONSE_SIZE);
    long file_size=0;
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    // Allocate memory for the response
    char *f_content = (char *)malloc(file_size + 1);
    // Read the HTML file content
    fread(f_content, 1, file_size, f);
    f_content[file_size] = '\0'; // Null-terminate the string

    fclose(f);

    asprintf(&response, f_content,
             getAPS(), // Insert available WiFi networks
             getNetworks() // Insert available networks from the dynamic list
    );
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    free(response);
    free(f_content);
}

static esp_err_t js_get_handler(httpd_req_t *req) {
    // Open the JavaScript file from SPIFFS
    FILE* f = fopen("/spiffs/script.js", "r");
    if (f == NULL) {
        ESP_LOGE("HTTPD", "Failed to open file: /spiffs/script.js");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Get the file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate memory for the file content
    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        ESP_LOGE("HTTPD", "Failed to allocate memory for script.js");
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the file content
    fread(buffer, 1, size, f);
    buffer[size] = '\0'; // Null-terminate the string
    fclose(f);

    // Send the file content as the response
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, buffer, size);

    free(buffer);
    return ESP_OK;
}

static esp_err_t css_get_handler(httpd_req_t *req) {
    // Open the CSS file from SPIFFS
    FILE* f = fopen("/spiffs/style.css", "r");
    if (f == NULL) {
        ESP_LOGE("HTTPD", "Failed to open file: /spiffs/style.css");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Get the file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate memory for the file content
    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        ESP_LOGE("HTTPD", "Failed to allocate memory for style.css");
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the file content
    fread(buffer, 1, size, f);
    buffer[size] = '\0'; // Null-terminate the string
    fclose(f);

    // Send the file content as the response
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, buffer, size);

    free(buffer);
    return ESP_OK;
}

void push_data_to_dynamic_list(char* red, char* alias) {
    // Allocate memory for a new node
    TYPE_DYNAMIC_LIST *new_node = (TYPE_DYNAMIC_LIST *)malloc(sizeof(TYPE_DYNAMIC_LIST));
    if (new_node == NULL) {
        ESP_LOGE("Dynamic List", "Memory allocation failed");
        return;
    }
    // Initialize the new node
    strncpy(new_node->network_info.red, red, sizeof(new_node->network_info.red) - 1);
    strncpy(new_node->network_info.alias, alias, sizeof(new_node->network_info.alias) - 1);
    new_node->next_node = NULL;

    // If the list is empty, set the new node as the head
    if (dynamic_list == NULL) {
        dynamic_list = new_node;
    } else {
        // Traverse to the end of the list and append the new node
        dynamic_list_tail->next_node = new_node;
    }
    dynamic_list_tail = new_node;
    ESP_LOGI("Dynamic List", "Added network: %s with alias: %s", red, alias);
}

void FLASH_DATA_INIT() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("info", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS handle: %s", esp_err_to_name(err));
        return;
    }
    // Check if the data is already initialized
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, "data", NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Data not found, initialize it
        ESP_LOGI("NVS", "Data not found, initializing...");
        TYPE_FLASH_INFO flash_data = {0};
        nvs_set_blob(nvs_handle, "data", &flash_data, sizeof(flash_data));
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE("NVS", "Failed to commit data: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI("NVS", "Data initialized successfully");
        }
    }
    // If data already exist don't do anything to not consume flash writes, it will be overwrited later in the config phase
    return;
}

esp_err_t html_get_handler(httpd_req_t *req) {
    // Initialize the web page
    init_web_page(req);
    return ESP_OK;
}

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

void esp_now_receive_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    // Handle received data
    ESP_LOGI("ESP-NOW", "Received data from MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI("ESP-NOW", "Data length: %d", data_len);
    ESP_LOG_BUFFER_HEX("ESP-NOW", data, data_len);
    if(strstr((const char *)data, "RAV") != NULL) {
        ESP_LOGI("ESP-NOW", "Payload: %s", data);
        TYPE_PAYLOAD payload;
        memset(&payload, 0, sizeof(payload));
        sscanf((const char *)data, "RAV:%c[^:]%32s[^:]%64s", &payload.command, payload.red, payload.info);
        if(payload.command == 'P'){
            ESP_LOGI("ESP-NOW", "Command of pairing received: %c", payload.command);
            // Add the network to the dynamic list
            push_data_to_dynamic_list(payload.red, payload.info);
        }
    }
}

void AP_WIFI_INIT(){
    ESP_LOGI("AP_WIFI_INIT", "Initializing WiFi in AP mode");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    // Initialize the WiFi in AP mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Set up the WiFi configuration
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "RAV",
            .ssid_len = strlen("RAV"),
            .authmode = WIFI_AUTH_OPEN,
            .channel = 5,
            .max_connection = 1,
            .beacon_interval = 100,
        },
    };
    // Set the WiFi configuration
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    // Start the WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI("AP_WIFI_INIT", "WiFi AP mode initialized with SSID: %s", wifi_config.ap.ssid);
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if(httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI("AP_WIFI_INIT", "HTTP server started successfully");
        // Register the GET handler for the root path
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = html_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t css_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = css_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &css_uri);

        httpd_uri_t js_uri = {
            .uri = "/script.js",
            .method = HTTP_GET,
            .handler = js_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &js_uri);
    } else {
        ESP_LOGE("AP_WIFI_INIT", "Failed to start HTTP server");
    }

    // Initialize ESP-NOW
    ESP_LOGI("AP_WIFI_INIT", "Initializing ESP-NOW");
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI("AP_WIFI_INIT", "ESP-NOW initialized successfully");
    // Register the send and receive callbacks
    ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_receive_cb));
    ESP_LOGI("AP_WIFI_INIT", "Send and receive callbacks registered successfully");
    // Set the broadcast peer
    uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t peer_info = {
        .channel = 5, // Use the same channel as the AP
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    esp_err_t ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK) {
        ESP_LOGE("AP_WIFI_INIT", "Failed to add peer: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("AP_WIFI_INIT", "Broadcast peer added successfully");
    }
}

void first_time(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_spiffs();

    FLASH_DATA_INIT();
    AP_WIFI_INIT();
}