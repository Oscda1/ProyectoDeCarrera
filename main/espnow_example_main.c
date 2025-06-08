#include "RAV.h"

TYPE_FLASH_INFO flash_data = {0}; // Global variable to hold flash data

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
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
        ret = nvs_get_blob(my_handle, "data", &flash_data, sizeof(flash_data));
        if(flash_data.alr_init == false){
            ESP_LOGI("NVS", "First time configuration");
            first_time();
        }
    }
    do{
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
    }while(true);

}