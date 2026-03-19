#include "uart.h"
#include "hid.h"
#include "device.h"
#include "utils.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"

// Log tag
#define LOG_UART "uart"

// Global UART manager
dev_uart_manager_t g_uart_manager = {
    .event_queue = NULL,
    .report_mutex = NULL,
    .uart_task_handle = NULL,
    .initialized = false
};

// Simple protocol format:
// Frame format: [START_BYTE][TYPE][DATA...][CHECKSUM]
#define UART_START_BYTE         0xAA
#define UART_TYPE_BUTTON        0x01
#define UART_TYPE_STICK         0x02
#define UART_MAX_FRAME_SIZE     16

// Button mapping from UART ID to pro2_btns
static const pro2_btns button_map[] = {
    A,      // ID 0
    B,      // ID 1
    X,      // ID 2
    Y,      // ID 3
    L,      // ID 4
    R,      // ID 5
    ZL,     // ID 6
    ZR,     // ID 7
    Minus,  // ID 8
    Plus,   // ID 9
    LClick, // ID 10
    RClick, // ID 11
    Home,   // ID 12
    Capture,// ID 13
    Up,     // ID 14
    Down,   // ID 15
    Left,   // ID 16
    Right,  // ID 17
    GR,     // ID 18
    GL,     // ID 19
    C       // ID 20
};
#define BUTTON_MAP_SIZE (sizeof(button_map) / sizeof(button_map[0]))

static uint8_t calculate_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

dev_uart_event_type_t dev_uart_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event) {
    if (len < 4 || data[0] != UART_START_BYTE) {
        return UART_EVENT_UNKNOWN;
    }

    uint8_t frame_type = data[1];
    size_t data_len = len - 3; // Exclude start byte, type, and checksum

    // Verify checksum
    uint8_t checksum = calculate_checksum(data, len - 1);
    if (checksum != data[len - 1]) {
        ESP_LOGE(LOG_UART, "Checksum mismatch: expected 0x%02X, got 0x%02X", checksum, data[len - 1]);
        return UART_EVENT_UNKNOWN;
    }

    switch (frame_type) {
        case UART_TYPE_BUTTON:
            if (data_len == 2) {
                uint8_t button_id = data[2];
                bool pressed = data[3] != 0;

                if (button_id < BUTTON_MAP_SIZE) {
                    event->type = UART_EVENT_BUTTON;
                    event->data.button.button_id = button_id;
                    event->data.button.pressed = pressed;
                    ESP_LOGD(LOG_UART, "Button event: id=%d, pressed=%d", button_id, pressed);
                    return UART_EVENT_BUTTON;
                }
            }
            break;

        case UART_TYPE_STICK:
            if (data_len == 5) {
                uint8_t stick_id = data[2];
                uint16_t x = (data[3] << 8) | data[4];
                uint16_t y = (data[5] << 8) | data[6];

                // Validate stick coordinates (12-bit)
                if (stick_id < 2 && x <= 0xFFF && y <= 0xFFF) {
                    event->type = UART_EVENT_STICK;
                    event->data.stick.stick_id = stick_id;
                    event->data.stick.x = x;
                    event->data.stick.y = y;
                    ESP_LOGD(LOG_UART, "Stick event: id=%d, x=0x%03X, y=0x%03X", stick_id, x, y);
                    return UART_EVENT_STICK;
                }
            }
            break;

        default:
            ESP_LOGW(LOG_UART, "Unknown frame type: 0x%02X", frame_type);
            break;
    }

    return UART_EVENT_UNKNOWN;
}

static void uart_rx_task(void *arg) {
    uint8_t data[UART_RX_BUFFER_SIZE];
    uint8_t frame_buffer[UART_MAX_FRAME_SIZE];
    size_t frame_pos = 0;
    bool in_frame = false;

    ESP_LOGI(LOG_UART, "UART RX task started");

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, sizeof(data), pdMS_TO_TICKS(10));

        if (len > 0) {
            ESP_LOGD(LOG_UART, "Received %d bytes", len);

            for (int i = 0; i < len; i++) {
                uint8_t byte = data[i];

                if (!in_frame && byte == UART_START_BYTE) {
                    // Start of new frame
                    frame_pos = 0;
                    frame_buffer[frame_pos++] = byte;
                    in_frame = true;
                    ESP_LOGD(LOG_UART, "Frame start detected");
                } else if (in_frame) {
                    // Continue frame
                    if (frame_pos < UART_MAX_FRAME_SIZE) {
                        frame_buffer[frame_pos++] = byte;

                        // TODO EasyCon Adapter
                        
                        // Simple frame completion detection:
                        // Minimum frame size is 4 bytes, we check if we have enough data
                        if (frame_pos >= 4) {
                            dev_uart_event_t event;
                            dev_uart_event_type_t type = dev_uart_parse_frame(frame_buffer, frame_pos, &event);

                            if (type != UART_EVENT_UNKNOWN) {
                                // Valid frame, send to queue
                                if (g_uart_manager.event_queue != NULL) {
                                    if (xQueueSend(g_uart_manager.event_queue, &event, 0) != pdTRUE) {
                                        ESP_LOGW(LOG_UART, "Event queue full, dropping event");
                                    }
                                }
                                in_frame = false;
                            } else if (frame_pos >= UART_MAX_FRAME_SIZE) {
                                // Frame too long or invalid
                                ESP_LOGW(LOG_UART, "Invalid frame or frame too long");
                                in_frame = false;
                            }
                        }
                    } else {
                        // Frame too long
                        ESP_LOGW(LOG_UART, "Frame too long, resetting");
                        in_frame = false;
                    }
                }
            }
        }

        // Small delay to yield CPU
        vTaskDelay(1);
    }
}

int dev_uart_init(void) {
    if (g_uart_manager.initialized) {
        ESP_LOGW(LOG_UART, "UART already initialized");
        return 0;
    }

    // Create event queue
    g_uart_manager.event_queue = xQueueCreate(32, sizeof(dev_uart_event_t));
    if (g_uart_manager.event_queue == NULL) {
        ESP_LOGE(LOG_UART, "Failed to create event queue");
        return -1;
    }

    // Create mutex for HID report access
    g_uart_manager.report_mutex = xSemaphoreCreateMutex();
    if (g_uart_manager.report_mutex == NULL) {
        ESP_LOGE(LOG_UART, "Failed to create mutex");
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
        return -1;
    }

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(UART_PORT_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to configure UART parameters: %s", esp_err_to_name(err));
        goto error;
    }

    // Set UART pins
    err = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to set UART pins: %s", esp_err_to_name(err));
        goto error;
    }

    // Install UART driver
    err = uart_driver_install(UART_PORT_NUM, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to install UART driver: %s", esp_err_to_name(err));
        goto error;
    }

    ESP_LOGI(LOG_UART, "UART initialized on port %d, baud rate %d", UART_PORT_NUM, UART_BAUD_RATE);
    ESP_LOGI(LOG_UART, "RX pin: GPIO%d, TX pin: GPIO%d", UART_RX_PIN, UART_TX_PIN);

    g_uart_manager.initialized = true;
    return 0;

error:
    if (g_uart_manager.event_queue != NULL) {
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
    }
    if (g_uart_manager.report_mutex != NULL) {
        vSemaphoreDelete(g_uart_manager.report_mutex);
        g_uart_manager.report_mutex = NULL;
    }
    return -1;
}

int dev_uart_start_task(void) {
    if (!g_uart_manager.initialized) {
        ESP_LOGE(LOG_UART, "UART not initialized");
        return -1;
    }

    if (g_uart_manager.uart_task_handle != NULL) {
        ESP_LOGW(LOG_UART, "UART task already started");
        return 0;
    }

    // Create UART RX task
    BaseType_t rc = xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 4, &g_uart_manager.uart_task_handle);
    if (rc != pdPASS) {
        ESP_LOGE(LOG_UART, "Failed to create UART task: %d", rc);
        return -1;
    }

    ESP_LOGI(LOG_UART, "UART RX task started");
    return 0;
}

void dev_uart_stop_task(void) {
    if (g_uart_manager.uart_task_handle != NULL) {
        vTaskDelete(g_uart_manager.uart_task_handle);
        g_uart_manager.uart_task_handle = NULL;
        ESP_LOGI(LOG_UART, "UART task stopped");
    }
}

void dev_uart_deinit(void) {
    dev_uart_stop_task();

    if (g_uart_manager.event_queue != NULL) {
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
    }

    if (g_uart_manager.report_mutex != NULL) {
        vSemaphoreDelete(g_uart_manager.report_mutex);
        g_uart_manager.report_mutex = NULL;
    }

    // Uninstall UART driver
    uart_driver_delete(UART_PORT_NUM);

    g_uart_manager.initialized = false;
    ESP_LOGI(LOG_UART, "UART deinitialized");
}

bool dev_uart_get_event(dev_uart_event_t* event) {
    if (!g_uart_manager.initialized || g_uart_manager.event_queue == NULL) {
        return false;
    }

    return xQueueReceive(g_uart_manager.event_queue, event, 0) == pdTRUE;
}

void dev_uart_process_events(void) {
    if (!g_uart_manager.initialized || g_uart_manager.event_queue == NULL) {
        return;
    }

    dev_uart_event_t event;
    while (dev_uart_get_event(&event)) {
        // Update HID report with mutex protection
        if (xSemaphoreTake(g_uart_manager.report_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (pro2_hid_report != NULL) {
                // Process event based on type
                switch (event.type) {
                    case UART_EVENT_BUTTON:
                        if (event.data.button.button_id < BUTTON_MAP_SIZE) {
                            pro2_set_button(pro2_hid_report,
                                          button_map[event.data.button.button_id],
                                          event.data.button.pressed);
                            ESP_LOGD(LOG_UART, "Processed button event: id=%d, pressed=%d",
                                     event.data.button.button_id, event.data.button.pressed);
                        } else {
                            ESP_LOGW(LOG_UART, "Invalid button ID: %d", event.data.button.button_id);
                        }
                        break;

                    case UART_EVENT_STICK:
                        if (event.data.stick.stick_id == 0) {
                            pro2_set_left_stick(pro2_hid_report,
                                              event.data.stick.x,
                                              event.data.stick.y);
                            ESP_LOGD(LOG_UART, "Processed left stick: x=0x%03X, y=0x%03X",
                                     event.data.stick.x, event.data.stick.y);
                        } else if (event.data.stick.stick_id == 1) {
                            pro2_set_right_stick(pro2_hid_report,
                                               event.data.stick.x,
                                               event.data.stick.y);
                            ESP_LOGD(LOG_UART, "Processed right stick: x=0x%03X, y=0x%03X",
                                     event.data.stick.x, event.data.stick.y);
                        } else {
                            ESP_LOGW(LOG_UART, "Invalid stick ID: %d", event.data.stick.stick_id);
                        }
                        break;

                    case UART_EVENT_UNKNOWN:
                    default:
                        ESP_LOGD(LOG_UART, "Unknown event type: %d", event.type);
                        break;
                }
            }
            xSemaphoreGive(g_uart_manager.report_mutex);
        }
    }
}

int dev_uart_send_data(const uint8_t* data, size_t len) {
    if (!g_uart_manager.initialized) {
        return -1;
    }

    int bytes_written = uart_write_bytes(UART_PORT_NUM, (const char*)data, len);
    if (bytes_written < 0) {
        ESP_LOGE(LOG_UART, "Failed to write UART data");
        return -1;
    }

    return bytes_written;
}

// Function to update HID report from button event (to be called from hid_task)
void uart_update_hid_report_from_button(uint8_t button_id, bool pressed) {
    if (button_id >= BUTTON_MAP_SIZE) {
        ESP_LOGW(LOG_UART, "Invalid button ID: %d", button_id);
        return;
    }

    if (xSemaphoreTake(g_uart_manager.report_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (pro2_hid_report != NULL) {
            pro2_set_button(pro2_hid_report, button_map[button_id], pressed);
            ESP_LOGD(LOG_UART, "Updated button %d (%s) to %s",
                     button_id,
                     button_id < BUTTON_MAP_SIZE ? "valid" : "invalid",
                     pressed ? "pressed" : "released");
        }
        xSemaphoreGive(g_uart_manager.report_mutex);
    }
}

// Function to update HID report from stick event (to be called from hid_task)
void uart_update_hid_report_from_stick(uint8_t stick_id, uint16_t x, uint16_t y) {
    if (stick_id >= 2) {
        ESP_LOGW(LOG_UART, "Invalid stick ID: %d", stick_id);
        return;
    }

    if (xSemaphoreTake(g_uart_manager.report_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (pro2_hid_report != NULL) {
            if (stick_id == 0) {
                pro2_set_left_stick(pro2_hid_report, x, y);
                ESP_LOGD(LOG_UART, "Updated left stick: x=0x%03X, y=0x%03X", x, y);
            } else {
                pro2_set_right_stick(pro2_hid_report, x, y);
                ESP_LOGD(LOG_UART, "Updated right stick: x=0x%03X, y=0x%03X", x, y);
            }
        }
        xSemaphoreGive(g_uart_manager.report_mutex);
    }
}