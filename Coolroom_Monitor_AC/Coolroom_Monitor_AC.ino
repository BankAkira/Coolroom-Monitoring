/*
=================
=  Connection   =
=================

OLED Display Module 128X64 0.96" I2C address 0x3C
  Vcc - > 3.3 / 5 V
  Gnd -> Gnd
  SCL -> A5 to Mega SCL//change when use with Mega
  SDA -> A4 to Mega SDA//change when use with Mega

RTC Module DS3231
  GND -> GND
  VCC -> +5V
  SDA -> A4 to Mega SDA//change when use with Mega
  SCL -> A5 to Mega SDL//change when use with Mega

SD Card Module
  CS   -> 10 to Mega 53/change when use with Mega
  SCK  -> 13 to Mega 52/change when use with Mega
  MOSI -> 11 to Mega 51/change when use with Mega
  MISO -> 12 to Mega 50/change when use with Mega
  VCC  -> 5V
  GND  -> GND

DHT22 inside
  +   -> +5V
  out -> 5
  -   -> GND

DHT22 outside
  +   -> +5V
  out -> 6
  -   -> GND

ACS711 Hall effective
  G     -> GND
  Vcc   -> 5V
  Vout  -> A1

Magnetic Sensor
  Vcc -> 5V
  Signal Out -> 3
  G -> GND

UC20 set jumper to auto start, remove other jumper
   UC20 TX to Mega RX1 (middle jumper)
   UT20 RX to Mega TX1 (middle jumper)

DS18B20 temp 4 sensor
	VCC -> 5V
	G -> GND
	ds18b20_0 -> 30
	ds18b20_1 -> 31
	ds18b20_2 -> 32
	ds18b20_3 -> 33

DC Volt Meter
	+ -> 5V
	- -> GND
	S -> A0

//Buzzer
  + -> 7
  - -> GND
  
*/
#include <SPI.h>
#include <Wire.h>
#define Country_Voltage 220
#define site_name "Hort-IL-Demo_02"
#define Temp_alert 40
#define Volt_alert 11.5

//Restart Program
#include <avr/wdt.h>

void restart(uint8_t prescaller) {
  wdt_enable(prescaller);
  while(1){}
}


//------- UC20 ------------------//
#include "TEE_UC20.h"
#include "internet.h"
#include "File.h"
#include "http.h"

INTERNET net;
UC_FILE file;
HTTP http;
int sent_result = 0;
int register_flag = 0;
int gsmqc = 0;

//Set APN
#define APN "www.dtac.co.th" //for DTAC Sim
#define USER ""
#define PASS ""
//#define APN "internet" //for AIS Sim
//#define USER ""
//#define PASS ""
//#define APN "internet" //for TrueMove Sim
//#define USER "True"
//#define PASS "true"

String rslt_msg = "";

void clear_rslt_buf() {
  rslt_msg = "";
}

void debug(String data) {
  Serial.println(data);
}

void data_out (char data) {
  if (data != '\r' && data != '\n')
  {
    rslt_msg = rslt_msg + data;
    Serial.println(rslt_msg);
  }
}

void read_file(String pattern,String file_name) {
  file.DataOutput = data_out;
  file.ReadFile(pattern,file_name);
}

int init_net() {
  Serial.print(F("GetOperatot --> "));
  Serial.println(gsm.GetOperator());
  Serial.print(F("SignalQuality --> "));
  gsmqc = gsm.SignalQuality();
  if(gsmqc == 99){ return 0;}
  Serial.println(gsmqc);
  net.DisConnect();
  net.Configure(APN,USER,PASS);
  Serial.println(F("Connect net"));
  net.Connect();
}

int Sent_Data(String url) {
  clear_rslt_buf();
  Serial.println(url);
  Serial.println("Start HTTP");
  http.begin(1);
  delay(1000);
  Serial.println("Send HTTP GET");
  http.url(url);
  int get_rslt = http.get();
  Serial.println(get_rslt);
  if(get_rslt != 200) {
      net.DisConnect();
      return 0;
  }
  Serial.println(F("Crear data in RAM"));
  file.Delete(RAM,"*");
  Serial.println(F("Save HTTP Response to RAM"));
  http.SaveResponseToMemory(RAM,"web.hml");
  Serial.println(F("Read data in RAM"));
  read_file(RAM,"web.hml");
  return 1;
}

String gen_url_data(float t_in1, float t_in2, float t_in3, float t_in4, float t_in5, float t_out, float rh1, float rh2, double adc, double volt)
{
  String url = "http://api.jateworkspace.com/coolroom/data.php?";
  url = url+"t-in1="+t_in1+"&t-in2="+t_in2+"&t-in3="+t_in3+"&t-in4="+t_in4+"&t-in5="+t_in5+"&t-out="+t_out+"&rh1="+rh1+"&rh2="+rh2+"&kWh="+String(adc, 5)+"&name="+site_name+"&volt="+volt;
  return url;
}

String gen_url_door(int door_status, unsigned long timer = 0) {
  if (door_status == HIGH)
  {
    door_status = 0;
  }
  else
  {
    door_status = 1;
  }
  String url = "http://api.jateworkspace.com/coolroom/door.php?";
  if (timer == 0)
  {url = url+"name="+site_name+"&status="+door_status;}
  else
  {url = url+"name="+site_name+"&status="+door_status+"&timer="+timer;}
    return url;
}

String gen_url_register() {
  String url ="http://api.jateworkspace.com/coolroom/register.php?name=";
  url = url+site_name+"&t-alert="+Temp_alert+"&v-alert="+Volt_alert;
  return url;
}


//-------- LCD Display -----------
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F, 20, 4);


//--------- RTC ------------------
#include "RTClib.h"
RTC_DS1307 RTC;


//----------- SD Card ------------
#include <SD.h>
const int chipSelect = 53;
File dataFile;
bool fileError = 0;
bool sdError = 0;
int currentMin = 0;
int lastMin = 0;
unsigned int RecNum = 0;

void write_log_header(bool fileError, bool sdError)
{
  if(!fileError and !sdError){
      dataFile.print("RecNum");
      dataFile.print(',');
      dataFile.print("Server_time");
      dataFile.print(',');
      dataFile.print("Date");
      dataFile.print(',');
      dataFile.print("Time");
      dataFile.print(',');
      dataFile.print("Temp_in_1");
      dataFile.print(',');
      dataFile.print("Temp_in_2");
      dataFile.print(',');
      dataFile.print("Temp_in_3");
      dataFile.print(',');
      dataFile.print("Temp_in_4");
      dataFile.print(',');
      dataFile.print("Temp_in_5");
      dataFile.print(',');               
      dataFile.print("RH_in");
      dataFile.print(','); 
      dataFile.print("Temp_out");
      dataFile.print(',');
      dataFile.print("RH_out");
      dataFile.print(',');
      dataFile.print("Current");
      dataFile.print(',');
      dataFile.print("Voltage");
      dataFile.print(',');
      dataFile.println("Door_open");
      dataFile.flush();
  } 
}


//----------- Sensor ------------------
#include <DHT.h>
#define T_RH_IN 5
#define T_RH_OUT 6
#define T_RH_IN_TYPE DHT22
#define T_RH_OUT_TYPE DHT22
DHT trhin(T_RH_IN, T_RH_IN_TYPE);
DHT trhout(T_RH_OUT, T_RH_OUT_TYPE);

//Magnetic Sensor
#include <StopWatch.h>
#define imagnetic 3
StopWatch sw_door(StopWatch::SECONDS);
int old_status = HIGH;
unsigned long timer;

//DC Volt Meter
double volt = 0;

float fmap(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float get_val_vdc() {
  return fmap(analogRead(A0), 0, 1023, 0.0, 25.0);
}

//ACS711 Hall Effective
#include "EmonLib.h"
EnergyMonitor emon1;
double meter_offset = 0.022;
const int inPinI = A1;
double adc = 0;

float get_val_amp() {
  return ((emon1.calcIrms(1500) - meter_offset) * 2) / 1.1399;
}

//DS18B20
#include "OneWire.h"
#include "DallasTemperature.h"
#define ONE_WIRE_PIN_0 30
#define ONE_WIRE_PIN_1 31
#define ONE_WIRE_PIN_2 32
#define ONE_WIRE_PIN_3 33

OneWire one_wire_pin0(ONE_WIRE_PIN_0);
OneWire one_wire_pin1(ONE_WIRE_PIN_1);
OneWire one_wire_pin2(ONE_WIRE_PIN_2);
OneWire one_wire_pin3(ONE_WIRE_PIN_3);
DallasTemperature ds18b20_0(&one_wire_pin0);
DallasTemperature ds18b20_1(&one_wire_pin1);
DallasTemperature ds18b20_2(&one_wire_pin2);
DallasTemperature ds18b20_3(&one_wire_pin3);

float temp_in_2, temp_in_3, temp_in_4, temp_in_5;

void init_ds18b20() {
  ds18b20_0.begin();
  ds18b20_1.begin();
  ds18b20_2.begin();
  ds18b20_3.begin();
}

void get_TempC_from_ds18b20() {
  ds18b20_0.requestTemperatures();
  ds18b20_1.requestTemperatures();
  ds18b20_2.requestTemperatures();
  ds18b20_3.requestTemperatures();
  temp_in_2 = ds18b20_0.getTempCByIndex(0);
  temp_in_3 = ds18b20_1.getTempCByIndex(0);
  temp_in_4 = ds18b20_2.getTempCByIndex(0);
  temp_in_5 = ds18b20_3.getTempCByIndex(0);
}

//Buzzer
const int buzzer_pin = 7;

void alert(int x) {
  if(x){
      tone(buzzer_pin, 500, 3000);
      delay(500);
  }
  noTone(buzzer_pin);
}


void setup()
{
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  RTC.begin();
//  if (!RTC.isrunning()) {
//    Serial.println("RTC is NOT Running!");
//  }
  Serial.println("pass RTC");
  DateTime now = RTC.now();
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);
  if (!SD.begin(chipSelect)){
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Card failed, or not present");
    Serial.println("Card failed, or not present");
    sdError = 1;
  }
  lcd.setCursor(0, 1);
  lcd.print("Card Initialized..");
  Serial.println("Pass check SD Card");
  if (!sdError) {
    dataFile = SD.open("CRlog.csv", FILE_WRITE);
    if (!dataFile) {
      lcd.clear();
      lcd.print("error open file");
      fileError = 1;
    }
  }
  write_log_header(fileError, sdError);
  Serial.println("Pass write log header");
  trhin.begin();
  Serial.println("Pass begin TRH-In");
  trhout.begin();
  Serial.println("Pass begin TRH-out");
  pinMode(imagnetic, INPUT);
  Serial.println("Pass check magnatic switch");
  init_ds18b20();
  Serial.println("Pass init ds18b20");
  emon1.current(1, 6.621);
  Serial.println("Pass AC Amp Meter");
  gsm.begin(&Serial1,9600);
  Serial.println("Pass GSM begin");
  gsm.Event_debug = debug;
  Serial.println("Pass GSM debug");
  gsm.PowerOn();
  while(gsm.WaitReady()){}
  init_net();
  delay(3000);
  if (register_flag == 0) {
    register_flag = Sent_Data(gen_url_register());
  }
}


void loop()
{
  if (register_flag == 0) {
    Sent_Data(gen_url_register());
  }

  DateTime now = RTC.now();
  currentMin = now.minute();
  float rh1 = trhin.readHumidity();
  float temp_in_1 = trhin.readTemperature();
  float rh2 = trhout.readHumidity();
  float temp_out = trhout.readTemperature();
  get_TempC_from_ds18b20();
  Serial.println(temp_in_2);
  Serial.println(temp_in_3);
  Serial.println(temp_in_4);
  Serial.println(temp_in_5);
  adc = get_val_amp();
  volt = Country_Voltage;
  if (temp_in_1 > Temp_alert || temp_in_2 > Temp_alert || temp_in_3 > Temp_alert || temp_in_4 > Temp_alert || temp_in_5 > Temp_alert || volt < Volt_alert) {
    alert(1);
  }
  alert(0);
  int Status = digitalRead(imagnetic);
  if (Status == HIGH) {
    if (old_status == LOW) {
      sw_door.stop();
      timer = sw_door.elapsed();
      if (!Sent_Data(gen_url_door(Status,timer))) {
        restart(WDTO_2S);
      }
      sw_door.reset();
      old_status = Status;
      timer = 0;
    }
  }
  if (Status == LOW) {
    if (old_status == HIGH) {
      if (!Sent_Data(gen_url_door(Status))) {
        restart(WDTO_2S);
      }
      sw_door.start();
      old_status = Status;
    }
    delay(500);
    if (Sent_Data(gen_url_data(temp_in_1, temp_in_2, temp_in_3, temp_in_4, temp_in_5, temp_out, rh1, rh2, adc, volt))) {
      if (!fileError && !sdError) {
        dataFile.print(RecNum);
        dataFile.print(','); 
          if (sent_result) {
            dataFile.print(rslt_msg);
            dataFile.print(',');
          }
          else {
            dataFile.print("NULL");
            dataFile.print(',');
          }
          dataFile.print(now.year(), DEC);
          dataFile.print('/');
          dataFile.print(now.month(), DEC);
          dataFile.print('/');
          dataFile.print(now.day(), DEC);
          dataFile.print(',');
          dataFile.print(now.hour(), DEC);
          dataFile.print(':');
          dataFile.print(now.minute(), DEC);
          dataFile.print(',');
          dataFile.print(temp_in_1,7);
          dataFile.print(',');
          dataFile.print(temp_in_2,7);
          dataFile.print(',');
          dataFile.print(temp_in_3,7);
          dataFile.print(',');
          dataFile.print(temp_in_4,7);
          dataFile.print(',');
          dataFile.print(temp_in_5,7);
          dataFile.print(',');
          dataFile.print(rh1,7);
          dataFile.print(',');
          dataFile.print(temp_out,7);
          dataFile.print(',');
          dataFile.print(rh2,7);
          dataFile.print(',');
          dataFile.print(adc,7);
          dataFile.print(',');
          dataFile.println(volt,7);
          dataFile.flush();
          RecNum++;
      }
    }
    else {
      restart(WDTO_2S);
    }
  }
  lcd.clear();
  lcd.print(now.year(), DEC);
  lcd.print('/');
  if (now.month() < 10) {lcd.print('0');}
  lcd.print(now.month(), DEC);
  lcd.print('/');
  if (now.day() < 10) {lcd.print('0');}
  lcd.print(now.day(), DEC);
  lcd.print(' ');
  if (now.hour() < 10) {lcd.print('0');}
  lcd.print(now.hour(), DEC);
  lcd.print(':');
  if (now.minute() < 10) {lcd.print('0');}
  lcd.print(now.minute(), DEC);
  lcd.setCursor(0, 1);
  lcd.print("RHout");
  lcd.print((int) rh2);
  lcd.print(" RHin");
  lcd.print((int) rh1);
  if (Status == HIGH) {lcd.print(" CLOSE");}
  if (Status == LOW) {lcd.print(" OPEN");}
  lcd.setCursor(0, 2);
  lcd.print("Tout");
  lcd.print((int) temp_out);
  lcd.print(" A");
  lcd.print(adc, 1);
  lcd.print(" V");
  lcd.print(volt, 1);
  lcd.setCursor(0, 3);
  lcd.print("Tin ");
  lcd.print((int) temp_in_1);
  lcd.print(" ");
  lcd.print((int) temp_in_2);
  lcd.print(" ");
  lcd.print((int) temp_in_3);
  lcd.print(" ");
  lcd.print((int) temp_in_4);
  lcd.print(" ");
  lcd.print("F");
  lcd.print((int) temp_in_5);
  if (currentMin != lastMin) {
    lastMin = currentMin;
    sent_result = 0;
    int sent_counter = 0;
    for (sent_counter; sent_counter < 10 && sent_result == 0; sent_counter++) {
      sent_result = Sent_Data(gen_url_data(temp_in_1, temp_in_2, temp_in_3, temp_in_4, temp_in_5, temp_out, rh1, rh2, adc, volt));
    }
    if(!sent_result) {
      restart(WDTO_2S);
    }
    if (!fileError && !sdError) {
      dataFile.print(RecNum);
      dataFile.print(','); 
      if (sent_result) {
        dataFile.print(rslt_msg);
        dataFile.print(',');
      }
      else {
        dataFile.print("NULL");
        dataFile.print(',');
      }
      dataFile.print(now.year(), DEC);
      dataFile.print('/');
      dataFile.print(now.month(), DEC);
      dataFile.print('/');
      dataFile.print(now.day(), DEC);
      dataFile.print(',');
      dataFile.print(now.hour(), DEC);
      dataFile.print(':');
      dataFile.print(now.minute(), DEC);
      dataFile.print(',');
      dataFile.print(temp_in_1,7);
      dataFile.print(',');
      dataFile.print(temp_in_2,7);
      dataFile.print(',');
      dataFile.print(temp_in_3,7);
      dataFile.print(',');
      dataFile.print(temp_in_4,7);
      dataFile.print(',');
      dataFile.print(temp_in_5,7);
      dataFile.print(',');
      dataFile.print(rh1,7);
      dataFile.print(',');
      dataFile.print(temp_out,7);
      dataFile.print(',');
      dataFile.print(rh2,7);
      dataFile.print(',');
      dataFile.print(adc,7);
      dataFile.print(',');
      dataFile.println(volt,7);
      dataFile.flush();
      RecNum++;
    }
  }
  delay(200);
}
