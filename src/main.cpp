// Thư viện
#include <LiquidCrystal.h>    // màn hình LCD
#include <EEPROM.h>           // bộ nhớ EEPROM
#include <SPI.h>              // spi cho module RFID
#include <MFRC522.h>         
#include "Adafruit_Keypad.h"  // bàn phím 3x4
#include <string.h>         
#include <avr/wdt.h>        


// KHAI BÁO CHÂN RFID
#define SS_PIN  10   // Chân Slave Select
#define RST_PIN  9   // Chân Reset
#define BUTTON   0   // Chân cho nút mở cửa

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Khởi tạo đối tượng đọc thẻ RFID

#define MAX_BUFF 50              // Số  thẻ RFID max
uint32_t ID_CARD_PASS[MAX_BUFF]; // Mảng lưu ID 
uint32_t uidDecTemp = 0;         // Biến tạm để ghép từng byte UID
uint32_t uidDec    = 0;          // ID thẻ vừa quét


byte bCounter, readBit;
unsigned long ticketNumber;


// KHAI BÁO CHÂN LCD 16x2
// Lần lượt ứng với các chân: RS, EN, D4, D5, D6, D7
LiquidCrystal My_LCD(8, 7, 6, 5, 4, 3);


// KHAI BÁO CHÂN KEYPAD 3x4
#define KEYPAD_PID3845 
#define C3    A1
#define C2    A2
#define C1    A3
#define R4    A4
#define R3    A5
#define R2    A6
#define R1    A7
#include "keypad_config.h"  // File config bàn phím
Adafruit_Keypad customKeypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);



// KHAI BÁO CHÂN KHÓA ĐIỆN VÀ CÒI BÁO
#define KHOA     2   // Chân điều khiển relay/khóa điện từ
#define COI      A0  // Chân điều khiển còi buzzer
#define KHOA_ON  1   // Giá trị mở khóa
#define KHOA_OFF 0   // Giá trị đóng khóa
#define COI_ON   1   // Còi bật
#define COI_OFF  0   // Còi tắt


// TRẠNG THÁI HOẠT ĐỘNG
typedef enum
{
    IDLE_MODE,          // Chờ thẻ hoặc mật khẩu
    WAIT_CLOSE,         // Cửa đang mở (đợi đóng tự động/nhập lệnh)
    ADDCARD_MODE,       // Cđ thêm thẻ mới vào hệ thống
    REMOVECARD_MODE,    // Cđ xóa thẻ khỏi hệ thống
    CHANGE_MASTER_CARD, // Cđ đổi thẻ master
    CHANGE_PASSWORD,    // Cđ đổi mật khẩu
    LOCK_1MIN           // Khóa hệ thống 1 phút do nhập sai nhiều lần
} MODE;

uint8_t modeRun = IDLE_MODE; // Trạng thái hiện tại của system


// LƯU MẬT KHẨU VÀ TRẠNG THÁI PHÍM
char password[7]       = "111111"; // Mật khẩu hiện tại
char passwordEnter[7]  = {0};      // Mật khẩu nhập ở IDLE_MODE
char passwordCard[6]   = {0};    
char passwordChange1[7] = {0};     // Mật khẩu mới lần 1
char passwordChange2[7] = {0};     // Xác nhận mk mới

uint8_t count      = 0;  // Đếm bit đã nhập
uint8_t countError = 0;  // Số lần nhập sai
uint8_t stateLock  = 0;  
uint8_t timeClose  = 1;  // Thời gian tự động đóng cửa

uint32_t timeMillisAUTO = 0; // Mốc thời gian để đếm tự động đóng cửa
uint32_t timeMillisRST  = 0; // Mốc thời gian dùng cho reset (dự phòng)

keypadEvent e; // Biến lưu sự kiện phím vừa xảy ra



// TRẠNG THÁI CON CỦA CÁC CHỨC NĂNG
// thêm thẻ
typedef enum { CHECK_CARD, ADD_CARD     } ADDCARD;

// xóa thẻ
typedef enum { CHECK_CARD_RM, REMOVE_CARD_RM } REMOVECARD;

// đổi mật khẩu: nhập lần 1, xác nhận
typedef enum { ENTER_PASS_1, ENTER_PASS_2 } CHANGEPASS;

uint8_t removeCardState = CHECK_CARD;   // Trạng thái chức năng xóa thẻ
uint8_t addCardState    = CHECK_CARD;   // Trạng thái chức năng thêm thẻ
uint8_t changePassword  = ENTER_PASS_1; // Trạng thái chức năng đổi mật khẩu

uint8_t countCheck  = 1; // Chỉ số duyệt mảng ID_CARD_PASS khi tìm/thêm/xóa thẻ
uint8_t allowAccess = 0; // Cờ xác nhận quyền truy cập
static uint8_t k    = 0; 


// KHAI BÁO HÀM
void    checkRFID(void);
void    removeCard(void);
void    addCard(void);
void    insertCardMaster(void);
void    changePasswordFunc(void);
void    checkButton(void);
void    handle_button(void);
void    release_button(void);
void    COI_beep(void);
uint8_t readRFIDinBUFFER(void);
void    checkBackHome(void);
void    autoReturn(void);
void    saveIDtoEEP(uint32_t valueID, uint8_t positionID);
void    saveIDtoBUFF(void);
void    clearIDinEEP(uint8_t positionID);
void    savePWtoEEP(uint32_t valuePASS);
void    savePASStoBUFF(void);
uint32_t convertPassToNumber(void);


// KHỞI TẠO
void setup()
{
    Serial.begin(9600);   // Khởi động Serial
    My_LCD.begin(16, 2);  // Khởi động LCD
    SPI.begin();          // Khởi động bus SPI (cho RFID)
    mfrc522.PCD_Init();   // Khởi động module RFID

    wdt_enable(WDTO_8S);

    customKeypad.begin();              // Khởi động bàn phím
    pinMode(KHOA, OUTPUT);             // Chân khóa
    pinMode(BUTTON, INPUT);            // Chân nút bấm
    digitalWrite(KHOA, KHOA_OFF);  
    pinMode(COI, OUTPUT);              // Chân còi
    digitalWrite(COI, COI_OFF);       // Tắt còi
    My_LCD.clear();

    // Kiểm tra EEPROM có dữ liệu hợp lệ
    if (EEPROM.read(251) != 0)
    {
        // xóa dữ liệu thẻ nếu lần đầu chạy hoăcj EEPRM bị lỗi
        for (uint8_t i = 0; i <= 250; i++)
        {
            EEPROM.write(i, 0);
        }
        EEPROM.write(251, 0);

        // Xóa mật khẩu lưu trong EEPROM
        EEPROM.write(252, 0);
        EEPROM.write(253, 0);
        EEPROM.write(254, 0);
    }
    else
    {
        // nạp danh sách thẻ từ EEPROM lên mảng RAM
        saveIDtoBUFF();
    }

    // Nạp mật khẩu từ EEPROM lên password trong RAM
    savePASStoBUFF();
    Serial.println("hello ae");
}

int countt = 0;


// VÒNG LẶP CHÍNH
void loop()
{
    wdt_reset();     
    checkButton();   // Xử lý bàn phím và logic chính theo trạng thái
    checkRFID();     // Xử lý thẻ RFID theo trạng thái hiện tại
    handle_button(); // Xử lý nút nhấn mở cửa từ bên trong
}



// MỞ CỬA TỪ BÊN TRONG
uint8_t currentSst = 1, lastSst = 1;

// Khi nút được nhả ra: kêu còi báo hiệu, mở khóa 3s rồi đóng
void release_button()
{
    COI_beep();
    countError = 0;
    digitalWrite(KHOA, KHOA_ON);   // Mở khóa
    delay(3000);                    // Giữ mở 3 giây
    digitalWrite(KHOA, KHOA_OFF);  // Đóng khóa lại
}

// Đọc trạng thái nút
void handle_button()
{
    static uint32_t t_debound_buton;
    if (millis() - t_debound_buton >= 40)
    {
        t_debound_buton = millis();
        currentSst = digitalRead(BUTTON);

        if (currentSst == 0) // Đang giữ nút
        {
            if (lastSst == 1)
                lastSst = currentSst;
        }
        else // Nhả nút
        {
            if (lastSst == 0)
            {
                lastSst = currentSst;
                release_button(); // Thực hiện mở cửa khi phát hiện nhả nút
            }
        }
    }
}



// XỬ LÝ RFID THEO TRẠNG THÁI HỆ THỐNG
void checkRFID()
{
    switch (modeRun)
    {
        case IDLE_MODE:
            // Chế độ chờ: quét thẻ và kiểm tra trong danh sách hợp lệ
            readRFIDinBUFFER();
            break;

        case ADDCARD_MODE:
            // Chế độ thêm thẻ: chỉ cho phép nếu đã xác thực quyền truy cập
            if (allowAccess == 1)
                addCard();
            else
            {
                My_LCD.setCursor(3, 0);
                My_LCD.print("CANT ACCESS ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("            ");
                delay(1000);
                modeRun = IDLE_MODE;
            }
            checkBackHome(); // Cho phép nhấn # để về màn hình chính
            break;

        case REMOVECARD_MODE:
            // Chế độ xóa thẻ: chỉ cho phép nếu đã xác thực quyền truy cập
            if (allowAccess == 1)
                removeCard();
            else
            {
                My_LCD.setCursor(3, 0);
                My_LCD.print("CANT ACCESS ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("            ");
                delay(1000);
                modeRun = IDLE_MODE;
            }
            checkBackHome();
            break;

        case CHANGE_MASTER_CARD:
            // Chế độ đổi thẻ master: chỉ cho phép nếu đã xác thực quyền truy cập
            if (allowAccess == 1)
                insertCardMaster();
            else
            {
                My_LCD.setCursor(3, 0);
                My_LCD.print("CANT ACCESS ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("            ");
                delay(1000);
                modeRun = IDLE_MODE;
            }
            checkBackHome();
            break;

        case CHANGE_PASSWORD:
            // Chế độ đổi mật khẩu: chỉ cho phép nếu đã xác thực quyền truy cập
            if (allowAccess == 1)
                changePasswordFunc();
            else
            {
                My_LCD.setCursor(3, 0);
                My_LCD.print("CANT ACCESS ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("            ");
                delay(1000);
                modeRun = IDLE_MODE;
            }
            checkBackHome();
            break;

        case LOCK_1MIN:
            // Khóa hệ thống 1p nếu nhập sai quá 5 lần
            My_LCD.setCursor(2, 0);
            My_LCD.print("TRY AGAIN ");
            My_LCD.setCursor(0, 1);
            My_LCD.print("  IN 1 MINUTE");
            for (int i = 60; i >= 0; i--)
            {
                wdt_reset(); // Reset watchdog 
                My_LCD.clear();
                My_LCD.setCursor(2, 0);
                My_LCD.print("TRY AGAIN ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("  IN ");

                // Kêu còi trong 10 giây
                if (i > 50) analogWrite(COI, 500);
                else        analogWrite(COI, 0);

                if (i > 9)
                {
                    My_LCD.setCursor(5, 1);
                    My_LCD.print(i);
                    My_LCD.setCursor(7, 1);
                    My_LCD.print(" SECONDS");
                }
                else
                {
                    My_LCD.setCursor(6, 1);
                    My_LCD.print(i);
                    My_LCD.setCursor(7, 1);
                    My_LCD.print(" SECONDS");
                }
                delay(1000); 
            }
            countError = 0;           // Xóa bộ đếm lỗi
            modeRun = IDLE_MODE;      // Quay về trạng thái chờ
            My_LCD.clear();
            break;
    }
}



// XÓA THẺ RFID
void removeCard()
{
    uint8_t checkDup = 0;
    switch (removeCardState)
    {
        case CHECK_CARD_RM:
            // quét thẻ cần xóa, kiểm tra xem có trong danh sách
            countCheck = 1;
            My_LCD.setCursor(1, 0);
            My_LCD.print(" REMOVE CARD  ");
            My_LCD.setCursor(1, 1);
            My_LCD.print(" PLEASE INSERT  ");

            if (!mfrc522.PICC_IsNewCardPresent()) return; // Chưa có thẻ
            if (!mfrc522.PICC_ReadCardSerial())   return; // Khoong đọc được thẻ
            else COI_beep();

            // Ghép các byte UID thành 32-bit
            for (byte i = 0; i < mfrc522.uid.size; i++)
            {
                uidDecTemp = mfrc522.uid.uidByte[i];
                uidDec = uidDec * 256 + uidDecTemp;
            }
            mfrc522.PICC_HaltA(); // Dừng giao tiếp với thẻ

            // Tìm thẻ trong danh sách (bỏ qua thẻ master)
            for (uint16_t i = 1; i < MAX_BUFF; i++)
            {
                if (uidDec == ID_CARD_PASS[i])
                    checkDup = 1;
            }

            if (checkDup == 0) // Thẻ không tồn tại trong danh sách
            {
                My_LCD.setCursor(0, 0);
                My_LCD.print("  NOT EXIST   ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("                ");
                uidDec = 0;
                delay(1000);
                My_LCD.clear();
            }
            else // Thẻ tồn tại 
            {
                removeCardState = REMOVE_CARD_RM;  // Xóa thẻ
            }
            break;

        case REMOVE_CARD_RM:
            // Duyệt mảng tìm vị trí thẻ và xóa
            if (ID_CARD_PASS[countCheck] == uidDec)
            {
                ID_CARD_PASS[countCheck] = 0; // Xóa khỏi RAM
                My_LCD.setCursor(3, 0);
                My_LCD.clear();
                My_LCD.setCursor(1, 0);
                My_LCD.print("WAITING... ");
                delay(1000);

                clearIDinEEP(countCheck); // Xóa khỏi EEPROM
                saveIDtoBUFF();           // Nạp lại danh sách từ EEPROM → RAM

                My_LCD.clear();
                My_LCD.print("REMOVE ");
                My_LCD.setCursor(3, 1);
                My_LCD.print("SUCCESSFUL ");
                delay(2000);

                My_LCD.clear();
                uidDec = 0;
                removeCardState = CHECK_CARD; // Quay về bước đầu
                countCheck++;
            }
            else
            {
                // Chưa tìm thấy, duyệt vị trí tiếp theo
                countCheck++;
                if (countCheck > MAX_BUFF)
                    countCheck = 1;
            }
            break;
    }
}


// THÊM THẺ RFID MỚI
void addCard()
{
    uint8_t checkDup = 0;
    switch (addCardState)
    {
        case CHECK_CARD:
            // Yêu cầu quét thẻ mới, kiểm tra xem đã có trong danh sách
            My_LCD.setCursor(3, 0);
            My_LCD.print(" ADD CARD  ");
            My_LCD.setCursor(1, 1);
            My_LCD.print(" PLEASE INSERT  ");

            if (!mfrc522.PICC_IsNewCardPresent()) return;
            if (!mfrc522.PICC_ReadCardSerial())   return;
            else COI_beep();

            // Ghép UID thành 32-bit
            for (byte i = 0; i < mfrc522.uid.size; i++)
            {
                uidDecTemp = mfrc522.uid.uidByte[i];
                uidDec = uidDec * 256 + uidDecTemp;
            }
            mfrc522.PICC_HaltA();

            // Kiểm tra thẻ đã có trong danh sách
            for (uint16_t i = 0; i < MAX_BUFF; i++)
            {
                if (uidDec == ID_CARD_PASS[i])
                    checkDup = 1;
            }

            if (checkDup == 1) // Thẻ đã tồn tại
            {
                My_LCD.setCursor(0, 0);
                My_LCD.print("  ALREADY EXIST ");
                My_LCD.setCursor(0, 1);
                My_LCD.print("                ");
                uidDec = 0;
                delay(1000);
                My_LCD.clear();
            }
            else // Thẻ chưa có
            {
                addCardState = ADD_CARD;  // ghi
            }
            break;

        case ADD_CARD:
            // Tìm ô trống trong mảng và lưu thẻ
            if (ID_CARD_PASS[countCheck] == 0) // Ô trống
            {
                ID_CARD_PASS[countCheck] = uidDec; // Ghi vào RAM
                My_LCD.clear();
                My_LCD.setCursor(2, 0);
                My_LCD.print("WAITING... ");
                delay(1000);

                saveIDtoEEP(ID_CARD_PASS[countCheck], countCheck); // Ghi vào EEPROM
                saveIDtoBUFF(); // Đồng bộ lại RAM từ EEPROM

                My_LCD.clear();
                My_LCD.setCursor(3, 0);
                My_LCD.print("ADD ");
                My_LCD.setCursor(3, 1);
                My_LCD.print("SUCCESSFUL ");
                delay(2000);
                My_LCD.clear();

                uidDec = 0;
                addCardState = CHECK_CARD; // Quay về bước đầu
                countCheck++;
            }
            else
            {
                // Ô đã có thẻ
                countCheck++;
                if (countCheck > MAX_BUFF) // Hết bộ nhớ
                {
                    countCheck = 1;
                    My_LCD.setCursor(0, 0);
                    My_LCD.print("CANNOT INSERT ");
                    My_LCD.setCursor(0, 1);
                    My_LCD.print("FULL MEMORY    ");
                    delay(2000);
                    My_LCD.clear();
                    addCardState = CHECK_CARD;
                    modeRun = IDLE_MODE;
                }
            }
            break;
    }
}


// ĐỔI THẺ MASTER
// Quét thẻ mới và ghi vào vị trí 0 của mảng
// Thẻ master có quyền: quẹt xong vẫn mở cửa
void insertCardMaster()
{
    My_LCD.setCursor(0, 0);
    My_LCD.print(" CHANGE MASTER ");
    My_LCD.setCursor(0, 1);
    My_LCD.print(" PLEASE INSERT  ");

    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial())   return;
    else COI_beep();

    // Ghép UID thành 32-bit
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec * 256 + uidDecTemp;
    }
    mfrc522.PICC_HaltA();

    if (uidDec != 0)
    {
        ID_CARD_PASS[0] = uidDec;             // Ghi thẻ master mới vào vị trí 0 RAM
        saveIDtoEEP(ID_CARD_PASS[0], 0);      // Lưu vào EEPROM
        saveIDtoBUFF();                        // Đồng bộ lại danh sách

        My_LCD.clear();
        My_LCD.setCursor(3, 0);
        My_LCD.print("CHANGE ");
        My_LCD.setCursor(3, 1);
        My_LCD.print("SUCCESSFUL ");
        delay(2000);
        My_LCD.clear();
        modeRun = IDLE_MODE;
    }
}


// XÁC THỰC RFID CHẾ ĐỘ IDLE
// - Thẻ hợp lệ → mở cửa, chuyển sang WAIT_CLOSE
// - Thẻ master  → thêm allowAccess = 1
// - Thẻ không hợp lệ → tăng lỗi, quá 5 lần → LOCK_1MIN
// Trả về: 1 nếu thẻ hợp lệ, 0 nếu không hợp lệ
uint8_t readRFIDinBUFFER()
{
    uint8_t check_ID = 0;

    if (!mfrc522.PICC_IsNewCardPresent()) return 0;
    if (!mfrc522.PICC_ReadCardSerial())   return 0;
    else COI_beep();

    // Ghép UID thành 32-bit
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec * 256 + uidDecTemp;
    }
    mfrc522.PICC_HaltA();

    // So sánh UID vừa đọc với toàn bộ danh sách thẻ trong RAM
    for (uint16_t i = 0; i < MAX_BUFF; i++)
    {
        if (uidDec == ID_CARD_PASS[i])
        {
            check_ID++;
            // Nếu khớp thẻ master (vị trí 0) → cấp quyền truy cập
            if (uidDec == ID_CARD_PASS[0])
                allowAccess = 1;
        }
    }

    if (check_ID > 0) // Thẻ hợp lệ
    {
        k = 0;
        My_LCD.setCursor(1, 0);
        My_LCD.print("   WELCOME   ");
        My_LCD.setCursor(3, 1);
        My_LCD.print("VALID CARD");
        modeRun = WAIT_CLOSE;       // Chuyển sang trạng thái cửa mở
        timeMillisAUTO = millis();  // Bắt đầu đếm thời gian tự đóng
        digitalWrite(KHOA, KHOA_ON);
        count = 0;
        countError = 0;
        delay(200);
        return 1;
    }
    else // Thẻ không hợp lệ
    {
        k = 0;
        My_LCD.setCursor(1, 0);
        My_LCD.print("   WELCOME   ");
        My_LCD.setCursor(2, 1);
        My_LCD.print("INVALID CARD");
        countError++;
        if (countError >= 5)
            modeRun = LOCK_1MIN; // Khóa hệ thống 1 phút
        delay(1000);
        My_LCD.clear();
    }

    return 0;
}



// XỬ LÝ BÀN PHÍM
void checkButton()
{
    checkRFID(); // kiểm tra thẻ RFID trước khi xử lý phím

    switch (modeRun)
    {
        case IDLE_MODE:
            // Trạng thái chờ
            allowAccess = 0;                   // Xóa quyền truy cập
            digitalWrite(KHOA, KHOA_OFF);     // Đóng khóa
            My_LCD.setCursor(1, 0);
            if (k == 0)
            {
                My_LCD.print("   WELCOME   "); // in lên LCD
                k = 1;
            }

            // Đọc bàn phím
            customKeypad.tick();
            while (customKeypad.available())
            {
                e = customKeypad.read();
                COI_beep();
                if (e.bit.EVENT == KEY_JUST_RELEASED)
                {
                    if (count == 0) My_LCD.clear(); // Xóa màn hình trước khi hiển thị mật khẩu

                    if ((char)e.bit.KEY == '#') // Phím # → hủy, về màn hình chính
                    {
                        count = 0;
                        My_LCD.clear();
                        modeRun = IDLE_MODE;
                        k = 0;
                    }
                    else // Phím số → lưu vào bộ đệm, hiển thị dấu * trên LCD
                    {
                        k = 0;
                        passwordEnter[count] = (char)e.bit.KEY;
                        My_LCD.setCursor(count + 4, 1);
                        My_LCD.print('*');
                        count++;
                    }
                }
            }

            // nhập đủ 6 số → kiểm tra mật khẩu
            if (count > 5)
            {
                k = 0;
                passwordEnter[6] = '\0'; // Kết thúc
                My_LCD.setCursor(9, 1);
                My_LCD.print('*');
                delay(50);

                if (strcmp(passwordEnter, password) == 0) // Mật khẩu đúng
                {
                    allowAccess = 1;
                    My_LCD.setCursor(0, 0);
                    My_LCD.print("CORRECT PASSWORD");
                    timeMillisAUTO = millis();
                    modeRun = WAIT_CLOSE; // Mở cửa, chuyển sang chờ đóng
                }
                else // Mật khẩu sai
                {
                    delay(100);
                    My_LCD.clear();
                    My_LCD.setCursor(1, 0);
                    My_LCD.print("WRONG PASSWORD");
                    My_LCD.setCursor(1, 1);
                    My_LCD.print(4 - countError); // Hiển thị số lần còn lại
                    My_LCD.setCursor(3, 1);
                    My_LCD.print("TIMES LEFT");
                    delay(2000);
                    My_LCD.clear();
                    countError++;
                    if (countError >= 5)
                        modeRun = LOCK_1MIN; // Khóa 1 phút
                }
                count = 0;
            }
            break;

        case WAIT_CLOSE:
            // Trạng thái cửa đang m
            k = 0;
            countError = 0;
            digitalWrite(KHOA, KHOA_ON); // Giữ khóa mở

            // Hết thời gian tự động → đóng cửa, reset RFID, về IDLE
            if (millis() - timeMillisAUTO > timeClose * 1000)
            {
                timeMillisAUTO = millis();
                My_LCD.clear();
                delay(10);
                My_LCD.begin(16, 2); // Khởi động lại LCD
                delay(10);
                count = 0;
                mfrc522.PCD_Reset(); // Reset module RFID
                delay(10);
                mfrc522.PCD_Init();
                delay(10);
                modeRun = IDLE_MODE;
            }

            // Đọc phím đặc biệt từ bàn phím với chức năng đặc biệt
            // #### (đổi master card)
            // **** (thêm thẻ) 
            // 0000 (xóa thẻ)
            // 8888 (đổi mật khẩu)
            customKeypad.tick();
            while (customKeypad.available())
            {
                if (count == 0)
                {
                    My_LCD.setCursor(0, 1);
                    My_LCD.print("                "); // Xóa dòng 2
                }
                e = customKeypad.read();
                timeMillisAUTO = millis(); // Mỗi lần nhấn phím thì gia hạn thời gian mở cửa
                COI_beep();
                if (e.bit.EVENT == KEY_JUST_RELEASED)
                {
                    passwordCard[count] = (char)e.bit.KEY;
                    My_LCD.setCursor(count + 5, 1);
                    My_LCD.print((char)e.bit.KEY);
                    count++;
                }
            }

            // Đã nhập đủ 4 ký tự → so sánh và chuyển chế độ
            if (count > 3)
            {
                My_LCD.setCursor(8, 1);
                My_LCD.print((char)e.bit.KEY);
                delay(100);
                My_LCD.clear();

                if (strcmp(passwordCard, "####") == 0)
                {
                    modeRun = CHANGE_MASTER_CARD;
                    digitalWrite(KHOA, KHOA_OFF);
                }
                else if (strcmp(passwordCard, "****") == 0)
                {
                    modeRun = ADDCARD_MODE;
                    digitalWrite(KHOA, KHOA_OFF);
                }
                else if (strcmp(passwordCard, "0000") == 0)
                {
                    modeRun = REMOVECARD_MODE;
                    digitalWrite(KHOA, KHOA_OFF);
                }
                else if (strcmp(passwordCard, "8888") == 0)
                {
                    modeRun = CHANGE_PASSWORD;
                    digitalWrite(KHOA, KHOA_OFF);
                }
                else
                {
                    modeRun = IDLE_MODE; // Lệnh không hợp lệ → về chờ
                }
                count = 0;
            }
            break;

        // Các chế độ khác chỉ cần đặt lại cờ k
        case ADDCARD_MODE:       k = 0; break;
        case REMOVECARD_MODE:    k = 0; break;
        case CHANGE_MASTER_CARD: k = 0; break;
        case CHANGE_PASSWORD:    k = 0; break;
    }
}



// TỰ ĐỘNG VỀ IDLE SAU MỘT KHOẢNG THỜI GIAN
void autoReturn()
{
    if (millis() - timeMillisAUTO > timeClose * 1000)
    {
        timeMillisAUTO = millis();
        modeRun = IDLE_MODE;
    }
}



// ĐỔI MẬT KHẨU
void changePasswordFunc()
{
    switch (changePassword)
    {
        case ENTER_PASS_1:
            // Nhập mật khẩu mới lần đầu
            My_LCD.setCursor(0, 0);
            My_LCD.print(" ENTER NEW PASS ");
            customKeypad.tick();
            while (customKeypad.available())
            {
                if (count == 0)
                {
                    My_LCD.setCursor(0, 1);
                    My_LCD.print("                ");
                }
                e = customKeypad.read();
                timeMillisAUTO = millis();
                COI_beep();
                if (e.bit.EVENT == KEY_JUST_RELEASED)
                {
                    if ((char)e.bit.KEY == '#') // Phím # 
                    {
                        count = 0;
                        My_LCD.clear();
                        modeRun = IDLE_MODE;
                    }
                    else
                    {
                        passwordChange1[count] = (char)e.bit.KEY;
                        My_LCD.setCursor(count + 5, 1);
                        My_LCD.print((char)e.bit.KEY);
                        count++;
                    }
                }
            }
            if (count > 5) // Nhập đủ 6 ký tự → chuyển sang bước xác nhận
            {
                delay(200);
                changePassword = ENTER_PASS_2;
                count = 0;
                My_LCD.clear();
            }
            break;

        case ENTER_PASS_2:
            // Nhập lại mật khẩu mới để xác nhận
            My_LCD.setCursor(0, 0);
            My_LCD.print("ENTER PASS AGAIN");
            customKeypad.tick();
            while (customKeypad.available())
            {
                if (count == 0)
                {
                    My_LCD.setCursor(0, 1);
                    My_LCD.print("                ");
                }
                e = customKeypad.read();
                timeMillisAUTO = millis();
                COI_beep();
                if (e.bit.EVENT == KEY_JUST_RELEASED)
                {
                    if ((char)e.bit.KEY == '#') // Phím # → hủy
                    {
                        count = 0;
                        My_LCD.clear();
                        modeRun = IDLE_MODE;
                    }
                    else
                    {
                        passwordChange2[count] = (char)e.bit.KEY;
                        My_LCD.setCursor(count + 5, 1);
                        My_LCD.print((char)e.bit.KEY);
                        count++;
                    }
                }
            }
            if (count > 5) // Đã nhập đủ → so sánh 2 lần nhập
            {
                delay(50);
                if (strcmp(passwordChange1, passwordChange2) == 0) // Hai lần khớp nhau
                {
                    passwordChange2[6] = '\0';
                    memcpy(password, passwordChange1, 7); // Cập nhật mật khẩu trong RAM

                    // Chuyển mật khẩu thành số rồi lưu vào EEPROM
                    uint32_t valueCV = convertPassToNumber();
                    savePWtoEEP(valueCV);
                    delay(100);
                    savePASStoBUFF(); // Đồng bộ lại RAM từ EEPROM

                    My_LCD.clear();
                    My_LCD.setCursor(3, 0);
                    My_LCD.print("CHANGE ");
                    My_LCD.setCursor(3, 1);
                    My_LCD.print("SUCCESSFUL ");
                    delay(2000);
                    changePassword = ENTER_PASS_1; // Reset về bước 1
                    modeRun = IDLE_MODE;
                    My_LCD.clear();
                }
                else // Hai lần nhập không khớp
                {
                    My_LCD.clear();
                    My_LCD.setCursor(3, 0);
                    My_LCD.print("ERROR ");
                    My_LCD.setCursor(3, 1);
                    My_LCD.print("NOT CORRECT");
                    delay(1000);
                    My_LCD.clear();
                    changePassword = ENTER_PASS_1; // Yêu cầu nhập lại từ đầu
                }
                count = 0;
            }
            break;
    }
}


// CÒI BUZZER
// Bật còi 50ms → tắt 50ms (tiếng bíp xác nhận phím)
void COI_beep()
{
    analogWrite(COI, 150); // Bật còi với PWM duty cycle 150/255
    delay(50);
    analogWrite(COI, 0);   // Tắt còi
    delay(50);
}



// KIỂM TRA PHÍM # ĐỂ QUAY VỀ MÀN HÌNH CHÍNH
// Dùng trong các chế độ: thêm thẻ, xóa thẻ, đổi master
void checkBackHome()
{
    customKeypad.tick();
    while (customKeypad.available())
    {
        e = customKeypad.read();
        COI_beep();
        if (e.bit.EVENT == KEY_JUST_RELEASED)
        {
            if ((char)e.bit.KEY == '#') // Nhấn # → về màn hình chờ
            {
                modeRun = IDLE_MODE;
                My_LCD.clear();
                k = 0;
                My_LCD.setCursor(1, 0);
                My_LCD.print("   WELCOME   ");
            }
        }
    }
}



// LƯU ID THẺ VÀO EEPROM
// Mỗi ID (uint32_t) được chia thành 5 byte, lưu vào 5 ô EEPROM liên tiếp
// Địa chỉ bắt đầu = positionID * 5
void saveIDtoEEP(uint32_t valueID, uint8_t positionID)
{
    uint8_t a, b, c, d, e;
    // Tách số 32-bit thành từng cặp chữ số (mỗi biến chứa 2 chữ số BCD)
    e = valueID % 100;   valueID /= 100;
    d = valueID % 100;   valueID /= 100;
    c = valueID % 100;   valueID /= 100;
    b = valueID % 100;   valueID /= 100;
    a = valueID;

    EEPROM.write(positionID * 5,     e);
    EEPROM.write(positionID * 5 + 1, d);
    EEPROM.write(positionID * 5 + 2, c);
    EEPROM.write(positionID * 5 + 3, b);
    EEPROM.write(positionID * 5 + 4, a);
}



// NẠP TOÀN BỘ DANH SÁCH THẺ TỪ EEPROM LÊN MẢNG RAM
// Mỗi 5 ô EEPROM lưu 1 ID thẻ → ghép lại thành uint32_t
void saveIDtoBUFF()
{
    uint32_t a, b, c, d, e;
    for (uint8_t i = 0; i < MAX_BUFF; i++)
    {
        // Đọc 5 byte liên tiếp từ EEPROM
        e = EEPROM.read(i * 5);
        d = EEPROM.read(i * 5 + 1);
        c = EEPROM.read(i * 5 + 2);
        b = EEPROM.read(i * 5 + 3);
        a = EEPROM.read(i * 5 + 4);

        // Ghép lại thành số thập phân 10 chữ số
        ID_CARD_PASS[i] = (a / 10) * 1000000000UL + (a % 10) * 100000000UL
                        + (b / 10) * 10000000UL   + (b % 10) * 1000000UL
                        + (c / 10) * 100000UL      + (c % 10) * 10000UL
                        + (d / 10) * 1000UL        + (d % 10) * 100UL
                        + (e / 10) * 10UL          + (e % 10);
    }
}


// XÓA ID THẺ KHỎI EEPROM
// Ghi 0 vào 5 ô EEPROM tương ứng với vị trí positionID
void clearIDinEEP(uint8_t positionID)
{
    EEPROM.write(positionID * 5,     0);
    EEPROM.write(positionID * 5 + 1, 0);
    EEPROM.write(positionID * 5 + 2, 0);
    EEPROM.write(positionID * 5 + 3, 0);
    EEPROM.write(positionID * 5 + 4, 0);
}


// LƯU MẬT KHẨU VÀO EEPROM
// Mật khẩu 6 chữ số được nhóm thành 3 cặp
void savePWtoEEP(uint32_t valuePASS)
{
    uint32_t c, d, e;
    e = valuePASS % 100;   valuePASS /= 100; // 2 chữ số cuối
    d = valuePASS % 100;   valuePASS /= 100; // 2 chữ số giữa 
    c = valuePASS % 100;                     // 2 chữ số đầu 

    EEPROM.write(252, e);
    EEPROM.write(253, d);
    EEPROM.write(254, c);
}



// NẠP MẬT KHẨU TỪ EEPROM LÊN MẢNG password[] TRONG RAM
void savePASStoBUFF()
{
    uint32_t c, d, e;
    e = EEPROM.read(252); // 2 chữ số cuối của mật khẩu
    d = EEPROM.read(253); // 2 chữ số giữa
    c = EEPROM.read(254); // 2 chữ số đầu

    // Cộng 48 để chuyển từ số nguyên sang mã ASCII ký tự số 
    password[5] = (char)(e % 10 + 48);
    password[4] = (char)(e / 10 + 48);
    password[3] = (char)(d % 10 + 48);
    password[2] = (char)(d / 10 + 48);
    password[1] = (char)(c % 10 + 48);
    password[0] = (char)(c / 10 + 48);
}



// CHUYỂN MẬT KHẨU TỪ CHUỖI KÝ TỰ SANG SỐ NGUYÊN
// Dùng để lưu mật khẩu vào EEPROM
uint32_t convertPassToNumber()
{
    uint32_t valuee =
        ((uint32_t)password[5] - 48)          + 
        ((uint32_t)password[4] - 48) * 10     +  
        ((uint32_t)password[3] - 48) * 100    +  
        ((uint32_t)password[2] - 48) * 1000   +
        ((uint32_t)password[1] - 48) * 10000  +  
        ((uint32_t)password[0] - 48) * 100000; 
    return valuee;
}
