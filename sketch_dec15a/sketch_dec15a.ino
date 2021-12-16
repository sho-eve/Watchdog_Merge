#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <ArduinoJson.h>
#include <time.h>
#include <FIRFilter.h>

//RFID
constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;

MFRC522 mfrc522(SS_PIN, RST_PIN);

//Keypad
const byte ROWS = 4; //using 4 rows keypad
const byte COLS = 4; //using 4 cols keypad

//Sensor
int interval = 50;

int motionSensor = 0;
int motionSensorBefore = 0;
int sensorTilt = 0;
int sensorTiltBefore = 0;

const double a[] = {
  6.237074932031003e-19,  1.203468256554930e-03,  2.789059944207731e-03,  4.234500608097008e-03,
    3.949464324766553e-03,  -2.416866536162014e-18, -8.270810074278890e-03, -1.861479317048980e-02,
    -2.543297162983507e-02, -2.127139910497268e-02, 6.003184622079840e-18,  3.965539443147850e-02,
    9.204504649186659e-02,  1.453456837490316e-01,  1.852171297065770e-01,  2.000000000000000e-01,
    1.852171297065770e-01,  1.453456837490316e-01,  9.204504649186659e-02,  3.965539443147850e-02,
    6.003184622079840e-18,  -2.127139910497268e-02, -2.543297162983507e-02, -1.861479317048980e-02,
    -8.270810074278890e-03, -2.416866536162014e-18, 3.949464324766553e-03,  4.234500608097008e-03,
    2.789059944207731e-03,  1.203468256554930e-03,  6.237074932031003e-19};

//FIRFilter
FIRFilter fir (a);

int person = 0;
int sensorTiltNum = 0;

int motionTilt[] = {0,0,0,0,0,0,0,0,0,0,0};

/*
 * motion[]'s EXvalue rule
 * plus = 2
 * minus = 1
 * zero = 0
 */

void updateMotionSensor (){
  motionSensorBefore = motionSensor;
  sensorTiltBefore = sensorTilt;
  motionSensor = analogRead (A2); //A2 AMN22112 using
  motionSensor = fir.filter (motionSensor);
  sensorTilt = (motionSensor - motionSensorBefore) / 5;
  delay (interval);
}

int AbleToPassTime;
int PassNum = 0;
int startTime, endTime;
char ctoi;
int AbleToPassNum;

char keys[ROWS][COLS] = {
  //配列を表す
  {'1', '2', '3', 'A'},  //{s13, s9, s5, s1},
  {'4', '5', '6', 'B'},  //{s14, s10, s6, s2},
  {'7', '8', '9', 'C'},  //{s15, s11, s7, s3},
  {'*', '0', '#', 'D'}   //{s16, s12, s8, s4}に対応
};
 
byte rowPins[ROWS] = {6, 7, A0, A1}; //接続するピン番号
byte colPins[COLS] = {2, 3, 4, 5}; 

Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS); //キーパッドの初期化

int Sensor (){

  updateMotionSensor();

  int exNumber = 0;

  if (sensorTilt != 0){
    for (int i = 0; i < 10; i++){
  
      if (sensorTilt > 0){
        motionTilt[i] = 2;
  
      } else if (sensorTilt < 0){
        motionTilt[i] = 1;
        
      } else {
        for (int j = 0; j < 10; j++){
          updateMotionSensor();
  
          if (sensorTilt == 0){
            sensorTiltNum++;
          }
        }
  
        if (sensorTiltNum > 9){
          break;
        }
      }
  
      updateMotionSensor();
  
      int loopNum = 0;
  
      while ((sensorTiltBefore >= 0) == (sensorTilt >= 0)){
        updateMotionSensor();
        loopNum++;
        if (loopNum > 10) {
          break;
        }
      }
      
    }
  } else {
    return 0;
  }

  int sameCount = 1;

  //remove zero signal
  while (sameCount){
    sameCount = 0;
    for (int i = 0; i < 10; i++){
      if (motionTilt[i] == 0){
        for (int j = i; j < 10; j++){
          motionTilt[j] = motionTilt[j + 1];
          if (motionTilt[j] != 0){
            sameCount = 1;
          }
        }
      }
    }
  }

  sameCount = 1;

  //remove same signal
  while (sameCount){
    sameCount = 0;
    for (int i = 0; i < 10; i++){
      if (motionTilt[i] == motionTilt[i + 1]){
        for (int j = i; j < 10; j++){
          motionTilt[j] = motionTilt[j + 1];
          if ((motionTilt[j]) != 0){
            sameCount = 1;
          }
        }
      }
    }
  }

  for (int i = 0; i < 10; i++){
    if (motionTilt[i] != 0){
      exNumber++;
    }
  }

  if (exNumber <= 1){
    return 0;
  } else if (exNumber <= 4){
    return 1;
  } else {
    return 2;
  }
}

//if AuthTag is 0, loop will receive return
int AuthTag (){
  if (!mfrc522.PICC_IsNewCardPresent()){
    return 0;
  }

  if (!mfrc522.PICC_ReadCardSerial()){
    return 0;
  }

  if (mfrc522.uid.uidByte[0] == 0xAA && mfrc522.uid.uidByte[1] == 0x35 //UID
  && mfrc522.uid.uidByte[2] == 0x1B && mfrc522.uid.uidByte[3] == 0xB1){
    Serial.println("入室可能なIDです");
    Serial.println("入室する人数を入力してください");
    Serial.print("入室予定人数：");
    while (1){
      char customKey = customKeypad.getKey();
      if (customKey){
        Serial.println(customKey);
        ctoi = customKey;
        AbleToPassTime = 0;
        break;
      }
    }
  } else {
    Serial.println("のび太さんのえっち〜！！！");
    return 0;
  }

  AbleToPassNum = ctoi - '0';

  return 1;
}

int AuthPeople(){
  AbleToPassTime = 0;
  PassNum = 0;
  int startTime = millis();
  while (AbleToPassTime <= 100000 && PassNum < AbleToPassNum){
    AbleToPassTime = millis() - startTime;

    int val = Sensor();

    if (val == 1){
      Serial.print(PassNum + 1);
      Serial.println("人目通過");
      PassNum++;
      AbleToPassTime = 0;
      startTime = millis();
    } else if (val == 2){
      return 1;
    }
  }

  return 0;
}

void setup (){
  Serial.begin(9600);
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

}

void loop (){
  if (Sensor() > 0){
    Serial.println("Alert001");
    return;
  }

  delay(interval);

  int Tagflag = AuthTag();

  if (Tagflag == 0){
    //Serial.println("タグを検出していません");
    return;
  }

  

  if (AuthPeople() == 1){
    Serial.println("Alert002");
    Serial.println("現在の情報をリセットします");
    return;
  }

  Serial.println("Success!");
  
  
}


 
