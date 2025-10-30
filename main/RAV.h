#ifndef RAV_H
#define RAV_H

#include <stdio.h>
#include "string.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_mac.h"

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

// #define ESP_1
#define ESP_2
// #define ESP_3

#if defined(ESP_1)
#define PIN 12345
#elif defined(ESP_2)
#define PIN 00005
#else
#define PIN 12321
#endif

#define ESP_NOW_STACK_SIZE 4096
#define BUTTON_LOGIC_STACK_SIZE 4096

#define ESP_NOW_CHANNEL 11

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
    EXAMPLE_ESPNOW_DATA_BROADCAST=0,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

enum{
    DISP_COLABORADOR=0,
    DISP_USUARIO,
    DISP_BALIZA
};

enum{
    STATE_ACTIVO=0,                   // Significa que el dispositivo está en alcance y emparejado
    STATE_INACTIVO,                   // Significa que no se puede establecer comunicación con el dispositivo
    STATE_DESVINCULADO                // Significa que el dispositivo ha sido olvidado y no se puede volver a emparejar
};

enum{
    SEND_SCAN=0,
    SEND_PAIR,
    SEND_UNPAIR,
    SEND_ALERT,
    SEND_MESSAGE_TO,
};

enum{
    RESPONSE_ACK=0,
    RESPONSE_NACK,
    RESPONSE_DATA,
    RESPONSE_MAX_DEVICES
};

enum{
    ESP_NOW_SEND=0,
    ESP_NOW_RESPONSE,
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
    uint8_t command_type;               //Type of command to be executed. Ej - ESP_NOW_SEND or ESP_NOW_RESPONSE.
    uint8_t command;                    //Command to be executed.
    char red[32];                       //Network name.
    char info[64];                      //Additional information.
    uint8_t mac[ESP_NOW_ETH_ALEN];       //MAC address of the peer device.
}TYPE_PAYLOAD;

typedef uint16_t PIN_TYPE;

typedef uint8_t TYPE_DEVICES_NUM;
typedef uint8_t TYPE_DEVICE;

extern EventGroupHandle_t wifi_event_group;
extern uint8_t WIFI_CONNECTED_BIT, WIFI_NOT_CONNECTED_BIT, ESP_NOW_ALERT_READY_BIT, ESP_NOW_ALERT_IN_PROGRESS;

typedef struct{
    uint8_t mac[ESP_NOW_ETH_ALEN];     //MAC address of the node.
    uint8_t state;                     //State of the node (active, inactive, forgotten).
}TYPE_NODE_INFO;

typedef struct{
    uint8_t active_nodes; //Number of active nodes.
    TYPE_NODE_INFO nodes[20]; // Array of nodes, max 20 nodes.
}TYPE_FLASH_NODES;

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
    uint8_t mac[ESP_NOW_ETH_ALEN];   //MAC address of the device on the network.
}TYPE_NETWORK_INFO;

typedef struct TYPE_DYNAMIC_LIST {
    TYPE_NETWORK_INFO network_info;       //Network information.
    struct TYPE_DYNAMIC_LIST* next_node;  //Pointer to the next node in the linked list.
} TYPE_DYNAMIC_LIST;

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
void push_data_to_dynamic_list(char* red, char* alias, uint8_t* mac);

// gpios.c
void gpiosInit();
void gpiosAnimationFirstStart();
void gpiosWifiEnabled();
void gpiosWifiDisabled();
void gpiosAnimationAlert(void *Arguments);

//espnow.c
void esp_now_task(void *pvArgs);
void esp_now_register_peer(const uint8_t *peer_addr);
void esp_now_send_data(const uint8_t *peer_addr, const uint8_t *data, int data_len);
void esp_now_send_data_to_all_peers(const uint8_t *data, int data_len);
void esp_now_init_peers(void);

//main
void wifi_init(uint8_t mode);

//sock
void task_sock(void *Arguments);

#endif
