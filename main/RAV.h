#ifndef RAV_H
#define RAV_H

#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/task.h"

/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

#define ESPNOW_QUEUE_SIZE           6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum {
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
} example_espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} example_espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} example_espnow_event_recv_cb_t;

typedef union {
    example_espnow_event_send_cb_t send_cb;
    example_espnow_event_recv_cb_t recv_cb;
} example_espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    example_espnow_event_id_t id;
    example_espnow_event_info_t info;
} example_espnow_event_t;

enum {
    EXAMPLE_ESPNOW_DATA_BROADCAST,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

enum{
    DISP_COLABORADOR=0,
    DISP_USUARIO,
    DISP_BALIZA
};

typedef struct{
    char ssid[32];                     //SSID of WiFi.
    char password[64];                 //Password of WiFi.
}WIFI_DETAILS;

typedef struct{
    uint8_t mac[ESP_NOW_ETH_ALEN];     //MAC address of ESPNOW device.
    bool activo;                    //Indicate that if the ESPNOW device is active or not.
}TYPE_ESP_NOW_DEVICE;

typedef struct{
    char command;                     //Command to be executed.
    char red[32];                     //Network name.
    char info[64];                    //Additional information.
}TYPE_PAYLOAD;

typedef uint16_t PIN_TYPE;

typedef uint8_t TYPE_DEVICES_NUM;
typedef uint8_t TYPE_DEVICE;

typedef struct{
    bool alr_init;                      //Indicate that if it is the first time configuration or not.
    WIFI_DETAILS wifi_details;          //WiFi details.
    TYPE_DEVICE device_type;            //Type of device (e.g., collaborator, user, beacon).
    PIN_TYPE pin;                       //Pin number for the device.
    char nomRed[32];                    //Network name.
    char alias[64];                     //Alias for the device.
}TYPE_FLASH_INFO;

typedef struct{
    char red[32];                     //Network name.
    char alias[64];                   //Alias for the device on the network.
}TYPE_NETWORK_INFO;

typedef struct{
    TYPE_NETWORK_INFO network_info;       //Network information.
    TYPE_NETWORK_INFO* next_node;         //Pointer to the next node in the linked list.
}TYPE_DYNAMIC_LIST;

/* User defined field of ESPNOW data in this example. */
typedef struct {
    uint8_t type;                         //Broadcast or unicast ESPNOW data.
    uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
    uint16_t seq_num;                     //Sequence number of ESPNOW data.
    uint16_t crc;                         //CRC16 value of ESPNOW data.
    uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
    uint8_t payload[0];                   //Real payload of ESPNOW data.
} __attribute__((packed)) example_espnow_data_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
    bool unicast;                         //Send unicast ESPNOW data.
    bool broadcast;                       //Send broadcast ESPNOW data.
    uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
    uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
    uint16_t count;                       //Total count of unicast ESPNOW data to be sent.
    uint16_t delay;                       //Delay between sending two ESPNOW data, unit: ms.
    int len;                              //Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];   //MAC address of destination device.
} example_espnow_send_param_t;


// Function prototypes

// first_time_config.c
void first_time(void);

// gpios.c
void gpiosInit();
void gpiosAnimationFirstStart();
void gpiosWifiEnabled();
void gpiosWifiDisabled();

#endif
