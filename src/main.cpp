#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h"

// KHỞI TẠO OPJECT
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ I2C có thể là 0x27 hoặc 0x3F

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Cấu hình Keypad 4x3
const byte ROWS = 4; 
const byte COLS = 3; 
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {KEYPAD_R1, KEYPAD_R2, KEYPAD_R3, KEYPAD_R4};
byte colPins[COLS] = {KEYPAD_C1, KEYPAD_C2, KEYPAD_C3};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// input
String inputPassword = "";
unsigned long doorOpenTimestamp = 0;
bool isDoorOpen = false;
bool isAlarming = false;

// Các hàm bổ sung
void beep(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    delay(duration);
  }
}

void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, message, "");
  }
}

void unlockDoor(String userMethod) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cua mo!");
  lcd.setCursor(0, 1);
  lcd.print("Xin chao...");
  
  beep(1, 500); // Kòi báo
  digitalWrite(RELAY_PIN, RELAY_ON);
  
  sendTelegramMessage("Cửa đã được mở bằng: " + userMethod);
  
  delay(DOOR_OPEN_TIME_MS); // Giữ chốt mở trong 3s
  digitalWrite(RELAY_PIN, RELAY_OFF);
  
  lcd.clear();
  lcd.print("Moi nhap ma/The");
}

// SETUP
void setup() {
  Serial.begin(115200);
  
  // Khởi tạo chân GPIO
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  
  digitalWrite(RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);

  // Khởi tạo SPI và RFID
  SPI.begin();
  mfrc522.PCD_Init();

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dang ket noi WiFi");

  // Kết nối WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // bảo mật cho Telegram
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  lcd.clear();
  lcd.print("WiFi OK!");
  sendTelegramMessage("Hệ thống cửa thông minh đã khởi động!");
  delay(1000);
  
  lcd.clear();
  lcd.print("Moi nhap ma/The");
}


void loop() {
  // KIỂM TRA TRẠNG THÁI CẢM BIẾN CỬA
  // Cảm biến từ MC-38: Mở cửa = HIGH, Đóng cửa = LOW
  int doorState = digitalRead(DOOR_SENSOR_PIN);
  
  if (doorState == HIGH) { // Cửa đang mở
    if (!isDoorOpen) {
      isDoorOpen = true;
      doorOpenTimestamp = millis(); // Ghi nhận thời điểm cửa mở
    } else {
      // Nếu cửa mở quá 10s
      if (millis() - doorOpenTimestamp > DOOR_ALARM_DELAY_MS) {
        digitalWrite(BUZZER_PIN, HIGH); // Hú còi
        if (!isAlarming) {
          sendTelegramMessage("CẢNH BÁO: Cửa chưa được đóng!");
          isAlarming = true;
        }
      }
    }
  } 
  else { // Cửa đã đóng
    isDoorOpen = false;
    isAlarming = false;
    digitalWrite(BUZZER_PIN, LOW); // Tắt còi
  }

  // XỬ LÝ NHẬP KEYPAD
  char key = keypad.getKey();
  if (key) {
    beep(1, 50);       //phản hồi phím
    if (key == '*') {
      inputPassword = ""; 
      lcd.clear();
      lcd.print("Moi nhap ma/The");
    } else if (key == '#') {
      // Xác nhận nhập mật khẩu
      if (inputPassword == MASTER_PASSWORD) {
        unlockDoor("Mật khẩu bàn phím");
      } else {
        lcd.clear();
        lcd.print("Sai mat khau!");
        beep(3, 100);
        delay(1000);
        lcd.clear();
        lcd.print("Moi nhap ma/The");
      }
      inputPassword = "";
    } else {
      // Lưu phím vào chuỗi
      inputPassword += key;
      lcd.setCursor(0, 1);
      lcd.print("Mat khau: ");
      for(int i=0; i<inputPassword.length(); i++) lcd.print("*");
    }
  }


  // XỬ LÝ QUẸT THẺ RFID
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
      uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidString.toUpperCase();
    Serial.println("Quẹt thẻ UID: " + uidString);
    beep(1, 100);

    // Kiểm tra UID
    if (uidString == "A1B2C3D4") {
      unlockDoor("Thẻ từ (Bố)");
    } else if (uidString == "E5F6A7B8") {
      unlockDoor("Thẻ từ (Mẹ)");
    } else {
      lcd.clear();
      lcd.print("The khong hop le");
      beep(3, 100);
      delay(1000);
      lcd.clear();
      lcd.print("Moi nhap ma/The");
    }
    mfrc522.PICC_HaltA(); // Tạm dừng đọc thẻ
  }
}
