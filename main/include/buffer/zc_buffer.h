#ifndef  ZC_BUFFER_H
#define ZC_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_CACHE_LINE_SIZE       32

typedef struct {
  struct {
    volatile uint32_t head;         // writer index
    uint32_t watermark;             // Flow control watermark
    // padding to ensure the two fields are not in the same cache line
    uint8_t _padding[ESP32_CACHE_LINE_SIZE - sizeof(uint32_t) * 2];
  } tx __attribute__((aligned(ESP32_CACHE_LINE_SIZE)));

  struct {
    volatile uint32_t tail;         // reader index
    uint32_t last_commit;           // last committed index (debug)
    // same padding as above
    uint8_t _padding[ESP32_CACHE_LINE_SIZE - sizeof(uint32_t) * 2];
  } rx __attribute__((aligned(ESP32_CACHE_LINE_SIZE)));

  uint8_t *buffer_base;
  uint32_t capacity;
} zc_ringbuf_t;

// Ensure the two fields are not in the same cache line (offset difference must be ≥ 32)
_Static_assert(offsetof(zc_ringbuf_t, rx)
  - offsetof(zc_ringbuf_t, tx) >= ESP32_CACHE_LINE_SIZE,
  "False Sharing protection failed: head/tail in same cache line");

// Verify overall structure alignment (to prevent DMA alignment issues)
_Static_assert(sizeof(zc_ringbuf_t) % ESP32_CACHE_LINE_SIZE == 0,
  "zc_ringbuf_t size must be multiple of cache line");

/**
 * @brief producer acquires contiguous writable space.
 */
uint32_t zc_reserve(zc_ringbuf_t *rb, uint8_t **ptr);

/**
 * @brief producer commits 
 */
void zc_commit(zc_ringbuf_t *rb, uint32_t len);

/**
 * @brief consumer peeks at readable data (without consuming it).
 */
uint32_t zc_peek(zc_ringbuf_t *rb, uint8_t **ptr);

/**
 * @brief consumer confirms consumption (releases space)
 */
void zc_consume(zc_ringbuf_t *rb, uint32_t len);

/**
 * @brief consumer reads a single byte (for streaming parsing).
 */
bool zc_read_byte(zc_ringbuf_t *rb, uint8_t *out);

/**
 * @brief consumer batch peek across wrap-around boundary (without consuming).
 *
 * @param rb          Ring buffer
 * @param wanted_len  Maximum bytes to peek
 * @param head_ptr    Output pointer to first contiguous segment
 * @param head_len    Output length of first contiguous segment
 * @param wrap_ptr    Output pointer to second contiguous segment (wrap-around), may be NULL
 * @param wrap_len    Output length of second contiguous segment, may be NULL
 * @return Total bytes available to peek (head_len + wrap_len)
 */
uint32_t zc_peek_bulk(zc_ringbuf_t *rb, uint32_t wanted_len,
                      uint8_t **head_ptr, uint32_t *head_len,
                      uint8_t **wrap_ptr, uint32_t *wrap_len);

/**
 * @brief consumer batch read
 */
uint32_t zc_read_bulk(zc_ringbuf_t *rb, uint8_t *dst, uint32_t len);

/**
 * @brief zero copy ring buffer init
 */
esp_err_t zc_init(zc_ringbuf_t *rb, uint8_t *buffer, uint32_t capacity, uint32_t watermark);

/**
 * @brief reset buffer
 */
void zc_reset(zc_ringbuf_t *rb);
  
#ifdef __cplusplus
}
#endif

#endif // ZC_BUFFER_H