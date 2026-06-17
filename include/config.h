#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <Arduino.h>

// THÔNG SỐ KẾT NỐI
#define BLYNK_TEMPLATE_ID   "TMPL_XXXXX"       // ID web Blynk
#define BLYNK_TEMPLATE_NAME "Smart Door Lock"
#define BLYNK_AUTH_TOKEN    "Token_Blynk"

#define WIFI_SSID           "Yanne"
#define WIFI_PASS           "09102105"

// ĐỊNH NGHĨA CHÂN GPIO
// SPI & RFID RC522 (SCK=18, MISO=19, MOSI=23)
#define RFID_SS_PIN         5
#define RFID_RST_PIN        4

// Keypad 3x4
#define KEYPAD_R1           13
#define KEYPAD_R2           12
#define KEYPAD_R3           14
#define KEYPAD_R4           27
#define KEYPAD_C1           26
#define KEYPAD_C2           25
#define KEYPAD_C3           33

// Cơ cấu chấp hành & Cảnh báo
#define RELAY_PIN           17
#define BUZZER_PIN          15

// THÔNG SỐ HỆ THỐNG 
#define MAX_PASSWORD_LEN    6       // Độ dài mật khẩu tối đa
#define MAX_WRONG_ATTEMPTS  3       // Số lần sai tối đa trước khi khóa
#define LOCKOUT_TIME_MS     30000   // Thời gian phạt (30 giây)
#define UNLOCK_TIME_MS      4000    // Thời gian mở chốt (4 giây)

#define OLED_I2C_ADDRESS    0x3C    // Địa chỉ I2C của màn hình

#endif // CONFIG_H