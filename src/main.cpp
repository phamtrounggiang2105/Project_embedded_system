#include "config.h"
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Preferences.h>
#include <BlynkSimpleEsp32.h>

// ================= KHỞI TẠO ĐỐI TƯỢNG =================
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_SH1106G display(128, 64, &Wire, -1);
Preferences preferences;

// Cấu hình Keypad 3x4
static const uint8_t ROWS = 4;
static const uint8_t COLS = 3;
static char keys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};
static uint8_t rowPins[ROWS] = {KEYPAD_R1, KEYPAD_R2, KEYPAD_R3, KEYPAD_R4};
static uint8_t colPins[COLS] = {KEYPAD_C1, KEYPAD_C2, KEYPAD_C3};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================= QUẢN LÝ TRẠNG THÁI (Ganssle Guideline) =================
// Gom các biến toàn cục vào một struct để dễ quản lý bộ nhớ và tránh phân mảnh
typedef struct {
    char current_input[MAX_PASSWORD_LEN + 1];
    uint8_t input_length;
    String master_password;
    
    uint8_t wrong_attempts;
    bool is_locked_out;
    uint32_t lockout_start_time;
    
    bool is_door_unlocked;
    uint32_t unlock_start_time;
    
    bool update_display;
} SystemState;

static SystemState sys_state = {
    "", 0, "123456", 0, false, 0, false, 0, true
};

// ================= HÀM HIỂN THỊ OLED =================
static void refreshDisplay(const char* line1, const char* line2) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Căn giữa dòng 1 (Tiêu đề)
    display.setCursor(0, 10);
    display.println(line1);
    
    // Hiển thị dòng 2 (Nội dung / Mật khẩu)
    display.setTextSize(2);
    display.setCursor(0, 35);
    display.println(line2);
    
    display.display();
}

static void renderInputScreen() {
    char hidden_pass[MAX_PASSWORD_LEN + 1] = {0};
    for (uint8_t i = 0; i < sys_state.input_length; i++) {
        hidden_pass[i] = '*';
    }
    refreshDisplay("Nhap Mat Khau:", hidden_pass);
}

// ================= HÀM TIỆN ÍCH (Không dùng delay block luồng) =================
static void beep(uint8_t times) {
    // Dùng vòng lặp siêu nhỏ không ảnh hưởng lớn đến luồng
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(80); 
        digitalWrite(BUZZER_PIN, LOW);
        if (times > 1) delay(80);
    }
}

static void unlockDoor() {
    sys_state.is_door_unlocked = true;
    sys_state.unlock_start_time = millis();
    sys_state.wrong_attempts = 0; // Reset số lần sai
    
    digitalWrite(RELAY_PIN, HIGH); // Giật chốt XG07
    refreshDisplay("THONG BAO", "XIN CHAO!");
    beep(1);
}

static void triggerLockout() {
    sys_state.is_locked_out = true;
    sys_state.lockout_start_time = millis();
    refreshDisplay("CANH BAO!", "KHOA 30S");
    // Hú còi dài
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000); 
    digitalWrite(BUZZER_PIN, LOW);
}

static void checkWrongAttempts() {
    sys_state.wrong_attempts++;
    sys_state.input_length = 0; // Xóa mật khẩu đang nhập
    memset(sys_state.current_input, 0, sizeof(sys_state.current_input));
    
    refreshDisplay("LOI!", "SAI MA/THE");
    beep(3);
    
    if (sys_state.wrong_attempts >= MAX_WRONG_ATTEMPTS) {
        triggerLockout();
    } else {
        delay(1000); // Tạm dừng 1s để người dùng đọc thông báo lỗi
        sys_state.update_display = true; // Yêu cầu vẽ lại màn hình chờ
    }
}

// ================= BLYNK IOT (Mở cửa & Đổi mật khẩu từ xa) =================
// Nút nhấn mở cửa (Virtual Pin V0)
BLYNK_WRITE(V0) {
    if (param.asInt() == 1 && !sys_state.is_locked_out) {
        unlockDoor();
    }
}

// Terminal đổi mật khẩu (Virtual Pin V1)
BLYNK_WRITE(V1) {
    String new_pass = param.asStr();
    if (new_pass.length() > 0 && new_pass.length() <= MAX_PASSWORD_LEN) {
        sys_state.master_password = new_pass;
        preferences.putString("pwd", new_pass);
        Blynk.virtualWrite(V1, "Doi mat khau thanh cong!");
    } else {
        Blynk.virtualWrite(V1, "Loi: Mat khau 1-6 ky tu.");
    }
}

// ================= SETUP HỆ THỐNG =================
void setup() {
    // 1. Cấu hình phần cứng
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Đảm bảo chốt đang khóa
    digitalWrite(BUZZER_PIN, LOW);

    // 2. Khởi tạo OLED
    display.begin(OLED_I2C_ADDRESS, true);
    refreshDisplay("He Thong", "Khoi dong...");

    // 3. Khởi tạo SPI & RFID
    SPI.begin();
    rfid.PCD_Init();

    // 4. Đọc mật khẩu từ EEPROM (Preferences)
    preferences.begin("smartlock", false);
    sys_state.master_password = preferences.getString("pwd", "123456");

    // 5. Kết nối Blynk & WiFi
    refreshDisplay("WiFi", "Ket noi...");
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
    
    sys_state.update_display = true;
    beep(2); // Báo hiệu sẵn sàng
}

// ================= VÒNG LẶP CHÍNH =================
void loop() {
    Blynk.run(); // Duy trì kết nối IoT (Bắt buộc phải chạy liên tục)
    uint32_t current_time = millis();

    // --- XỬ LÝ TRẠNG THÁI PHẠT (ANTI-BRUTE FORCE) ---
    if (sys_state.is_locked_out) {
        if (current_time - sys_state.lockout_start_time >= LOCKOUT_TIME_MS) {
            sys_state.is_locked_out = false;
            sys_state.wrong_attempts = 0;
            sys_state.update_display = true;
        } else {
            // Cập nhật đồng hồ đếm ngược mỗi giây
            static uint32_t last_tick = 0;
            if (current_time - last_tick >= 1000) {
                last_tick = current_time;
                uint8_t remain = (LOCKOUT_TIME_MS - (current_time - sys_state.lockout_start_time)) / 1000;
                char buf[10];
                sprintf(buf, "Cho %ds", remain);
                refreshDisplay("DANG KHOA!", buf);
            }
        }
        return; // Chặn toàn bộ thao tác nhập liệu bên dưới
    }

    // --- XỬ LÝ TRẠNG THÁI MỞ CỬA (NON-BLOCKING RELAY) ---
    if (sys_state.is_door_unlocked) {
        if (current_time - sys_state.unlock_start_time >= UNLOCK_TIME_MS) {
            digitalWrite(RELAY_PIN, LOW); // Nhả Relay để lò xo đóng chốt
            sys_state.is_door_unlocked = false;
            sys_state.update_display = true;
        }
        return; // Đang mở cửa thì không cần nhận thêm phím
    }

    // --- CẬP NHẬT MÀN HÌNH CHỜ ---
    if (sys_state.update_display) {
        if (sys_state.input_length == 0) {
            refreshDisplay("Smart Lock", "Moi quet/nhap");
        } else {
            renderInputScreen();
        }
        sys_state.update_display = false;
    }

    // --- XỬ LÝ KEYPAD ---
    char key = keypad.getKey();
    if (key) {
        beep(1);
        if (key == '*') {
            // Nút xóa lùi
            if (sys_state.input_length > 0) {
                sys_state.input_length--;
                sys_state.current_input[sys_state.input_length] = '\0';
                sys_state.update_display = true;
            }
        } else if (key == '#') {
            // Nút xác nhận
            if (String(sys_state.current_input) == sys_state.master_password) {
                unlockDoor();
            } else {
                checkWrongAttempts();
            }
            // Reset mảng nhập
            sys_state.input_length = 0;
            memset(sys_state.current_input, 0, sizeof(sys_state.current_input));
            sys_state.update_display = true;
        } else {
            // Nhập số (Kiểm tra chống tràn mảng)
            if (sys_state.input_length < MAX_PASSWORD_LEN) {
                sys_state.current_input[sys_state.input_length] = key;
                sys_state.input_length++;
                sys_state.current_input[sys_state.input_length] = '\0';
                sys_state.update_display = true;
            }
        }
    }

    // --- XỬ LÝ RFID ---
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        String uid_str = "";
        for (uint8_t i = 0; i < rfid.uid.size; i++) {
            uid_str += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
            uid_str += String(rfid.uid.uidByte[i], HEX);
        }
        uid_str.toUpperCase();
        rfid.PICC_HaltA(); // Tạm dừng thẻ

        // Thay mã UID này bằng mã thẻ của bạn (Đọc qua Serial trước đó)
        if (uid_str == "A1B2C3D4" || uid_str == "E5F6A7B8") {
            unlockDoor();
        } else {
            checkWrongAttempts();
        }
    }
}