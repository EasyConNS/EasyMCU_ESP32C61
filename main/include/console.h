#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stdint.h>
#include "nimble/ble.h"

typedef struct {
    ble_addr_t ble_addr;
    uint16_t conn_handle;
    uint16_t mtu;
} console_ns2_t;

extern console_ns2_t g_console_ns2;

#endif // _CONSOLE_H_
