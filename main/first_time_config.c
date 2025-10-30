#include "RAV.h"
#include <esp_http_server.h>
#include <esp_spiffs.h>

#define INDEX_HTML_PATH "/spiffs/register.html"

#define WIFI_OPTIONS_START "<div class=\"wifi-option\" onclick=\"selectWifiNetwork('"
#define WIFI_NO_OPTIONS "<div class=\"wifi-option\">No networks found</div>"
#define ESPNOW_OPTIONS_START "<option value=\""
#define ESPNOW_NO_OPTIONS "<option value=\"\" disabled>No networks found</option>"
#define RESPONSE_SIZE 8192
#define MIN(a,b) ((a) < (b) ? (a) : (b))

TYPE_DYNAMIC_LIST *dynamic_list = NULL; // Pointer to the dynamic list of networks
TYPE_DYNAMIC_LIST *dynamic_list_tail = NULL; // Pointer to the tail of the dynamic list

char wifi_option[256];
char ssid[33];
uint8_t scan_payload[32] = {0};

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
    wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    // Build the response string with available networks
    response[0] = '\0'; // Initialize the response string
    for (int i = 0; i < ap_count; i++) {
        snprintf(ssid, sizeof(ssid), "%s", ap_records[i].ssid);
        if(strlen(ssid) == 0){
            ESP_LOGW("WiFi Scan", "SSID is empty for network %d, skipping...", i);
            continue; // Skip networks with empty SSID
        }
        ESP_LOGI("WiFi Scan", "Network %d: SSID: %s, RSSI: %d", i + 1, ssid, ap_records[i].rssi);
        // Append the WiFi option HTML to the response
        
        snprintf(wifi_option, sizeof(wifi_option), WIFI_OPTIONS_START "%s')\">%s</div>", ssid, ssid);
        strncat(response, wifi_option, RESPONSE_SIZE - strlen(response) - 1);
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
    ESP_LOGI("Dynamic List", "Building response for available networks in dynamic list");
    char *response = (char *)malloc(RESPONSE_SIZE);
    memset(response, 0, RESPONSE_SIZE); // Initialize the response string to avoid garbage values
    response[0] = '\0'; // Initialize the response string
    TYPE_DYNAMIC_LIST *current_node = dynamic_list;
    while (current_node != NULL) {
        // Append the network information to the response
        char network_info[256];
        snprintf(network_info, sizeof(network_info), ESPNOW_OPTIONS_START "%s:%s:%02x.%02x.%02x.%02x.%02x.%02x\">%s : %s </option>", 
                 current_node->network_info.red, current_node->network_info.alias,
                    current_node->network_info.mac[0], current_node->network_info.mac[1],
                    current_node->network_info.mac[2], current_node->network_info.mac[3],
                    current_node->network_info.mac[4], current_node->network_info.mac[5],
                 current_node->network_info.red, current_node->network_info.alias
                 );
        strncat(response, network_info, RESPONSE_SIZE - strlen(response) - 1);
        current_node = current_node->next_node; 
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

void push_data_to_dynamic_list(char* red, char* alias, uint8_t *mac) {
    // Allocate memory for a new node
    TYPE_DYNAMIC_LIST *new_node = (TYPE_DYNAMIC_LIST *)malloc(sizeof(TYPE_DYNAMIC_LIST));
    if (new_node == NULL) {
        ESP_LOGE("Dynamic List", "Memory allocation failed");
        return;
    }
    // Initialize the new node
    strncpy(new_node->network_info.red, red, sizeof(new_node->network_info.red) - 1);
    strncpy(new_node->network_info.alias, alias, sizeof(new_node->network_info.alias) - 1);
    memcpy(new_node->network_info.mac, mac, ESP_NOW_ETH_ALEN);
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

void save_data(httpd_req_t *req) {
    TYPE_FLASH_INFO flash_data = {0};
    TYPE_FLASH_NODES flash_nodes={0};
    char buf[100];
    char device_type[10];
    int ret, remaining = req->content_len;
    uint8_t first_node=0;
    if (remaining > sizeof(buf)) {
        ESP_LOGE("HTTPD", "Request too large");
        httpd_resp_send_err(req, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE, "Request Entity Too Large");
        return;
    }
    // Read the request body
    ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
    if (ret <= 0) {
        ESP_LOGE("HTTPD", "Failed to read request body: %d", ret);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return;
    }
    buf[ret] = '\0'; // Null-terminate the string

   ESP_LOGI("HTTPD", "Received data: %s", buf);
    sscanf(buf, "RAV:%63[^:]:%9[^:]:%31[^:]:%63[^:]:%31[^:]:%*[^:]:%hhx.%hhx.%hhx.%hhx.%hhx.%hhx", 
         flash_data.alias, device_type, flash_data.wifi_details.ssid, flash_data.wifi_details.password, flash_data.nomRed,
         &flash_nodes.nodes[0].mac[0], &flash_nodes.nodes[0].mac[1], &flash_nodes.nodes[0].mac[2],
         &flash_nodes.nodes[0].mac[3], &flash_nodes.nodes[0].mac[4], &flash_nodes.nodes[0].mac[5]);

    ESP_LOGI("HTTPD", "Parsed data: Alias: %s, Device Type: %s, SSID: %s, Password: %s, Network Name: %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                flash_data.alias, device_type, flash_data.wifi_details.ssid, flash_data.wifi_details.password, flash_data.nomRed,
                flash_nodes.nodes[0].mac[0], flash_nodes.nodes[0].mac[1],
                flash_nodes.nodes[0].mac[2], flash_nodes.nodes[0].mac[3],
                flash_nodes.nodes[0].mac[4], flash_nodes.nodes[0].mac[5]);

    if(flash_nodes.nodes[0].mac[0] == 0 && flash_nodes.nodes[0].mac[1] == 0 &&
    flash_nodes.nodes[0].mac[2] == 0 && flash_nodes.nodes[0].mac[3] == 0 &&
    flash_nodes.nodes[0].mac[4] == 0 && flash_nodes.nodes[0].mac[5] == 0) {
        ESP_LOGI("FLASH_NODES","Se acaba de crear la red, primer nodo de la red");
        first_node = 1; // Indicate that this is the first node in the network
    }

    if (strcmp(device_type, "Baliza") == 0) {
        flash_data.device_type = DISP_BALIZA;
        strcpy(flash_data.wifi_details.ssid, "\0");
        strcpy(flash_data.wifi_details.password, "\0");
    } else {
        if (strcmp(flash_data.wifi_details.ssid, "N/A") == 0) {
            strcpy(flash_data.wifi_details.ssid, "\0");
            strcpy(flash_data.wifi_details.password, "\0");
            flash_data.device_type = DISP_USUARIO;
        } else {
            flash_data.device_type = DISP_COLABORADOR;
        }
    }

    flash_data.alr_init = true;
    flash_data.pin = PIN;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("data", NVS_READWRITE, &nvs_handle);
    size_t size = sizeof(flash_data);
    nvs_erase_key(nvs_handle, "data"); 
    err = nvs_set_blob(nvs_handle, "data", &flash_data, size);
    if(err!= ESP_OK) {
        ESP_LOGE("NVS", "Failed to set data in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    nvs_open("data", NVS_READWRITE, &nvs_handle);
    nvs_get_blob(nvs_handle, "data", &flash_data, &size);
    nvs_close(nvs_handle);
    ESP_LOGI("NVS", "Data saved successfully: Alias: %s, SSID: %s, Password: %s, Network Name: %s, Device Type: %d : %s, Pin: %d",
                flash_data.alias,
                flash_data.wifi_details.ssid,
                flash_data.wifi_details.password,
                flash_data.nomRed,
                flash_data.device_type,
                flash_data.device_type == DISP_COLABORADOR ? "Colaborador" :
                flash_data.device_type == DISP_USUARIO ? "Usuario" : "Baliza",
                flash_data.pin);

    if(first_node==0){ 
        flash_nodes.active_nodes = 1;
        err = nvs_open("node_info", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE("NVS", "Failed to open NVS handle for node_info: %s", esp_err_to_name(err));
            return;
        }else{
            nvs_erase_key(nvs_handle, "node_info"); 
            nvs_set_blob(nvs_handle, "node_info", &flash_nodes, sizeof(flash_nodes));
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            ESP_LOGI("NVS", "Node info saved successfully");
        }
        sprintf(buf, "RAV:%d:%d:%s", ESP_NOW_SEND, SEND_PAIR,flash_data.nomRed);
        esp_now_register_peer(flash_nodes.nodes[0].mac); 
        esp_now_send_data(flash_nodes.nodes[0].mac, (uint8_t *)buf, strlen(buf));
    }

    ESP_LOGI("HTTPD", "Data saved successfully: Alias: %s, SSID: %s, Password: %s, Network Name: %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                flash_data.alias, flash_data.wifi_details.ssid, flash_data.wifi_details.password, flash_data.nomRed,
                flash_nodes.nodes[0].mac[0], flash_nodes.nodes[0].mac[1],
                flash_nodes.nodes[0].mac[2], flash_nodes.nodes[0].mac[3],
                flash_nodes.nodes[0].mac[4], flash_nodes.nodes[0].mac[5]);
    memset(buf, 0, sizeof(buf));

    esp_restart();
    return;
}

esp_err_t html_get_handler(httpd_req_t *req) {
    // Initialize the web page
    if(req->method != HTTP_GET && req->method != HTTP_POST) {
        ESP_LOGE("HTTPD", "Unsupported method: %d", req->method);
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
        return ESP_FAIL;
    }
    if(req->method == HTTP_GET){
        ESP_LOGI("HTTPD", "GET request received");
        init_web_page(req);
    } else{
        ESP_LOGI("HTTPD", "POST request received");
        save_data(req);
    }
    return ESP_OK;
}

void AP_WIFI_INIT(){
    wifi_init(1); // Initialize WiFi in AP mode
    xTaskCreate(esp_now_task, "esp_now_task", ESP_NOW_STACK_SIZE, NULL, 5, NULL);

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

        httpd_uri_t index_uri = {
            .uri = "/submit",
            .method = HTTP_POST,
            .handler = html_get_handler, // Reuse the same handler for POST requests
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
            

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
}

void first_time(void){
    uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(gpiosAnimationFirstStart, "gpiosAnimationFirstStart", 2048, NULL, 5, NULL);
    init_spiffs();

    FLASH_DATA_INIT();
    AP_WIFI_INIT();
    esp_now_register_peer(broadcast_mac);
    sprintf((char *)scan_payload, "RAV:%"PRIu8":%"PRIu8, ESP_NOW_SEND, SEND_SCAN);
    esp_now_send_data(broadcast_mac, scan_payload, strlen((char *)scan_payload));
    ESP_LOGI("FIRST_TIME", "First time configuration completed, waiting for data...");
}