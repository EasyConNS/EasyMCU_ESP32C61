#ifndef _POR2_H
#define _POR2_H

#include <stdint.h>

// ESP
#include "host/ble_gap.h"
#include "host/ble_store.h"

// Utils
#include "uthash.h"

// Log tags
#define LOG_PRO2_BLE_APP        "pro2_app"
#define LOG_BLE_LL              "ble_ll"
#define LOG_BLE_GATT            "ble_gatt"
#define LOG_BLE_GAP             "ble_gap"
#define LOG_BLE_NVS             "ble_nvs"

// NVS Key
#define NVS_NAME_PAIRING        "PAIRING"
#define NVS_KEY_DEVICE_ADDR     "DEVICE_MAC"
#define NVS_KEY_HOST_ADDR       "HOST_MAC"
#define NVS_KEY_LTK             "BLE_LTK"

// LTK Length
#define LTK_LEN                 16
#define LTK_KEY_SIZE            16
#define ESP_BD_ADDR_LEN          6

// NS2 Data Feature
#define NS2_DATA_EMPTY_LEN      0x21    // write 0x0016 0x00 len 33
#define PRO2_DATA_EMPTY_LEN     0x0e    // notify 0e/1e 0x00 len 14

// HID Report Interval(ms)
#define HID_REPORT_INTERVAL     15

/**
 * Global Constants
 */

// LTK Key B1 - Pro2 Controller fixed value
extern const uint8_t c_ltk_pro2_B1[LTK_KEY_SIZE];

/**
 * Global Variables
 */

// NS2 conn handle
extern uint16_t g_ns2_conn_handle;

// MTU
extern uint16_t g_mtu;

// LTK
extern uint8_t g_ltk[LTK_KEY_SIZE];

// LTK reverse
extern uint8_t g_ltk_re[LTK_KEY_SIZE];

// LTK ESP Store Impl
extern struct ble_store_value_sec* g_ltk_sec;

// NS2 bt address
extern uint8_t g_ns2_addr[ESP_BD_ADDR_LEN];

// NS2 bt address reverse
extern uint8_t g_ns2_addr_re[ESP_BD_ADDR_LEN];

// NS2 bt address ESP Impl
extern ble_addr_t g_ns2_ble_addr;

// Pro2 bt address
extern uint8_t g_pro2_addr[ESP_BD_ADDR_LEN];

// Pro2 bt address reverse
extern uint8_t g_pro2_addr_re[ESP_BD_ADDR_LEN];

// Pro2 device status Enum
typedef enum DEV_STATUS {
  DEV_BOOT,           // esp device boot
  DEV_ADV_IND,        // ble adv started
  DEV_CONNECT_IND,    // ble connected
  DEV_PAIRING,        // ble pairing
  DEV_READY,          // ble paired, ready for hid
} g_status_t;
// Pro2 device status
extern g_status_t g_status;

// BLE Subscription Info

// Subscribe State
typedef struct {
  uint16_t handle;
  uint16_t conn_handle;
  bool notify_enabled;
  bool indicate_enabled;
} g_subscribe_state_t;

// Subscribe Entry
typedef struct {
  uint16_t handle;
  g_subscribe_state_t state;
  UT_hash_handle hh;
} g_subscribe_entry_t;

// Subscribe Table
extern g_subscribe_entry_t *g_subscribe_map;

// BLE ADV Params

// Manufacturer Info
extern uint8_t g_manufacturer_data[26];

// Pro2 ADV opcode
extern uint8_t g_adv_opcode;

/**
 * Global Functions
 */

/**
 * ESP Adapter Layer Functions
*/ 

// BLE Basic

// ble init
void pro2_ble_init(void);
// adv
void pro2_ble_advertise();
// gap handle
int handle_gap_event(struct ble_gap_event* event, void* arg);
// gatt svr init
int gatt_svr_init();
// gatt svr register callback 
void pro2_gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg);
// LTK gen function, only BLE_STORE_GEN_KEY_LTK Impl, Other return -1
int pro2_store_gen_key_cb(uint8_t key,struct ble_store_gen_key *gen_key, uint16_t conn_handle);

// subscription notify function
int pro2_notify(uint16_t conn_handle, uint16_t chr_val_handle, const uint8_t* data, const size_t data_len);

// NVS

// Pro2 Pairing Info save
int pro2_pairing_info_save();

// BLE Store (Pairing)

// BLE Store Config init
void ble_store_config_init(void);
// BLE Store Config read
int ble_store_config_read(int obj_type, const union ble_store_key *key,
                      union ble_store_value *value);
// BLE Store Config write
int ble_store_config_write(int obj_type, const union ble_store_value *val);
// BLE bonding helper function
void ble_gatts_bonding_established(uint16_t conn_handle);

/**
 * Subscription
 */
void subscribe_entry_set(uint16_t handle, uint16_t conn_handle, bool notify_enabled, bool indicate_enabled);
g_subscribe_state_t* subscribe_entry_get(uint16_t handle);
void subscribe_entry_del(uint16_t handle);
void subscribe_map_destroy();

#endif // _POR2_H