#include "device.h"

ns2_device_t g_dev_ns2 = {
    .ble_addr = {
        .type = BLE_ADDR_PUBLIC,
        .val = { 0 }
    },
    // .addr_re = {0},
    .conn_handle = 0,
    .mtu = 512
};