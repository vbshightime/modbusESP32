#include <WiFi.h>
#include <BytebeamArduino.h>
#include "arduino_secrets.h"
#include "ModbusMaster.h"
ModbusMaster master;

#define MODBUS_DE_PIN   15
#define MODBUS_RE_PIN   4
#define MODBUS_RX_PIN 26 // Rx pin  
#define MODBUS_TX_PIN 27 // Tx pin 


// modbus stream name
char tempStream[] = "device_shadow";

// wifi credentials
const char* WIFI_SSID     = SECRET_SSID;
const char* WIFI_PASSWORD = SECRET_PASS;

// sntp credentials
const long  gmtOffset_sec = 19800;      // GMT + 5:30h
const int   daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";


void modbusPreTransmission()
{
  delay(500);
  digitalWrite(MODBUS_DE_PIN, HIGH);
    digitalWrite(MODBUS_RE_PIN,HIGH);
}

void modbusPostTransmission()
{
  digitalWrite(MODBUS_DE_PIN, LOW);
    digitalWrite(MODBUS_RE_PIN,LOW);
  delay(500);
}


// function to setup the wifi with predefined credentials
void setupWifi() {
  // set the wifi to station mode to connect to a access point
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID , WIFI_PASSWORD);

  Serial.println();
  Serial.print("Connecting to " + String(WIFI_SSID));

  // wait till chip is being connected to wifi  (Blocking Mode)
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }

  // now it is connected to the access point just print the ip assigned to chip
  Serial.println();
  Serial.print("Connected to " + String(WIFI_SSID) + ", Got IP address : ");
  Serial.println(WiFi.localIP());
}

// function to sync time from ntp server with predefined credentials
void syncTimeFromNtp() {
  // sync the time from ntp server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;

  // get the current time
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  // log the time info to serial :)
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println();
}

// function to get the time 
unsigned long long getEpochMillis() {
  time_t now;
  struct tm timeinfo;

  // get the current time i.e make sure the device is in sync with the ntp server
  if (!getLocalTime(&timeinfo)) {
    Serial.println("failed to obtain time");
    return 0;
  }

  // get the epoch time
  time(&now);

  // generate the epoch millis
  unsigned long long timeMillis = ((unsigned long long)now * 1000) + (millis() % 1000);

  return timeMillis;
}

// function to publish chip temperature to strem
bool publishModbus(char* stream) {
  static int sequence = 0;
  unsigned long long milliseconds = 0;
  const char* payload = "";
  String modbusStr = "";
  StaticJsonDocument<1024> doc;

  // get the current epoch millis
  milliseconds = getEpochMillis();

  // make sure you got the millis
  if(milliseconds == 0) {
    Serial.println("failed to get epoch millis");
    return false;
  }

  // increment the sequence counter
  sequence++;
  
  uint16_t temp = 0.0;
  uint16_t humid = 0.0;
  uint8_t read_temp_humid_reg;
  
  read_temp_humid_reg = master.readInputRegisters(1, 2);
  
  if (master.ku8MBSuccess == read_temp_humid_reg){
      temp = master.getResponseBuffer(0);
      humid = master.getResponseBuffer(1);
  }
  
  JsonArray modbusJsonArray = doc.to<JsonArray>();
  JsonObject modbusJsonObj_1 = modbusJsonArray.createNestedObject();

  modbusJsonObj_1["timestamp"]   = milliseconds;
  modbusJsonObj_1["sequence"]    = sequence;
  modbusJsonObj_1["temperature"] = temp;
  modbusJsonObj_1["Humid"] = humid;
  
  serializeJson(modbusJsonArray, modbusStr);
  payload = modbusStr.c_str();

  Serial.printf("publishing %s to %s\n", payload, stream);

  return Bytebeam.publishToStream(stream, payload);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(MODBUS_DE_PIN,OUTPUT);
  pinMode(MODBUS_RE_PIN,OUTPUT);
  digitalWrite(MODBUS_DE_PIN,LOW);
  digitalWrite(MODBUS_RE_PIN,LOW);
  Serial2.begin(9600, SERIAL_8E1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  Serial2.setTimeout(200);
  master.begin(1,Serial2);
  setupWifi();
  syncTimeFromNtp();
  // begin the bytebeam client
  if(!Bytebeam.begin()) {
    Serial.println("Bytebeam Client Initialization Failed.");
  } else {
    Serial.println("Bytebeam Client is Initialized Successfully.");
  }
  master.preTransmission(modbusPreTransmission);
}

void loop() {
  // put your main code here, to run repeatedly:
  // bytebeam client loop
  Bytebeam.loop();
  if(!publishModbus(tempStream)) {
    Serial.printf("Failed to publish chip temperature to %s", tempStream);
  }
  
  
  delay(1000);
}
