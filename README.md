TODO

esp-idf version: v5.5.2+ (test pass, don't use v5.5.1)

ESP32-C61 CONFIG_BT_NIMBLE_TRANSPORT_EVT_SIZE=70 -> 257
fix: hci init error, assert failed: ble_hs_init ble_hs.c:957 (rc == 0)