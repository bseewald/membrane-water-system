/*
 * Membrane Water System - Library for collecting sensors information.
 *
 * Created by Bruna Seewald, January 22, 2020.
 * Copyright (c) 2020 Bruna Seewald.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <WiFi.h>
#include <Ezo_i2c.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"

// ---------------------------------------------
// Debug options
// ---------------------------------------------
#define DEBUG

#ifdef DEBUG
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

// ---------------------------------------------
// Wifi credentials
// ---------------------------------------------
const char* ssid     = "PROJETOAUTOFL";
const char* password = "2020projetoautoFL";

// ---------------------------------------------
// Ph and EC - I2C address
// ---------------------------------------------
Ezo_board ph = Ezo_board(99, "PH");
// Ezo_board ec = Ezo_board(100, "EC");

// ---------------------------------------------
// UART 2 PINS
// ---------------------------------------------
#define RXD2 16
#define TXD2 17
HardwareSerial serial_balance(2);
const uint8_t GETWEIGHT[] = { 0x50, 0x52, 0x49, 0x4E, 0x54, 0x0D }; // PRINT<CR>

// ---------------------------------------------
// DS18B20 Temperature Sensor
// ---------------------------------------------
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temp_sensors(&oneWire);
DeviceAddress temp_sensor_feed = { 0x28, 0x92, 0x98, 0x79, 0x97, 0x7, 0x3, 0xAD };
DeviceAddress temp_sensor_permeate = { 0x28, 0x26, 0x2B, 0x79, 0x97, 0x2, 0x3, 0x88 };

// ---------------------------------------------
// RTC
// ---------------------------------------------
RTC_DS3231 rtc;

// ---------------------------------------------
// NTP
// ---------------------------------------------
WiFiUDP ntp_udp;
NTPClient time_client(ntp_udp);

// ---------------------------------------------
// LED
// ---------------------------------------------
#define LED 15

// ---------------------------------------------
// SD Card
// ---------------------------------------------
#define SD_CS 5

// ---------------------------------------------
// Variables
// ---------------------------------------------
enum reading_step {REQUEST, READ};
enum reading_step current_step = REQUEST;

unsigned long next_poll_time = 0;
const unsigned long reading_delay = 1000;     // how long we wait to receive a response, in milliseconds
const unsigned long loop_delay = 300000;      // collect loop time: 5min
// const unsigned long loop_delay = 30000;


void setup()
{
  // SerialPrint
  Serial.begin(115200);
  delay(10);

  // LED
  pinMode(LED, OUTPUT);

  // Balance
  serial_balance.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(10);

  // Temp
  temp_sensors.begin();

  // I2C
  Wire.begin();

  // Initialize SD card
  SD.begin(SD_CS);
  DEBUG_PRINTLN("Initializing SD card...");
  if(!SD.begin(SD_CS)) {
    DEBUG_PRINTLN("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    DEBUG_PRINTLN("No SD card attached");
  }

  // WiFi network
  while(!Serial);

  DEBUG_PRINT("Connecting to "); DEBUG_PRINTLN(ssid);
  WiFi.begin(ssid, password);
  delay(10000);
  DEBUG_PRINTLN("WiFi connected"); DEBUG_PRINT("IP address: "); DEBUG_PRINTLN(WiFi.localIP());

  // NTP
  time_client.begin();
  time_client.setTimeOffset(-10800);  // GMT +1 = 3600 / GMT -1 = -3600

  // RTC
  if (!rtc.begin()) {
    DEBUG_PRINTLN("Couldn't find RTC");
    while (1);
  }
  RTC_Valid();
}

void loop()
{
  String values = "";

  // ---------------------------------------------
  // Collect info from sensors
  // ---------------------------------------------
  switch(current_step) {
    case REQUEST:
      if (millis() >= next_poll_time){

        // --------------------------------------------------------------
        // Send the command to get balance, temperature, pH and EC values
        // --------------------------------------------------------------
        temp_sensors.requestTemperatures();

        ph.send_read_cmd();
        // ec.send_read_cmd();

        // PRINT<CR> ... The same operation as pressing the [PRINT] key
        serial_balance.write(GETWEIGHT, sizeof(GETWEIGHT));

        // Set when the response will arrive
        next_poll_time = millis() + reading_delay;
        current_step = READ;
      }
      break;

    case READ:
      if (millis() >= next_poll_time) {

        // LED ON
        digitalWrite(LED, HIGH);

        String _time = get_timestamp();
        values += "time=" + _time;

        // BALANCE
        String weighing = "";
        char c;
        while(serial_balance.available() > 0) {
          c = char(serial_balance.read());
          if(c != ' ' && c != '[' && c != ']' && c != 'g'){
            weighing += c;
          }
        }
        values += "&weighing=" + weighing;

        // TEMPERATURE
        float feed = temp_sensors.getTempC(temp_sensor_feed);
        float permeate = temp_sensors.getTempC(temp_sensor_permeate);
        values += "&feed=" + String(feed) + "&permeate=" + String(permeate);

        // pH
        ph.receive_read_cmd();
        DEBUG_PRINT(ph.get_name()); DEBUG_PRINT(": ");
        if(reading_succeeded(ph) == true){
          DEBUG_PRINTLN(ph.get_last_received_reading(), 2);
          values += "&ph=" + String(ph.get_last_received_reading(), 2);
        }

        // EC
        // ec.receive_read_cmd();
        // DEBUG_PRINT(ec.get_name()); DEBUG_PRINT(": ");
        // if(reading_succeeded(ec) == true){
        //   DEBUG_PRINTLN(ec.get_last_received_reading(), 0);
        //   values += "&ec=" + String(ec.get_last_received_reading(), 2);
        // }

        // Next collect: +5 min
        next_poll_time =  millis() + loop_delay;    // update the time for the next reading loop
        current_step = REQUEST;

        DEBUG_PRINTLN(values);

        // ---------------------------------------------
        // Save in SD Card
        // ---------------------------------------------
        DEBUG_PRINTLN("SAVE IN SD CARD");
        save_in_file(_time,
                    weighing,
                    String(feed),
                    String(permeate),
                    String(ph.get_last_received_reading(), 2),
                    "0.0");
                    // String(ec.get_last_received_reading(), 2)

        // LED OFF
        delay(5000);
        digitalWrite(LED, LOW);
      }
      break;
  }
}

// ---------------------------------------------
// RTC Setup
// ---------------------------------------------
void RTC_Update(){

  // NTP update
  time_client.update();

  unsigned long secs = time_client.getEpochTime();
  unsigned long rawTime = secs / 86400L;  // in days
  unsigned long days = 0, year = 1970;
  uint8_t month;
  static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};

  while((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
    year++;
  rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
  days=0;
  for (month=0; month<12; month++) {
    uint8_t monthLength;
    if (month==1) { // february
      monthLength = LEAP_YEAR(year) ? 29 : 28;
    } else {
      monthLength = monthDays[month];
    }
    if (rawTime < monthLength) break;
    rawTime -= monthLength;
  }

  DateTime now = DateTime(uint16_t(year), ++month, uint8_t(++rawTime), time_client.getHours(), time_client.getMinutes(), time_client.getSeconds());
  rtc.adjust(now);
}

void RTC_Valid(){
  if(rtc.lostPower()) {
    DEBUG_PRINTLN("RTC lost power, lets set the time!");
    RTC_Update();
  }
}

String get_timestamp(){
  DateTime now = rtc.now();
  char timestamp[10];
  sprintf(timestamp, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(timestamp);
}

String get_datestamp(){
  DateTime now = rtc.now();
  char datestamp[11];
  sprintf(datestamp, "%04d-%02d-%02d", now.year(), now.month(), now.day());
  return String(datestamp);
}

void save_in_file(String _time, String _weight, String _feed, String _permeate, String _ph, String _ec){
  String filename = "/" + get_datestamp() + ".csv";
  String _values = _time + "," + _weight + "," + _feed + "," + _permeate + "," + _ph + "," + _ec + "\n";

  // Create a file on the SD card and write the data labels
  File file = SD.open(filename, "a+");
  if(!file) {
    DEBUG_PRINTLN("Failed to open file");
  }
  else {
    if(file.print(_values)) {
      DEBUG_PRINTLN("Message saved");
    } else {
    DEBUG_PRINTLN("Message not saved");
    }
  }
  file.close();
  return;
}

// this function makes sure that when we get a reading
// we know if it was valid or if we got an error
bool reading_succeeded(Ezo_board &Sensor) {
  switch (Sensor.get_error()) {
    case Ezo_board::SUCCESS:
      return true;

    case Ezo_board::FAIL:
      DEBUG_PRINTLN("Failed");
      return false;

    case Ezo_board::NOT_READY:     // if the reading was taken to early, the command has not yet finished calculating
      DEBUG_PRINTLN("Pending");
      return false;

    case Ezo_board::NO_DATA:
      DEBUG_PRINTLN("No Data");
      return false;

    default:                      // if none of the above happened
     return false;
  }
}
