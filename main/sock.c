#include "RAV.h"
#include "sys/socket.h"

const char server_ip[] = "192.168.137.192";
char buffer_sock[100] = {0};

void task_sock(void *Arguments){
    int sock = NULL;
    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE("Socket", "Failed to create socket");
        vTaskDelete(NULL);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6000); // Port number
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE("Socket", "Connection failed");
        close(sock);
        vTaskDelete(NULL);
    }
    ESP_LOGI("Socket", "Connected to server %s on port 6000", server_ip);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA); // Leer la dirección MAC del dispositivo
    sprintf(buffer_sock, "REGISTRAR_CONEXION:%02X.%02X.%02X.%02X.%02X.%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (send(sock, buffer_sock, strlen(buffer_sock), 0) < 0) {
        ESP_LOGE("Socket", "Failed to send data");
    }else{
        ESP_LOGI("Socket", "Data sent successfully: %s", buffer_sock);
    }
    sprintf(buffer_sock, "AGREGAR_NODO:%02X.%02X.%02X.%02X.%02X.%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (send(sock, buffer_sock, strlen(buffer_sock), 0) < 0) {
        ESP_LOGE("Socket", "Failed to send data");
    } else {
        ESP_LOGI("Socket", "Node added successfully: %s", buffer_sock);
    }
    
    while (1) {
        memset(buffer_sock, 0, sizeof(buffer_sock));
        int bytes_received = recv(sock, buffer_sock, sizeof(buffer_sock) - 1, 0);
        if (bytes_received < 0) {
            ESP_LOGE("Socket", "Failed to receive data");
        } else if (bytes_received == 0) {
            ESP_LOGI("Socket", "Connection closed by server");
        }
        buffer_sock[bytes_received] = '\0'; // Null-terminate the received data
        ESP_LOGI("Socket", "Received data: %s", buffer_sock);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
    }
}