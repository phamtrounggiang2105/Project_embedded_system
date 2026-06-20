#include <LiquidCrystal.h>    // Thu vien LCD
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>          // thu vien RFID
#include "Adafruit_Keypad.h"  // thu vien Keypad
#include <string.h>
#include <avr/wdt.h>


#define SS_PIN 10
#define RST_PIN 9
#define BUTTON  0 // RX PIN
MFRC522 mfrc522(SS_PIN, RST_PIN);   
#define MAX_BUFF 50    
uint32_t ID_CARD_PASS[MAX_BUFF];
uint32_t uidDecTemp = 0;
uint32_t uidDec = 0;


    
byte bCounter, readBit;
unsigned long ticketNumber;


// Khai báo chân kết nối LCD
LiquidCrystal My_LCD(8, 7, 6, 5, 4, 3);


// Khai báo chân kết nối KEYPAD 3x4
#define KEYPAD_PID3845
#define C3    A1
#define C2    A2
#define C1    A3
#define R4    A4
#define R3    A5
#define R2    A6
#define R1    A7
#include "keypad_config.h"
Adafruit_Keypad customKeypad = Adafruit_Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);


//
#define KHOA      2
#define COI       A0
#define KHOA_ON   1
#define KHOA_OFF  0
#define COI_ON    1
#define COI_OFF   0



typedef enum
{
   IDLE_MODE,
   WAIT_CLOSE,
   ADDCARD_MODE,
   REMOVECARD_MODE,
   CHANGE_MASTER_CARD,
   CHANGE_PASSWORD,
   LOCK_1MIN
}MODE;
uint8_t modeRun = IDLE_MODE;

char password[7] = "111111";
char passwordEnter[7] = {0}; 
char passwordCard[6] = {0}; 
char passwordChange1[7] = {0}; 
char passwordChange2[7] = {0};
uint8_t count = 0;
uint8_t countError = 0;
uint8_t stateLock = 0;
uint8_t timeClose = 1;
uint32_t timeMillisAUTO=0;
uint32_t timeMillisRST = 0;
keypadEvent e;



typedef enum
{
   CHECK_CARD,
   ADD_CARD
}ADDCARD;
typedef enum
{
   CHECK_CARD_RM,
   REMOVE_CARD_RM
}REMOVECARD;
typedef enum
{
   ENTER_PASS_1,
   ENTER_PASS_2
}CHANGEPASS;
uint8_t removeCardState = CHECK_CARD;
uint8_t addCardState = CHECK_CARD;
uint8_t changePassword = ENTER_PASS_1;
uint8_t countCheck = 1;
uint8_t allowAccess = 0;
static uint8_t k = 0;

// Các hàm chính
void checkRFID(void);
void removeCard(void);
void addCard(void);
void insertCardMaster(void);
void changePasswordFunc(void);
void checkButton(void);
void handle_button(void);
void release_button(void);
void COI_beep(void);
uint8_t readRFIDinBUFFER(void);
void checkBackHome(void);
void autoReturn(void);
void saveIDtoEEP(uint32_t valueID, uint8_t positionID);
void saveIDtoBUFF(void);
void clearIDinEEP(uint8_t positionID);
void savePWtoEEP(uint32_t valuePASS);
void savePASStoBUFF(void);
uint32_t convertPassToNumber(void);

void setup()
{
  Serial.begin(9600);  
  My_LCD.begin(16, 2);
  SPI.begin(); 
  mfrc522.PCD_Init();  
 
  wdt_enable(WDTO_8S);
  
  customKeypad.begin();
  pinMode(KHOA, OUTPUT);
  pinMode(BUTTON, INPUT);
  digitalWrite(KHOA, KHOA_OFF);
  pinMode(COI, OUTPUT);
  digitalWrite(COI, COI_OFF);
  My_LCD.clear();

  if(EEPROM.read(251) != 0)
  {
      for(uint8_t i = 0; i <= 250; i++)
      { 
           EEPROM.write( i , 0 );
      }
      EEPROM.write( 251 , 0 );

      // Dia chi lưu password
      EEPROM.write( 252 , 0 );
      EEPROM.write( 253 , 0 );
      EEPROM.write( 254 , 0 );
  }
  else
  {
      saveIDtoBUFF();  
  }
  savePASStoBUFF();
  Serial.println("hello ae");
}
int countt = 0;
void loop()
{
  wdt_reset();
  checkButton();
  checkRFID();
  handle_button();
}


uint8_t currentSst=1,lastSst=1;
void release_button()
{
    COI_beep();
    countError=0;
    digitalWrite(KHOA, KHOA_ON);
    delay(3000);
    digitalWrite(KHOA, KHOA_OFF );
}

void handle_button()
{
  static uint32_t t_debound_buton;
  if(millis() - t_debound_buton >= 40)
  {
    t_debound_buton= millis();
    currentSst = digitalRead(BUTTON);
    if(currentSst==0)  // Giu tay
    {
        if(lastSst==1)
        {
          lastSst=currentSst;
        }
    } 
    else   //Nha Tay
    {
        if(lastSst==0)
        {
          lastSst=currentSst;
          release_button();
        }     
    }
  }
}

void checkRFID()
{
    switch(modeRun)
    {
         case IDLE_MODE:
            readRFIDinBUFFER();
            break;   
         case ADDCARD_MODE:
            if( allowAccess == 1)
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
            checkBackHome();
            break;
         case REMOVECARD_MODE:
            if( allowAccess == 1)
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
            if( allowAccess == 1)
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
             if( allowAccess == 1)
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
            My_LCD.setCursor(2, 0);
            My_LCD.print("TRY AGAIN ");
            My_LCD.setCursor(0, 1);
            My_LCD.print("  IN 1 MINUTE");
            for(int i = 60; i >=0; i --)
            {
                 wdt_reset();
                 My_LCD.clear();
                 My_LCD.setCursor(2, 0);
                 My_LCD.print("TRY AGAIN ");
                 My_LCD.setCursor(0, 1);
                 My_LCD.print("  IN ");
                 if( i > 50 ) analogWrite(COI, 500);
                 else analogWrite(COI, 0);
                 if( i >9)
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
            countError=0;
            modeRun = IDLE_MODE;
            My_LCD.clear();
            break;
    }
}


void removeCard()
{
    uint8_t checkDup = 0;
    switch(removeCardState)
    {
        case CHECK_CARD_RM:
             countCheck = 1;
             My_LCD.setCursor(1, 0);
             My_LCD.print(" REMOVE CARD  ");
             My_LCD.setCursor(1, 1);
             My_LCD.print(" PLEASE INSERT  ");
             if ( ! mfrc522.PICC_IsNewCardPresent()) 
                  return;
             if ( ! mfrc522.PICC_ReadCardSerial())
                  return;
             else
                  COI_beep();
             for (byte i = 0; i < mfrc522.uid.size; i++) {
                  uidDecTemp = mfrc522.uid.uidByte[i];
                  uidDec = uidDec*256+uidDecTemp;
             } 
             mfrc522.PICC_HaltA();
             for(uint16_t i = 1; i < MAX_BUFF; i++)
             {
                  if(uidDec == ID_CARD_PASS[i])
                  {
                     checkDup = 1;
                  }
             }
             if( checkDup == 0)
             {
                  My_LCD.setCursor(0, 0);
                  My_LCD.print("  NOT EXIST   ");
                  My_LCD.setCursor(0, 1);
                  My_LCD.print("                ");
                  uidDec = 0;
                  delay(1000);
                  My_LCD.clear();
             }
             else
             {
                  removeCardState = REMOVE_CARD_RM;
             }
            break;
        case REMOVE_CARD_RM:
           //  Serial.println("check remove");
             if(ID_CARD_PASS[countCheck] == uidDec)
             {
                ID_CARD_PASS[countCheck] = 0  ;
                My_LCD.setCursor(3, 0);
                My_LCD.clear();
                My_LCD.setCursor(1, 0);
                My_LCD.print("WAITING... ");
                delay(1000);
                
                clearIDinEEP(countCheck);
                saveIDtoBUFF();
                for(uint8_t i = 0; i < MAX_BUFF; i++)
                {
                  // Serial.println(ID_CARD_PASS[i]);
                }
                
                My_LCD.clear();
                My_LCD.print("REMOVE ");
                My_LCD.setCursor(3, 1);
                My_LCD.print("SUCCESSFUL ");
                delay(2000);
                
  
                My_LCD.clear();
                uidDec = 0;
                removeCardState = CHECK_CARD;
                countCheck++;
             }
             else
             {
                countCheck++;
                if(countCheck > MAX_BUFF)
                {
                     countCheck = 1;
                }
             }
             break;
    }
   
}
void addCard()
{
     uint8_t checkDup = 0;
     switch(addCardState)
     {
          case  CHECK_CARD:
             My_LCD.setCursor(3, 0);
             My_LCD.print(" ADD CARD  ");
             My_LCD.setCursor(1, 1);
             My_LCD.print(" PLEASE INSERT  ");
             if ( ! mfrc522.PICC_IsNewCardPresent()) 
                  return;
             if ( ! mfrc522.PICC_ReadCardSerial())
                  return;
             else
                  COI_beep();
             for (byte i = 0; i < mfrc522.uid.size; i++) {
                  uidDecTemp = mfrc522.uid.uidByte[i];
                  uidDec = uidDec*256+uidDecTemp;
             } 
             mfrc522.PICC_HaltA();
             for(uint16_t i = 0; i < MAX_BUFF; i++)
             {
                  if(uidDec == ID_CARD_PASS[i])
                  {
                     checkDup = 1;
                  }
             }
             if( checkDup == 1)
             {
                  My_LCD.setCursor(0, 0);
                  My_LCD.print("  ALREADY EXIST ");
                  My_LCD.setCursor(0, 1);
                  My_LCD.print("                ");
                  uidDec = 0;
                  delay(1000);
                  My_LCD.clear();
             }
             else
             {
                  addCardState = ADD_CARD;
             }
             break;
        case  ADD_CARD:
             if(ID_CARD_PASS[countCheck] == 0)
             {
                ID_CARD_PASS[countCheck] = uidDec  ;
                My_LCD.clear();
                My_LCD.setCursor(2, 0);
                My_LCD.print("WAITING... ");
                delay(1000);
                saveIDtoEEP(ID_CARD_PASS[countCheck] , countCheck);
                saveIDtoBUFF();
                for(uint8_t i = 0; i < MAX_BUFF; i++)
                {
                  // Serial.println(ID_CARD_PASS[i]);
                }
                My_LCD.clear();
                My_LCD.setCursor(3, 0);
                My_LCD.print("ADD ");
                My_LCD.setCursor(3, 1);
                My_LCD.print("SUCCESSFUL ");
                delay(2000);
                My_LCD.clear();
                
                uidDec = 0;
                addCardState = CHECK_CARD;
                countCheck++;
             }
             else
             {
                countCheck++;
                if(countCheck > MAX_BUFF)
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
void insertCardMaster()
{
    My_LCD.setCursor(0, 0);
    My_LCD.print(" CHANGE MASTER ");
    My_LCD.setCursor(0, 1);
    My_LCD.print(" PLEASE INSERT  ");
    
    if ( ! mfrc522.PICC_IsNewCardPresent()) 
        return;
    if ( ! mfrc522.PICC_ReadCardSerial())
        return;
    else
        COI_beep();
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec*256+uidDecTemp;
    } 

    mfrc522.PICC_HaltA();

    if(uidDec != 0)
    {
      ID_CARD_PASS[0] = uidDec;
      saveIDtoEEP(ID_CARD_PASS[0] , 0);
      saveIDtoBUFF();
      for(uint8_t i = 0; i < MAX_BUFF; i++)
      {
        // Serial.println(ID_CARD_PASS[i]);
      }
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
uint8_t readRFIDinBUFFER()
{
    uint8_t check_ID = 0;
    
    if ( ! mfrc522.PICC_IsNewCardPresent())
        return 0;
    if ( ! mfrc522.PICC_ReadCardSerial())
        return 0;
    else
        COI_beep();

    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec*256+uidDecTemp;
    }

    mfrc522.PICC_HaltA();
    for(uint16_t i = 0; i < MAX_BUFF; i++)
    {
        if(uidDec == ID_CARD_PASS[i])
        {
           check_ID ++;  
           if( uidDec == ID_CARD_PASS[0])
           {
                allowAccess = 1;
           }
        }     
    }
    if(check_ID > 0)
    {
        k=0;
        My_LCD.setCursor(1, 0);
        My_LCD.print("   WELCOME   ");
        My_LCD.setCursor(3, 1);
        My_LCD.print("VALID CARD");
        modeRun = WAIT_CLOSE;      
        timeMillisAUTO = millis();
        digitalWrite(KHOA, KHOA_ON);
        count = 0;
        countError = 0;
        delay(200);
        return 1;
    }
    else
    {
         k=0;
         My_LCD.setCursor(1, 0);
         My_LCD.print("   WELCOME   ");
         My_LCD.setCursor(2, 1);
         My_LCD.print("INVALID CARD");
         countError ++; 
         if(  countError >= 5)
                 modeRun = LOCK_1MIN;
         delay(1000);
         My_LCD.clear();  
    }
    
    return 0;
}
void checkButton()
{   

  checkRFID();
  switch(modeRun)
  {
    case IDLE_MODE:
       allowAccess = 0;
       digitalWrite(KHOA, KHOA_OFF);
       My_LCD.setCursor(1, 0);
       if( k == 0)
       {
          My_LCD.print("   WELCOME   ");
          k = 1; 
       }
       customKeypad.tick();
        while(customKeypad.available()){
            e = customKeypad.read();
            COI_beep();
            if(e.bit.EVENT == KEY_JUST_RELEASED) 
            {
                if(count == 0) 
                    My_LCD.clear();
                if( (char)e.bit.KEY == '#' )
                {
                     count=0; 
                     My_LCD.clear();
                     modeRun = IDLE_MODE ;
                     k= 0;
                }
                else
                {
                    k = 0;
                    passwordEnter[count] = (char)e.bit.KEY;
                    My_LCD.setCursor(count + 4 , 1);
                    My_LCD.print('*');
                    count++ ;
                }
            }  
        }  
        if(count > 5)
        {
            k = 0;
            passwordEnter[6] = '\0';
            My_LCD.setCursor(9 , 1);
            My_LCD.print('*');
            delay(50);
          //  Serial.println(passwordEnter);
            if(strcmp(passwordEnter,password) == 0)
            {
                allowAccess = 1;
         //      My_LCD.clear(); 
                My_LCD.setCursor(0, 0);
                My_LCD.print("CORRECT PASSWORD");
         //       My_LCD.setCursor(4, 1);
         //       My_LCD.print("OPEN DOOR");
                timeMillisAUTO = millis();
                modeRun = WAIT_CLOSE; 
            }
            else
            {
                delay(100);
                My_LCD.clear();
                My_LCD.setCursor(1, 0);
                My_LCD.print("WRONG PASSWORD");
                My_LCD.setCursor(1, 1);
                My_LCD.print(4-countError);
                My_LCD.setCursor(3, 1);
                My_LCD.print("TIMES LEFT");
                delay(2000);
                My_LCD.clear();  
                countError ++;   
                if(  countError >= 5)
                    modeRun = LOCK_1MIN;
            }
            count = 0;
        }
      break;
    case WAIT_CLOSE:
       // auto lock door
       k = 0;
       countError=0;
       digitalWrite(KHOA, KHOA_ON);
       if(millis() - timeMillisAUTO > timeClose*1000)
       {
            timeMillisAUTO = millis();
            
            My_LCD.clear();
            delay(10);
            My_LCD.begin(16, 2);
            delay(10);
            count=0; 

            mfrc522.PCD_Reset();
            delay(10);
            mfrc522.PCD_Init();  
            delay(10);
            modeRun = IDLE_MODE; 
       }
       
       customKeypad.tick();
       while(customKeypad.available()){
            if(count == 0 ) 
            {
                 My_LCD.setCursor(0 , 1);
                 My_LCD.print("                ");
            }
            e = customKeypad.read();
            // đợi k cho về idle mode
            timeMillisAUTO = millis();
            COI_beep();
            if(e.bit.EVENT == KEY_JUST_RELEASED) 
            {
                passwordCard[count] = (char)e.bit.KEY; 
                My_LCD.setCursor(count + 5 , 1);
                My_LCD.print((char)e.bit.KEY);
                count++ ;
            }
       }
       if(count > 3)
       {    

            My_LCD.setCursor(8 , 1);
            My_LCD.print((char)e.bit.KEY);
            delay(100);
            
            My_LCD.clear();
            
            if(strcmp(passwordCard, "####") == 0)
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
                 modeRun = IDLE_MODE;
            }
            count = 0;
       }
      break;
    case ADDCARD_MODE:
       k = 0;
      break;
    case REMOVECARD_MODE:   
       k = 0;
      break;
    case CHANGE_MASTER_CARD:
       k = 0;
      break;
    case CHANGE_PASSWORD:
       k = 0;
       break;
       
  }
}
void autoReturn()
{
   if(millis() - timeMillisAUTO > timeClose*1000)
   {
        timeMillisAUTO = millis();
        modeRun = IDLE_MODE; 
   }
}
void changePasswordFunc()
{
    switch(changePassword)
    {
          case    ENTER_PASS_1:
               My_LCD.setCursor(0, 0);
               My_LCD.print(" ENTER NEW PASS ");
               customKeypad.tick();
               while(customKeypad.available()){
                    if(count == 0) 
                    {
                         My_LCD.setCursor(0 , 1);
                         My_LCD.print("                ");
                    }
                    e = customKeypad.read();
                    // đợi k cho về idle mode
                    timeMillisAUTO = millis();
                    COI_beep();
                    if(e.bit.EVENT == KEY_JUST_RELEASED) 
                    {
                        if( (char)e.bit.KEY == '#' )
                        {
                              count=0; 
                              My_LCD.clear();
                              modeRun = IDLE_MODE ;
                              
                        }
                        else
                        {
                            passwordChange1[count] = (char)e.bit.KEY; 
                            My_LCD.setCursor(count + 5 , 1);
                            My_LCD.print((char)e.bit.KEY);
                            count++ ;
                        }
                    }
               }
               if(count > 5)
               {
                    delay(200);
                    changePassword = ENTER_PASS_2;
                    count  = 0;
                    My_LCD.clear();
               }
              break;
          case    ENTER_PASS_2:
             My_LCD.setCursor(0, 0);
             My_LCD.print("ENTER PASS AGAIN");
             customKeypad.tick();
             while(customKeypad.available()){
                  if(count == 0) 
                  {
                       My_LCD.setCursor(0 , 1);
                       My_LCD.print("                ");
                  }
                  e = customKeypad.read();
                  // đợi k cho về idle mode
                  timeMillisAUTO = millis();
                  COI_beep();
                  if(e.bit.EVENT == KEY_JUST_RELEASED) 
                  {
                      if( (char)e.bit.KEY == '#' )
                      {
                              count=0; 
                              My_LCD.clear();
                              modeRun = IDLE_MODE ;
                      }
                      else
                      {
                            passwordChange2[count] = (char)e.bit.KEY; 
                            My_LCD.setCursor(count + 5 , 1);
                            My_LCD.print((char)e.bit.KEY);
                            count++ ;
                      }
                  }
             }
             if(count > 5)
             {
                  delay(50);
                  
                  if(strcmp(passwordChange1, passwordChange2) == 0)
                  {      
                        passwordChange2[6] = '\0';
                        memcpy (password, passwordChange1, 7);
                    //    Serial.println(password);
                        // luu vao flash
                        uint32_t valueCV =convertPassToNumber();
                        savePWtoEEP(valueCV);
                        delay(100);
                        savePASStoBUFF(); 
                        
                        My_LCD.clear();
                        My_LCD.setCursor(3, 0);
                        My_LCD.print("CHANGE ");
                        My_LCD.setCursor(3, 1);
                        My_LCD.print("SUCCESSFUL ");
                        delay(2000);
                        changePassword = ENTER_PASS_1;
                        modeRun = IDLE_MODE ;
                        My_LCD.clear();
                  }
                  else
                  {
                        My_LCD.clear();
                        My_LCD.setCursor(3, 0);        
                        My_LCD.print("ERROR ");
                        My_LCD.setCursor(3, 1);
                        My_LCD.print("NOT CORRECT");
                        delay(1000);
                        My_LCD.clear();
                        changePassword = ENTER_PASS_1;
                  }
                  count = 0;
             }
             break;
    }     
}
void COI_beep()
{
  analogWrite(COI, 150);
  delay(50);
  analogWrite(COI, 0);
  delay(50);
}

void checkBackHome()
{
    customKeypad.tick();
    while(customKeypad.available()){
          e = customKeypad.read();
            COI_beep();
            if(e.bit.EVENT == KEY_JUST_RELEASED) 
            {
                if((char)e.bit.KEY == '#')
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


void saveIDtoEEP(uint32_t valueID, uint8_t positionID)
{
    uint8_t a,b,c,d,e;
    e = valueID % 100;
    valueID /= 100;
    d = valueID % 100;
    valueID /= 100;
    c = valueID % 100;
    valueID /= 100;
    b = valueID % 100;
    valueID /= 100;
    a = valueID;
    EEPROM.write(positionID*5    , e);
    EEPROM.write(positionID*5 + 1, d);
    EEPROM.write(positionID*5 + 2, c);
    EEPROM.write(positionID*5 + 3, b);
    EEPROM.write(positionID*5 + 4, a);  


}

void saveIDtoBUFF()
{
    // Dia chi lưu thẻ RFID, mỗi 5 ô nhớ lưu 1 ID CARD
   uint32_t a,b,c,d,e;
   for(uint8_t i =0; i < MAX_BUFF; i++)
   { 
      e = EEPROM.read(i*5    );
      d = EEPROM.read(i*5 + 1);
      c = EEPROM.read(i*5 + 2);
      b = EEPROM.read(i*5 + 3);
      a = EEPROM.read(i*5 + 4);  
      ID_CARD_PASS[i] = (a/10)*1000000000 + (a%10)*100000000 + (b/10)*10000000 + (b%10)*1000000 +(c/10)*100000 + (c%10)*10000 + (d/10)*1000 + (d%10)*100 + e%10 + (e/10)*10;
   }
}

void clearIDinEEP(uint8_t positionID )
{
    EEPROM.write(positionID*5    , 0);
    EEPROM.write(positionID*5 + 1, 0);
    EEPROM.write(positionID*5 + 2, 0);
    EEPROM.write(positionID*5 + 3, 0);
    EEPROM.write(positionID*5 + 4, 0);  
}

void savePWtoEEP(uint32_t valuePASS)
{
    uint32_t c,d,e;
    e = valuePASS % 100;
    valuePASS /= 100;
    d = valuePASS % 100;
    valuePASS /= 100;
    c = valuePASS % 100;

    EEPROM.write(252, e);
    EEPROM.write(253, d);
    EEPROM.write(254, c);
}

void savePASStoBUFF()
{
    uint32_t c,d,e;
    e = EEPROM.read(252);
    d = EEPROM.read(253);
    c = EEPROM.read(254);
    password[5] =  (char) (e %10 + 48) ;
    password[4] =  (char) (e /10 + 48 ) ;
    password[3] =  (char) (d %10 + 48 ) ;
    password[2] =  (char) (d /10 + 48 ) ;
    password[1] =  (char) (c %10 + 48 ) ;
    password[0] =  (char) (c /10 + 48 ) ;  
}
uint32_t convertPassToNumber()
{
     uint32_t valuee = ((uint32_t)password[5] - 48) + ((uint32_t)password[4]-48)*10 +
    ((uint32_t)password[3]-48)*100 + ((uint32_t)password[2]-48)*1000 +
    ((uint32_t)password[1]-48)*10000 + ((uint32_t)password[0] - 48)*100000;
    return valuee;
}