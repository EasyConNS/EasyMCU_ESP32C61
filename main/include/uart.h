#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// UART Configuration
#define UART_PORT_NUM           UART_NUM_1
#define UART_BAUD_RATE          115200
#define UART_RX_BUFFER_SIZE     1024
#define UART_TX_BUFFER_SIZE     1024
#define UART_RX_PIN             4   // GPIO4 for RX
#define UART_TX_PIN             5   // GPIO5 for TX

// Button event for queue communication
typedef struct {
    uint8_t button_id;      // Button ID (mapping to pro2_btns)
    bool pressed;           // Pressed (true) or released (false)
} button_event_t;

// Stick event for queue communication
typedef struct {
    uint8_t stick_id;       // 0 = left stick, 1 = right stick
    uint16_t x;             // X coordinate (12-bit, 0-0xFFF)
    uint16_t y;             // Y coordinate (12-bit, 0-0xFFF)
} stick_event_t;

// Union for event data
typedef union {
    button_event_t button;
    stick_event_t stick;
} dev_uart_event_data_t;

// UART event types
typedef enum {
    UART_EVENT_BUTTON,
    UART_EVENT_STICK,
    UART_EVENT_UNKNOWN
} dev_uart_event_type_t;

// Complete UART event with type
typedef struct {
    dev_uart_event_type_t type; // Event type
    dev_uart_event_data_t data; // Event data
} dev_uart_event_t;

// UART manager structure
typedef struct {
    QueueHandle_t event_queue;      // Queue for UART events
    SemaphoreHandle_t report_mutex; // Mutex for HID report access
    TaskHandle_t uart_task_handle;  // UART task handle
    bool initialized;               // UART initialized flag
} dev_uart_manager_t;

// Global UART manager
extern dev_uart_manager_t g_uart_manager;

/**
 * @brief Initialize UART driver and create UART task
 * @return 0 on success, negative error code on failure
 */
int dev_uart_init(void);

/**
 * @brief Deinitialize UART driver and free resources
 */
void dev_uart_deinit(void);

/**
 * @brief Start UART task for receiving data
 * @return 0 on success, negative error code on failure
 */
int dev_uart_start_task(void);

/**
 * @brief Stop UART task
 */
void dev_uart_stop_task(void);

/**
 * @brief Get an event from UART queue (non-blocking)
 * @param event Output event if available
 * @return true if event was retrieved, false if queue is empty
 */
bool dev_uart_get_event(dev_uart_event_t* event);

/**
 * @brief Process events from UART queue and update HID report
 * This function should be called periodically from the HID task
 */
void dev_uart_process_events(void);

/**
 * @brief Parse UART data frame
 * @param data Raw UART data
 * @param len Length of data
 * @param event Output event (if valid frame)
 * @return Event type if valid frame, UART_EVENT_UNKNOWN otherwise
 */
dev_uart_event_type_t dev_uart_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event);

/**
 * @brief Send data via UART (for debugging/response)
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, negative on error
 */
int dev_uart_send_data(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _UART_H_