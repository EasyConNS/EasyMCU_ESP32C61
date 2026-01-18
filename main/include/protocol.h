#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>

/**
 * feature flags
 */
#define FEATURE_0C_01_BUTTON_STATE          (0x01U)
#define FEATURE_0C_02_ANALOG_STICKS         (0x02U)
#define FEATURE_0C_04_IMU                   (0x04U)
#define FEATURE_0C_08_UNUSED                (0x08U)

// JoyCon Only
#define FEATURE_0C_10_MOUSE_DATA            (0x10U)
#define FEATURE_0C_20_RUMBLE                (0x20U)
#define FEATURE_0C_40_UNUSED                (0x40U)

#define FEATURE_0C_80_MAGNETOMETER          (0x80U)

// device type
typedef enum {
    DEVICE_TYPE_PRO2,
    DEVICE_TYPE_JOYCON,
} device_type_t;

void encode_feature_info(uint8_t flags, device_type_t type, uint8_t out[8]);

// pro2 gatt cmds & response struct
typedef struct {
    uint8_t cmd;
    uint8_t subcmd;
    uint8_t* rsp_data;
    uint16_t rsp_len;
} pro2_gatt_rsp_t;

// pro2 gatt cmd handle
int cmd_process(pro2_gatt_rsp_t* rsp, uint8_t* data, uint16_t len);

// Memory Sim
typedef struct {
    uint32_t start_addr;
    size_t block_len;
    const uint8_t* data;
} mem_sim_t;
// flash memory
extern mem_sim_t flash_memory;
// memory init
void memory_init();
// read memory block
int read_memory(uint32_t addr, size_t read_len, uint8_t* out_buffer);

#endif // _PROTOCOL_H_