#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// CẤU HÌNH MẠNG
#define WIFI_SSID           "Yanne"
#define WIFI_PASSWORD       "09102105"
#define BOT_TOKEN           "_"   // Hiện tại chưa cập nhật
#define CHAT_ID             "ID"  // Hiện tại chưa cập nhật

// define pin
// Thẻ từ RC525
#define RFID_SS_PIN         5
#define RFID_RST_PIN        4

// Keypad 4x3
#define KEYPAD_R1           13
#define KEYPAD_R2           12
#define KEYPAD_R3           14
#define KEYPAD_R4           27
#define KEYPAD_C1           26
#define KEYPAD_C2           25
#define KEYPAD_C3           33

// Ngoại vi khác
#define DOOR_SENSOR_PIN     15
#define RELAY_PIN           17
#define BUZZER_PIN          2

// CẤU HÌNH LOGIC
#define RELAY_ON            HIGH 
#define RELAY_OFF           LOW

#define DOOR_OPEN_TIME_MS   3000  // Thời gian mở chốt
#define DOOR_ALARM_DELAY_MS 10000 // Thời gian trước khi báo động (10 giây)

// Mật khẩu tĩnh
const String MASTER_PASSWORD = "123"; 

#endif
