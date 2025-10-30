#include "RAV.h"

volatile uint8_t gpio_state = 0;
volatile TickType_t curTime=0;

    uint8_t state = 0, longPressed=0;
    TickType_t press_start_time = 0; // Tiempo en que el botón fue presionado
    TickType_t press_duration = 0;   // Duración de la presión del botón
    TYPE_FLASH_INFO gpio_flash_data = {0}; // Estructura para almacenar los datos de NVS
    char buffer_gpio[105] = {0}; // Buffer para almacenar los datos de ESPNOW

void buttonLogic(void *arg) {
    nvs_handle_t nvs_handle;
    size_t size=sizeof(gpio_flash_data);
                    nvs_open("data", NVS_READWRITE, &nvs_handle);
                    nvs_get_blob(nvs_handle, "data", &gpio_flash_data, &size);
                    nvs_close(nvs_handle); // Cerrar el handle de NVS
                    
    while (1) {
        if(press_start_time==0){
            if(gpio_get_level(GPIO_NUM_13) == 1){ // Si el botón está presionado
                press_start_time = xTaskGetTickCount(); // Guardar el tiempo de inicio
            }
        }else{
            if(gpio_get_level(GPIO_NUM_13) == 0){ // Si el botón se ha soltado
                if(state==1){
                    press_duration = xTaskGetTickCount() - press_start_time; // Calcular la duración de la presión
                    if(press_duration >= pdMS_TO_TICKS(30)){
                        ESP_LOGI("Button Logic", "Boton presionado durante %" PRIu32 " ms", press_duration * portTICK_PERIOD_MS);
                        press_duration = 0; // Reiniciar la duración de la presión
                        press_start_time = 0; // Reiniciar el tiempo de inicio
                        if(xEventGroupGetBits(wifi_event_group) & ESP_NOW_ALERT_IN_PROGRESS){
                            xEventGroupClearBits(wifi_event_group, ESP_NOW_ALERT_IN_PROGRESS); // Limpiar el bit de alerta en progreso
                        }else{
                            if(xEventGroupGetBits(wifi_event_group) & ESP_NOW_ALERT_READY_BIT){
                                sprintf(buffer_gpio, "RAV:%d:%d:%s", ESP_NOW_SEND, SEND_ALERT,gpio_flash_data.nomRed);
                                ESP_LOGI("Button Logic", "Boton presionado brevemente, enviando: %s", buffer_gpio);
                                xEventGroupSetBits(wifi_event_group, ESP_NOW_ALERT_IN_PROGRESS); // Establecer el bit de alerta en progreso
                                esp_now_send_data_to_all_peers((uint8_t *)buffer_gpio, strlen(buffer_gpio)); // Enviar alerta a todos los nodos
                                xTaskCreate(gpiosAnimationAlert, "gpiosAnimationAlert", 2048, NULL, 5, NULL); // Iniciar la animación de alertas
                            }
                        }
                    }else{
                        ESP_LOGI("Button Logic", "Boton presionado brevemente, no se considera una acción");
                        press_duration = 0; // Reiniciar la duración de la presión
                        press_start_time = 0; // Reiniciar el tiempo de inicio
                    }
                    state = 0; // Cambiar el estado a no presionado
                }
                if(longPressed==1){
                    ESP_LOGI("Button Logic", "Boton en presion larga liberado");
                    longPressed = 0; // Reiniciar la presión larga
                    press_start_time = 0; // Reiniciar el tiempo de inicio
                }
            }else{
                if(state==0){
                    state = 1; // Cambiar el estado a presionado
                    ESP_LOGI("Button Logic", "Boton presionado");
                }
                press_duration = xTaskGetTickCount() - press_start_time; // Calcular la duración de la presión
                if((press_duration >= pdMS_TO_TICKS(5000))&&(longPressed==0)){
                    ESP_LOGI("Button Logic", "Boton en presion larga durante %" PRIu32 " ms", press_duration * portTICK_PERIOD_MS);
                    press_duration = 0; // Reiniciar la duración de la presión
                    longPressed = 1; // Indicar que se ha detectado una presión larga
                    nvs_open("node_info", NVS_READWRITE, &nvs_handle);
                    TYPE_FLASH_NODES flash_nodes = {0};
                    size_t size = sizeof(flash_nodes);
                    nvs_get_blob(nvs_handle, "node_info", &flash_nodes, &size); // Obtener los nodos reiniciados
                    for(uint8_t x = 0; x < flash_nodes.active_nodes; x++) {
                        sprintf(buffer_gpio, "RAV:%d:%d", ESP_NOW_SEND, SEND_UNPAIR);
                        esp_now_send_data(flash_nodes.nodes[x].mac, (uint8_t *)buffer_gpio, strlen(buffer_gpio));
                    }
                    memset(&flash_nodes, 0, sizeof(flash_nodes)); // Reiniciar los nodos
                    nvs_commit(nvs_handle); // Guardar los cambios en NVS
                    nvs_close(nvs_handle); // Cerrar el handle de NVS
                    ESP_LOGI("Button Logic", "Datos de NVS reiniciados, reiniciando el dispositivo...");
                    esp_restart(); // Reiniciar el dispositivo
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Delay para evitar un bucle demasiado rápido
    }
}

void gpiosWifiEnabled(){
    ESP_LOGI("gpiosWifiEnabled", "WiFi enabled, turning on green LED and turning off others");
    gpio_set_level(GPIO_NUM_22, 1); // Apagar LED azul
    gpio_set_level(GPIO_NUM_21, 0); // Encender LED verde
    gpio_set_level(GPIO_NUM_23, 1); // Apagar LED rojo
}

void gpiosWifiDisabled(){
    ESP_LOGI("gpiosWifiDisabled", "WiFi disabled, turning on blue LED and turning off others");
    gpio_set_level(GPIO_NUM_23, 1); // Apagar LED rojo
    gpio_set_level(GPIO_NUM_21, 1); // Apagar LED verde
    gpio_set_level(GPIO_NUM_22, 0); // Encender LED azul
}

void gpiosAnimationFirstStart(void *Arguments){
    do{
        gpio_set_level(GPIO_NUM_21, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_21, 1);
        gpio_set_level(GPIO_NUM_22, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_22, 1);
        gpio_set_level(GPIO_NUM_23, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_23, 1);
    }while(1);
}

void gpiosAnimationAlert(void *Arguments){
    gpio_set_level(GPIO_NUM_21, 1); // Apagar LED verde
    gpio_set_level(GPIO_NUM_22, 1); // Apagar LED azul
    do{
        if((xEventGroupGetBits(wifi_event_group) & ESP_NOW_ALERT_IN_PROGRESS)){
            gpio_set_level(GPIO_NUM_23,0); // Encender LED rojo
            vTaskDelay(300 / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_NUM_23,1); // Apagar LED rojo
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }else{
            gpio_set_level(GPIO_NUM_23,1); // Apagar LED rojo
            if(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT){
                gpio_set_level(GPIO_NUM_21, 0); // Encender LED verde
                gpio_set_level(GPIO_NUM_22, 1); // Apagar LED azul
            }else{
                gpio_set_level(GPIO_NUM_21, 1); // Apagar LED verde
                gpio_set_level(GPIO_NUM_22, 0); // Encender LED azul
            }
            vTaskDelete(NULL); // Terminar la tarea si no hay alerta en progreso
        }
            
        
    }while(true);
}

void gpiosMessage(void *Arguments){
    ESP_LOGI("gpiosMessage", "Starting GPIO message animation");
    // Flashing red
    gpio_set_level(GPIO_NUM_21, 1); // Turn off green LED
    gpio_set_level(GPIO_NUM_22, 1); // Turn off blue LED
    do{
        gpio_set_level(GPIO_NUM_23, 0); // Turn on red LED
        vTaskDelay(300 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_23, 1); // Turn off red LED
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }while(1);
}

void gpiosInit(){
    // RGB LEDs
    gpio_reset_pin(GPIO_NUM_21);
    gpio_reset_pin(GPIO_NUM_22);
    gpio_reset_pin(GPIO_NUM_23);
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_22, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_23, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_21, 1);
    gpio_set_level(GPIO_NUM_22, 1);
    gpio_set_level(GPIO_NUM_23, 1);
    // Button
    gpio_reset_pin(GPIO_NUM_13);
    gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLDOWN_ONLY);

    xTaskCreatePinnedToCore(buttonLogic, "buttonLogic", BUTTON_LOGIC_STACK_SIZE, NULL, 5, NULL, 1);
}