#include <Wire.h>
#include <SPI.h>
#include <SD.h>
//#include <EEPROM.h>
#include "RTC8564.h"

//#define SENSOR_MAX6675 //comment MAX31855
// Initialize the Thermocouple
// set pin of MAX31855,MAX6675
#define MAXDO   3
#define MAXCS   4
#define MAXCLK  5
#ifdef SENSOR_MAX6675
  #include "max6675.h"
  MAX6675 thermocouple(MAXCLK, MAXCS, MAXDO);
#else
  #include "Adafruit_MAX31855.h"
  Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);
#endif

float temperatureData = 0.0;  // Temperature

/*** OLED Definition ***/
//#define DISPLAY_OLED //comment No Use OLED
#ifdef DISPLAY_OLED
  #include <U8g2lib.h>
  U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
  #define OLED_ADDR 0x3C
#endif

/*** Enumerated I2C addresses ***/
uint8_t Enumerated_I2C_addresses[8];

/*** SD Card Definition ***/
#define SS_pin 9  // D9

/*** Switches Definition ***/
#define QTY_of_switches 4
#define sw_Upper 0
#define sw_Lower 1
#define sw_Center 2
#define sw_Push 3
struct SwitchStatus {
  uint8_t PinNumber;
  int Counter;
  bool Pressed;
  bool Released;
  bool LongPressing;
  bool LongPressed;
};
volatile struct SwitchStatus SW[QTY_of_switches];

/*** LCD ***/
#define LCD_ADDR 0x3e
char DisplayData[20] = "n.k     products    ";
void writeLCDData(byte t_data){
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(0x40);
  Wire.write(t_data);
  Wire.endTransmission();
  delay(1);
}
void writeLCDCommand(byte t_command){
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(0x00);
  Wire.write(t_command);
  Wire.endTransmission();
  delay(1);
}
void init_LCD(){
  delay(400);
  writeLCDCommand(0x38);
  writeLCDCommand(0x39);
  writeLCDCommand(0x14);
  writeLCDCommand(0x7c);
  writeLCDCommand(0x55);
  writeLCDCommand(0x6c);
  delay(250);
  writeLCDCommand(0x38);
  writeLCDCommand(0x01);
  writeLCDCommand(0x0c);
  delay(1);
}
void LCD_xy(uint8_t x, uint8_t y){
    writeLCDCommand(0x80 + 0x40 * y + x);
}
void LCD_str2(const char *c) {
    unsigned char wk;
    int i;
    for (i=0;i<8;i++){
        wk = c[i];
        if  (wk == 0x00) break;
        writeLCDData(wk);
    }
    delay(1);
}
void LCD_char(char wk) {
  writeLCDData(wk);
  delay(1);
}
void LCD_char_with_Cursor(uint8_t x, uint8_t y, char wk) {
  writeLCDCommand(0x80 + 0x40 * y + x);
  writeLCDData(wk);
  LCD_cursor_Left();
  delay(1);
}
void LCD_clear(){
  writeLCDCommand(0x01);
  delay(1);
}
void LCD_cursor_on(){
  writeLCDCommand(0b00001111);
  //writeLCDCommand(0b00010000);  // bit3=S/C=L, bit2=R/K=L
  delay(1);
}
void LCD_cursor_off(){
  writeLCDCommand(0b00001100);
  delay(1);
}
void LCD_cursor_Left(){
  writeLCDCommand(0b00010000);
  delay(1);
}
void LCD_cursor_Right(){
  writeLCDCommand(0b00010100);
  delay(1);
}

/*** SHT31 ***/
#define SHT31_ADDR 0x44
int TData, HData;  //Temp data , Humi data
uint8_t THData[6];
/*
THData[0] temperature high
THData[1] temperature low
THData[2] CRC temperature
THData[3] humidity high
THData[4] humidity low
THData[5] CRC humidity
 */
void readSHT31(){
  Wire.beginTransmission(SHT31_ADDR);
  Wire.write(0x24);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(30);  // 12=NG, 13=Good
  Wire.requestFrom(SHT31_ADDR, 6);
  for (int i=0; i<6; i++){
    THData[i] = Wire.read();
  }
}
void getTH(){
  long lTData, lHData;
  readSHT31();
  lTData = 256 * THData[0] + THData[1];
  lTData = -450 + (1750 * lTData) / 65535;
  TData = (int)lTData;
  lHData = (long)(THData[3]);
  lHData = (lHData << 8) | THData[4];
  lHData = (1000 * lHData) / 65535;
  HData = (int)lHData;
}
void DispTH(){
  byte x, x2;
  float T2Data,H2Data;
  getTH();
  #ifdef DISPLAY_OLED
    if (isAaddressExistInI2C(OLED_ADDR)){
      u8g2.clearBuffer();
      u8g2.firstPage();
      do {
        /* all graphics commands have to appear within the loop body. */
        T2Data = (float)TData; dtostrf(T2Data/10,4,1,DisplayData);
        x = u8g2.drawStr(0,25,DisplayData);
        x2 = u8g2.drawStr(x,25," C");//"\xB0"); 
        //u8g2.drawStr(x+x2,15,"C");
        H2Data = (float)HData; dtostrf(H2Data/10,4,1,DisplayData);
        x = u8g2.drawStr(0,63,DisplayData); u8g2.drawStr(x,63," %");
      } while ( u8g2.nextPage() );
      u8g2.sendBuffer();
    }
  #endif
  sprintf(DisplayData, "%3d.%1d C\0", TData /10, abs(TData %10) );//"%3d.%1d\xDF\x43", TData /10, abs(TData %10) );
  LCD_xy(0,0); LCD_str2( DisplayData );
  sprintf(&DisplayData[8], " %2d.%1d %%\0", HData /10, HData %10 );
  LCD_xy(0,1); LCD_str2( &DisplayData[8] );
}

/*** Read Thermocouple ***/

void ReadThermocouple(void) {
  float temp = 0.0;  // Temperature
  for (byte i = 0; i < 5; i++) {
      do {
      temperatureData = thermocouple.readCelsius();
    }while(isnan(temperatureData)) ;
        
    temp = temp + temperatureData;
    delay(20);
  }
  #ifdef SENSOR_MAX6675
    temperatureData = (temp / 5.0)*0.99 -2.5;  //MAX6675
    //if ( temperatureData > 200 ) temperatureData-10;
  #else
    temperatureData = (temp / 5.0)*1.00-1.0;  //MAX31855
  #endif
}

/*** RTC8564 ***/
#define RTC8564_SLAVE_ADRS 0x51
bool TimerEnable = false;
uint8_t date_time[7];
/*date_time[0] = 0x00;//seconds
date_time[1] = 0x45;//minutes
date_time[2] = 0x17;//hours
date_time[3] = 0x28;//days
date_time[4] = 0x04;//weekdays
date_time[5] = 0x11;//months/century
date_time[6] = 0x24;//years*/
void writeCommandToRTC(uint8_t reg1, uint8_t dat1){
  Wire.beginTransmission(RTC8564_SLAVE_ADRS);
  Wire.write(reg1);
  Wire.write(dat1);
  Wire.endTransmission();
}
uint8_t readRegDataFromRTC(uint8_t reg1){
  Wire.beginTransmission(RTC8564_SLAVE_ADRS);
  Wire.write(reg1);
  Wire.endTransmission();
  Wire.requestFrom(RTC8564_SLAVE_ADRS, 1);
  if( Wire.available() ){
    return( Wire.read() );
  } else {
    return 0xff;
  }
}
// 60分間隔で/INT信号を繰り返しLOWにする
void generateINTeveryHour(void){
  writeCommandToRTC(0x0e, 0b00000011); // TE bit7 = 0 Stop Timer; TD bit0-1 0b11 = 1 minuts, 0b10 = 1second
  writeCommandToRTC(0x01, 0b00000001); // TI/TP = 0 Level INT mode; TF = 0 INT Hi-z; TIE = 1 Enable INT
  writeCommandToRTC(0x0f, 60);         // (TD bit * 60)
  writeCommandToRTC(0x0e, 0b10000011); // TE bit7 = 1 Start Timer; TD bit0-1 0b11 = 1 minuts, 0b10 = 1second
}
// /INTをクリアする = 電源OFF
void clearINT(void){
  delay(500); // Delay 500ms
  writeCommandToRTC(0x01, 0b00000001); // TF = 0 clear INT -> Power Off; TI/TP = 0; TIE = 1
}
// TE(Timer Enable) bitをクリアする
void endTimerOperation(void){
  writeCommandToRTC(0x0e, 0b00000011); // TE bit7 = 0 Stop Timer; TD bit0-1 0b11 = 1 minuts, 0b10 = 1second
}
// TE bitを読み出す
bool readTEbit(){
  if ( (readRegDataFromRTC(0x0e) & 0b10000000) != 0 ){
    return true;
  } else {
    return false;
  }
}
#define AAMinute 1
#define AAHour  2
#define AADay   3
#define AAWeek  4
#define AAMonth 5
#define AAYear  6
void LoadTimeValuesIntoArray(){
  date_time[AAMinute] = Rtc.minutes();
  date_time[AAHour] = Rtc.hours();
  date_time[AADay] = Rtc.days();
  date_time[AAWeek] = Rtc.weekdays();
  date_time[AAMonth] = Rtc.months();
  date_time[AAYear] = Rtc.years();
}
void SaveArrayValuesIntoRTC(){
  Rtc.sync( date_time, 7 );
}

/*** SD Card ***/
bool CardDetect(){
  if (digitalRead(A3) == LOW){
    return true;
  } else {
    return false;
  }
}

// 現在時刻をLCDに表示する
const char* Weekdays[]={"Su","Mo","Tu","Wd","Th","Fr","Sa"};
void DispCurrentTime(){
  Rtc.available();  //read RTC data
  if (isAaddressExistInI2C(LCD_ADDR)){
    LCD_clear();
    DisplayData[0] = (Rtc.years() >> 4) + 0x30;
    DisplayData[1] = (Rtc.years() & 0x0f) + 0x30;
    DisplayData[2] = ' '; 
    DisplayData[3] = (Rtc.months() >> 4) + 0x30;
    DisplayData[4] = (Rtc.months() & 0x0f) + 0x30;
    DisplayData[5] = '/';
    DisplayData[6] = (Rtc.days() >> 4) + 0x30;
    DisplayData[7] = (Rtc.days() & 0x0f) + 0x30;
    DisplayData[8] = 0;
    LCD_xy(0,0); LCD_str2( DisplayData );
    DisplayData[0] = *Weekdays[Rtc.weekdays()];
    DisplayData[1] = *(Weekdays[Rtc.weekdays()]+1);
    DisplayData[3] = (Rtc.hours() >> 4) + 0x30;
    DisplayData[4] = (Rtc.hours() & 0x0f) + 0x30;
    DisplayData[5] = ':';
    DisplayData[6] = (Rtc.minutes() >> 4) + 0x30;
    DisplayData[7] = (Rtc.minutes() & 0x0f) + 0x30;
    DisplayData[8] = 0;
    LCD_xy(0,1); LCD_str2( DisplayData );
  }
}
// 現在時刻と温湿度をSDに書き出す
void writeCurrentTimeAndTemp(){
  char WriteData[18]="2024/11/18 15:55\0";
  Rtc.available();  //read RTC data
  WriteData[2]=(Rtc.years() >> 4) + 0x30;
  WriteData[3]=(Rtc.years() & 0x0f) + 0x30;
  WriteData[5]=(Rtc.months() >> 4) + 0x30;
  WriteData[6]=(Rtc.months() & 0x0f) + 0x30;
  WriteData[8]=(Rtc.days() >> 4) + 0x30;
  WriteData[9]=(Rtc.days() & 0x0f) + 0x30;
  WriteData[11]=(Rtc.hours() >> 4) + 0x30;
  WriteData[12]=(Rtc.hours() & 0x0f) + 0x30;
  WriteData[14]=(Rtc.minutes() >> 4) + 0x30;
  WriteData[15]=(Rtc.minutes() & 0x0f) + 0x30;
  WriteData[16]=0;

  getTH();
  sprintf(DisplayData, ",%3d.%1d C, %2d.%1d %%,\n\0", TData /10, abs(TData %10), HData /10, HData %10 );
  
  Serial.print(WriteData);  // for test
  Serial.print(DisplayData);  // for test
  Serial.println(temperatureData);  // for test
  if ( CardDetect() ) {
    pinMode(SS_pin, OUTPUT);
    pinMode(10, OUTPUT);//pinMode(10, INPUT_PULLUP);
    delay(20);
    if (!SD.begin(SS_pin)) {
      Serial.println("SD init failed!");
      //while (1);
      return;
    } else Serial.println("SD init done.");
    delay(20);  // SD.beginの後、SD.openの前にwaitが要るっぽい。grobal data DisplayDataまたはWriteDataの一部が文字化けすることがある。
    File myFile = SD.open("LOG.TXT");//for test
    myFile.close();
    for (int i = 0; i < 10; i++ ){  // for test
        delay(500);
      myFile = SD.open("LOG.TXT", FILE_WRITE);
      if (myFile) {
        Serial.print("Writing to LOG...");
        //delay(20);
        myFile.write(WriteData, 16);
        myFile.write(DisplayData, 17);
        myFile.println(temperatureData);
        myFile.close();
        Serial.println("done.");
        /* Regarding the problem of failing to Begin SD every other time, it will no longer occur if you READ it once here. */
        myFile = SD.open("LOG.TXT", FILE_WRITE);
        myFile.close();
        break;
      } else {
        // if the file didn't open, print an error:
        Serial.print(" *");//ln("error opening log.txt");
      }
      if (i == 9) Serial.println(" error opening LOG");
    }
  } else {
    Serial.println("No SD found.");    
  }
  delay(100);
  digitalWrite(SS_pin, HIGH); // SS_pin inactive
  pinMode(10, INPUT_PULLUP);
}

/* Enumerate I2C addresses */
int ScanI2C(){
  int nDevices = 0;
  // 0x3C : OLED Akizuki
  // 0x3E : LCD AQM0802
  // 0x44 : Temp/Humi Sensor SHT31
  // 0x51 : RTC8564
  for (byte i = 0; i < 8; ++i) {
    Enumerated_I2C_addresses[i] = 0;
  }
  Serial.println("I2C Scanning...");
  for (byte address = 1; address < 127; ++address) {
    // The i2c_scanner uses the return value of
    // the Wire.endTransmission to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println("  !");
      ++nDevices;
      Enumerated_I2C_addresses[nDevices] = address;
    } else if (error == 4) {
      Serial.print("Unknown error 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C found\n");
  } else {
    Serial.println("done");
  }
  return nDevices;
}
bool isAaddressExistInI2C(uint8_t address_to_look_for){
  for (int i=0;i<8;i++){
      if(Enumerated_I2C_addresses[i] == address_to_look_for){
        return true;
      }
  }
  return false;  
}

/*** get switch status ***/
void getSwitchStatus(){
  for (int i=0; i < QTY_of_switches; i++){
    if (digitalRead(SW[i].PinNumber) == LOW){
      SW[i].Counter++;
      SW[i].Released = false;
      if (SW[i].Counter > 1000) SW[i].LongPressing = true;
    } else {
      SW[i].Released = true;
      if ( (SW[i].Counter > 10) && (SW[i].Counter < 1000) ) {
        SW[i].Pressed = true;
      } else SW[i].Pressed = false;
      if (SW[i].Counter > 1000) SW[i].LongPressed = true;
      SW[i].Counter = 0;   
    }
  }
}

/*** clear switch status ***/
void clearSwitchStatus(){
  for (int i=0; i < QTY_of_switches; i++){
    SW[i].Counter = 0;
    SW[i].LongPressing = false;
    SW[i].LongPressed = false;
    SW[i].Released = true;
    SW[i].Pressed = false;
  }
}

/*** Digit High/Low Up/Down ***/
void digitHUp(uint8_t x, uint8_t y, int DateTimeSelect, int MaxDigitValue){
  date_time[DateTimeSelect] += 0x10; if ( date_time[DateTimeSelect] >= MaxDigitValue ) date_time[DateTimeSelect] -= MaxDigitValue;
  LCD_char_with_Cursor(x,y, (date_time[DateTimeSelect] >> 4) + 0x30 );
}
void digitHDown(uint8_t x, uint8_t y, int DateTimeSelect, int MaxDigitValue){
  uint8_t tmpDT;
  tmpDT = date_time[DateTimeSelect];
  tmpDT = (tmpDT & 0xF0) -0x10; if ( tmpDT == 0xF0 ) tmpDT = MaxDigitValue -0x10;
  date_time[DateTimeSelect] = (date_time[DateTimeSelect] & 0x0F) | tmpDT;
  LCD_char_with_Cursor(x,y, (date_time[DateTimeSelect] >> 4) + 0x30 );
}
void digitLUp(uint8_t x, uint8_t y, int DateTimeSelect){ // minute:9, hour:9, day:9, month:9, year:9
  date_time[DateTimeSelect] += 1; if ( (date_time[DateTimeSelect] & 0x0f) > 9 ) date_time[DateTimeSelect] = date_time[DateTimeSelect] & 0xF0;
  LCD_char_with_Cursor(x,y, (date_time[DateTimeSelect] & 0x0f) + 0x30 );
}
void digitLDown(uint8_t x, uint8_t y, int DateTimeSelect){
  uint8_t tmpDT;
  tmpDT = date_time[DateTimeSelect];
  tmpDT = (tmpDT & 0x0f) -1; if ( tmpDT == 0xFF ) tmpDT = 9;
  date_time[DateTimeSelect] = (date_time[DateTimeSelect] & 0xF0) | tmpDT; 
  LCD_char_with_Cursor(x,y, (date_time[DateTimeSelect] & 0x0f) + 0x30 );
}
void digitWUp(uint8_t x, uint8_t y, int DateTimeSelect){
  date_time[DateTimeSelect] += 1; if ( date_time[DateTimeSelect] > 6 ) date_time[DateTimeSelect] = 0;
  LCD_char_with_Cursor(x,y, *Weekdays[date_time[DateTimeSelect]] );
  LCD_char_with_Cursor(x+1,y, *(Weekdays[date_time[DateTimeSelect]]+1) );
}
void digitWDown(uint8_t x, uint8_t y, int DateTimeSelect){
  date_time[DateTimeSelect] -= 1; if ( date_time[DateTimeSelect] == 0xFF ) date_time[DateTimeSelect] = 6;
  LCD_char_with_Cursor(x,y, *Weekdays[date_time[DateTimeSelect]] );
  LCD_char_with_Cursor(x+1,y, *(Weekdays[date_time[DateTimeSelect]]+1) );
}


void setup() {
  // put your setup code here, to run once:
  pinMode(8, OUTPUT); // LED
  pinMode(SS_pin, OUTPUT); // SS_pin =9
  digitalWrite(SS_pin, HIGH); // SS_pin inactive
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW); // HIGH: Forced Power On; Keep power on to finished writing files to the SD card
  pinMode(A2, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A0, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(A3, INPUT); // Detect SD Card
  
  SW[sw_Upper].PinNumber =  A1;
  SW[sw_Lower].PinNumber =  A0;
  SW[sw_Center].PinNumber = A2;
  SW[sw_Push].PinNumber =   10;
  clearSwitchStatus();
  
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\nTemp-Sensor");

  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock frequency 400KHz; Without this statement, fail writing to the SD card! Displaydata was broken.
  
  ScanI2C();
  
  Rtc.begin();
  Rtc.available();  //read RTC data

  if ( readTEbit() ){
    digitalWrite(8,0);  // Turns on the LED when generateINTeveryHour
    ReadThermocouple(); //for test
    //Serial.println(temperatureData);
    writeCurrentTimeAndTemp();  // to SD Card
    delay(100);
    clearINT(); // Power off when Power SW is off.   
  } else {
    digitalWrite(8,1);  // Turns off the LED
  }
  
  init_LCD();
  LCD_clear();
  
  // OLEDがあったら表示するテスト
  #ifdef DISPLAY_OLED
    if (isAaddressExistInI2C(OLED_ADDR)){
      u8g2.begin(); 
      u8g2.setFlipMode(0);
      u8g2.setContrast(128);
      //u8g2.enableUTF8Print();  //UTF8文字を有効化
      u8g2.setFont(u8g2_font_helvR24_tr);//u8g2_font_helvR14_tr);//f);//(u8g2_font_courR18_tf);
      u8g2.clearBuffer();
      u8g2.firstPage();
      do {
        /* all graphics commands have to appear within the loop body. */
        u8g2.drawStr(0,25,"Hello World");
      } while ( u8g2.nextPage() );
      u8g2.sendBuffer();
    }
  #endif
  // for debug  
  //Serial.println(readRegDataFromRTC(0x01), BIN);
  //Serial.println(readRegDataFromRTC(0x0e), BIN);
  //Serial.println(readRegDataFromRTC(0x0f), DEC);
}

#define AYearH  0
#define AYearL  1
#define AMonthH 2
#define AMonthL 3
#define ADayH   4
#define ADayL   5
#define AWeek   6
#define AHourH  7
#define AHourL  8
#define AMinutH 9
#define AMinutL 10
int AdjustEachDigitOfClock;
#define DispSensor 0
#define DispClock 1
#define AdjustClock 2
int StateOfDisplay = DispSensor;
int PollingCounter = 0;
void loop() {
  // Get Switches status
  getSwitchStatus();

  //　Processes once every two seconds.
  if( PollingCounter == 0 ){
    switch (StateOfDisplay){
      case DispSensor:      // Display temperature and humidity
        LCD_cursor_off();
        DispTH();
        clearINT(); //If don't clear INT, stays power-on all the time.
        break;
        
      case DispClock:       // Display Clock
        DispCurrentTime();
        clearINT(); //If don't clear INT, stays power-on all the time.
        break;
        
      case AdjustClock:     // Does nothing while setting the time.
        break;
    }    
  }
  
  //Long press on yellow button to Toggle On/Off INT Timer
  if (SW[sw_Push].LongPressed) {
    if ( readTEbit() ){
      endTimerOperation();
      digitalWrite(8, HIGH);  // Turn off LED
    } else {
      generateINTeveryHour();
      digitalWrite(8, LOW);  // Turn-on the LED
    }
    SW[sw_Push].LongPressed = false;
    SW[sw_Push].LongPressing = false;
    SW[sw_Push].Pressed = false;
  }
  
  if (SW[sw_Push].LongPressing) {
    LCD_clear();
    SW[sw_Push].LongPressed = false;
    SW[sw_Push].Pressed = false;
  }
  
  //Press Center button to switch display 0:temperature or 1:clock
  if (SW[sw_Center].Pressed && (StateOfDisplay != AdjustClock) ){
    LCD_clear();
    if (StateOfDisplay == DispSensor) {
      StateOfDisplay = DispClock;
    } else StateOfDisplay = DispSensor;
    SW[sw_Center].LongPressed = false;
    SW[sw_Center].Pressed = false;
    PollingCounter = 1999;
  }
  //Press Center button to switch AdjustEachDigitOfClock
  if (SW[sw_Center].Pressed && (StateOfDisplay == AdjustClock) ){
    AdjustEachDigitOfClock--;
    if (AdjustEachDigitOfClock < 0) AdjustEachDigitOfClock = 10;
    switch (AdjustEachDigitOfClock) {
      case AYearH:  LCD_char_with_Cursor(0,0, (date_time[AAYear] >> 4) + 0x30 );  break;
      case AYearL:  LCD_char_with_Cursor(1,0, (date_time[AAYear] & 0x0f) + 0x30 );break;
      case AMonthH: LCD_char_with_Cursor(3,0, (date_time[AAMonth] >> 4) + 0x30 ); break;
      case AMonthL: LCD_char_with_Cursor(4,0, (date_time[AAMonth] & 0x0f) + 0x30 );break;
      case ADayH:   LCD_char_with_Cursor(6,0, (date_time[AADay] >> 4) + 0x30 );   break;
      case ADayL:   LCD_char_with_Cursor(7,0, (date_time[AADay] & 0x0f) + 0x30 ); break;
      case AWeek:
        LCD_char_with_Cursor(0,1, *Weekdays[date_time[AAWeek]] );
        LCD_char_with_Cursor(1,1, *(Weekdays[date_time[AAWeek]]+1) );
        break;
      case AHourH:  LCD_char_with_Cursor(3,1, (date_time[AAHour] >> 4) + 0x30 );  break;
      case AHourL:  LCD_char_with_Cursor(4,1, (date_time[AAHour] & 0x0f) + 0x30 );break;
      case AMinutH: LCD_char_with_Cursor(6,1, (date_time[AAMinute] >> 4) + 0x30 );break;
      case AMinutL: LCD_char_with_Cursor(7,1, (date_time[AAMinute] & 0x0f) + 0x30 );break;
    }
    SW[sw_Center].LongPressed = false;
    SW[sw_Center].Pressed = false;
    SW[sw_Center].Counter = 0;
    //PollingCounter = 1999;
  }
  //Long-press on center button to adjust clock
  if ( SW[sw_Center].LongPressed ){
    switch ( StateOfDisplay ){
      case DispClock:
        StateOfDisplay = AdjustClock;
        LCD_cursor_on();
        LoadTimeValuesIntoArray();
        AdjustEachDigitOfClock = AMinutL;
        LCD_char_with_Cursor(7,1, (date_time[AAMinute] & 0x0f) + 0x30 );
        break;
      case AdjustClock:
        /*** Set the time on RTC ***/
        StateOfDisplay = DispClock;
        SaveArrayValuesIntoRTC();
        break;
    }
    SW[sw_Center].LongPressing = false;
    SW[sw_Center].LongPressed = false;
    SW[sw_Center].Pressed = false;
    PollingCounter = 1999;
  }
  if ( SW[sw_Center].LongPressing ){
    if(StateOfDisplay != DispClock) LCD_clear();
  }  

  //Press sw_Lower / sw_Upper on AdjustClock mode
  if ( StateOfDisplay == AdjustClock ){
    if ( SW[sw_Lower].Pressed ){
      switch (AdjustEachDigitOfClock) {
        case AYearH:  digitHDown(0, 0, AAYear, 0xA0); break;
        case AYearL:  digitLDown(1, 0, AAYear);       break;
        case AMonthH: digitHDown(3, 0, AAMonth, 0x20);break;
        case AMonthL: digitLDown(4, 0, AAMonth);      break;
        case ADayH:   digitHDown(6, 0, AADay, 0x40);  break;
        case ADayL:   digitLDown(7, 0, AADay);        break;
        case AWeek:   digitWDown(0, 1, AAWeek);       break;
        case AHourH:  digitHDown(3, 1, AAHour, 0x30); break;
        case AHourL:  digitLDown(4, 1, AAHour);       break;
        case AMinutH: digitHDown(6, 1, AAMinute, 0x60); break;
        case AMinutL: digitLDown(7, 1, AAMinute);     break;
      }
      SW[sw_Lower].Counter = 0;
      SW[sw_Lower].Pressed = false;
      SW[sw_Lower].LongPressed = false;
    }
    if ( SW[sw_Upper].Pressed ){
      switch (AdjustEachDigitOfClock) {
        case AYearH:  digitHUp(0, 0, AAYear, 0xA0); break;
        case AYearL:  digitLUp(1, 0, AAYear);       break;
        case AMonthH: digitHUp(3, 0, AAMonth, 0x20);break;
        case AMonthL: digitLUp(4, 0, AAMonth);      break;
        case ADayH:   digitHUp(6, 0, AADay, 0x40);  break;
        case ADayL:   digitLUp(7, 0, AADay);        break;
        case AWeek:   digitWUp(0, 1, AAWeek);       break;
        case AHourH:  digitHUp(3, 1, AAHour, 0x30); break;
        case AHourL:  digitLUp(4, 1, AAHour);       break;
        case AMinutH: digitHUp(6, 1, AAMinute, 0x60); break;
        case AMinutL: digitLUp(7, 1, AAMinute);     break;
      }
      SW[sw_Upper].Counter = 0; 
      SW[sw_Upper].Pressed = false;
      SW[sw_Upper].LongPressed = false;
    }
  }

  PollingCounter++;
  if (PollingCounter > 1999) {
    PollingCounter = 0;
  }
  delay(1);
}
