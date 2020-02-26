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
#include "FS.h"
#include "SD.h"
#include <SPI.h>
// #include <SPIFFS.h>
// #include "ESP32_MailClient.h"
// #include "esp_http_client.h"

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
// Server URL
// ---------------------------------------------
// #define SERVER_RETRY 1 // TODO: alterar valor
// #define ERROR 404
// bool send_again = false;
// const char *post_url = "http://13.68.215.66:58721";
// TODO: alterar IP liberado no servidor AWS


// ---------------------------------------------
// SMTP data
// ---------------------------------------------
// SMTPData smtp_data;


// ---------------------------------------------
// Wifi credentials
// ---------------------------------------------
// #define WIFI_RETRY 30
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
String formatted_date;
String time_stamp;
String day_stamp;

// ---------------------------------------------
// LED
// ---------------------------------------------
#define LED 15

// ---------------------------------------------
// Sleep
// ---------------------------------------------
// Conversion factor for micro seconds to seconds
uint64_t uS_TO_S_FACTOR = 1000000;
// Sleep for 10 minutes = 600 seconds
uint64_t TIME_TO_SLEEP = 600;

// ---------------------------------------------
// SD Card
// ---------------------------------------------
#define SD_CS 5

// ---------------------------------------------
// Variables
// ---------------------------------------------
enum reading_step {REQUEST, READ};
enum reading_step current_step = REQUEST;

int return_code = 0;
unsigned long next_poll_time = 0;
unsigned long check_wifi = 60000;

const unsigned long reading_delay = 1000;     // how long we wait to receive a response, in milliseconds
// const unsigned long loop_delay = 300000;      // collect loop time: 5min
const unsigned long loop_delay = 30000;
// const unsigned long retry_delay = 15000;
const unsigned long retry_delay = 1000;


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

  // SPIFFS partition
  // if(!SPIFFS.begin(true)){
  //   DEBUG_PRINTLN("An Error has occurred while mounting SPIFFS");
  // }

  // WiFi network
  while(!Serial);

  DEBUG_PRINT("Connecting to "); DEBUG_PRINTLN(ssid);
  WiFi.begin(ssid, password);
  // int i=0;
  // while(WiFi.status() != WL_CONNECTED && i++<=WIFI_RETRY)
  // {
  //   delay(1000);
  //   i=i+1;
  //   if(i > WIFI_RETRY){
  //     DEBUG_PRINTLN("Problem with wifi. Restarting .....");
  //     ESP.restart();
  //   }
  // }
  delay(10000);
  DEBUG_PRINTLN("WiFi connected"); DEBUG_PRINT("IP address: "); DEBUG_PRINTLN(WiFi.localIP());

  // NTP
  time_client.begin();
  // GMT +1 = 3600 / GMT -1 = -3600
  time_client.setTimeOffset(-10800);

  // RTC
  if (!rtc.begin()) {
    DEBUG_PRINTLN("Couldn't find RTC");
    while (1);
  }
  RTC_Valid();
  // RTC_Update();
}

void loop()
{
  String values = "";
  // int response = ERROR;

  // ---------------------------------------------
  // Reconnect wifi
  // ---------------------------------------------
  // if((WiFi.status() != WL_CONNECTED) && (millis() > check_wifi)) {
  //   DEBUG_PRINTLN("Reconnecting to WiFi...");
  //   WiFi.disconnect();
  //   WiFi.begin(ssid, password);
  //   check_wifi = millis() + 60000;
  //   DEBUG_PRINTLN("WiFi connected"); DEBUG_PRINT("IP address: "); DEBUG_PRINTLN(WiFi.localIP());
  // }

  // ---------------------------------------------
  // Send files to email (if necessary)
  // ---------------------------------------------
  // int currentHour = time_client.getHours();
  // int currentMin = time_client.getMinutes();
  // if(send_again && (currentHour == 23) && (currentMin > 50)){
  //   send_email();
  // }

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
        DEBUG_PRINTLN("Sending PRINT command...");
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
        DEBUG_PRINT("WEIGHING: "); DEBUG_PRINTLN(weighing);
        values += "&weighing=" + weighing;

        // TEMPERATURE
        float feed = temp_sensors.getTempC(temp_sensor_feed);
        float permeate = temp_sensors.getTempC(temp_sensor_permeate);
        // DEBUG_PRINT("TEMP FEED: "); DEBUG_PRINTLN(feed);
        // DEBUG_PRINT("TEMP PERMEATE: "); DEBUG_PRINTLN(permeate);
        values += "&feed=" + String(feed) + "&permeate=" + String(permeate);

        // pH
        ph.receive_read_cmd();
        DEBUG_PRINT(ph.get_name()); DEBUG_PRINT(": ");
        if(reading_succeeded(ph) == true){
          DEBUG_PRINTLN(ph.get_last_received_reading(), 2);
          values += "&ph=" + String(ph.get_last_received_reading(), 2);
        }

        // EC
        // EC.receive_read_cmd();
        // DEBUG_PRINT(EC.get_name()); DEBUG_PRINT(": ");
        // if(reading_succeeded(EC) == true){
        //   DEBUG_PRINTLN(EC.get_last_received_reading(), 0);
        //   values += "&ec=" + String(EC.get_last_received_reading(), 2);
        // }

        // Next collect: +5 min
        next_poll_time =  millis() + loop_delay;              // update the time for the next reading loop
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
                    // String(EC.get_last_received_reading(), 2)

        // LED OFF
        delay(5000);
        digitalWrite(LED, LOW);

        // ---------------------------------------------
        // Send URL to server
        // ---------------------------------------------
        // int i = 0;
        // while(i < SERVER_RETRY){
        //   response = post_data_server(values.c_str());
        //   if(response == 200)
        //     break;
        //   i++;
        //   delay(retry_delay);
        // }
        // if(response != 200){
        //   DEBUG_PRINTLN("SAVE IN SPIFFS");
        //   save_in_file(_time,
        //               weighing,
        //               String(feed),
        //               String(permeate),
        //               String(ph.get_last_received_reading(), 2),
        //               "0.0");
        //               // String(EC.get_last_received_reading(), 2)
        //   send_again = true;
        // }
      }
      break;
  }
}

// ---------------------------------------------
// RTC Setup
// ---------------------------------------------
void RTC_Update(){
  time_client.update();

  // 2018-05-28T16:00:13Z
  int split = formatted_date.indexOf("T");
  formatted_date = time_client.getFormattedDate();
  day_stamp = formatted_date.substring(0, split);
  time_stamp = formatted_date.substring(split+1, formatted_date.length()-1);

  char* token = strtok(day_stamp, "-");
  while(token != NULL) {
      printf("%s\n", token);
      token = strtok(NULL, "-");
  }


  DateTime now = DateTime(, time_stamp.c_str());
  DEBUG_PRINTLN(String(now.year(), DEC) + ":" + String(now.month(), DEC) + ":" + String(now.day(), DEC));
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
  // timestamp += String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);
  return String(timestamp);
}

String get_datestamp(){
  DateTime now = rtc.now();
  char datestamp[11];
  sprintf(datestamp, "%04d-%02d-%02d", now.year(), now.month(), now.day());
  // datestamp += String(now.year(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.day(), DEC);
  return String(datestamp);
}

String get_ntp_date(){
  unsigned long rawTime = (secs ? secs : this->getEpochTime()) / 86400L;  // in days
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
  String monthStr = ++month < 10 ? "0" + String(month) : String(month); // jan is month 1
  String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime); // day of month
  return String(year) + "-" + monthStr + "-" + dayStr +
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

// esp_err_t _http_event_handler(esp_http_client_event_t *evt){
//   switch (evt->event_id) {
//     case HTTP_EVENT_ERROR:
//       DEBUG_PRINTLN("HTTP_EVENT_ERROR");
//       break;
//     case HTTP_EVENT_ON_CONNECTED:
//       DEBUG_PRINTLN("HTTP_EVENT_ON_CONNECTED");
//       break;
//     case HTTP_EVENT_HEADER_SENT:
//       DEBUG_PRINTLN("HTTP_EVENT_HEADER_SENT");
//       break;
//     case HTTP_EVENT_ON_HEADER:
//       DEBUG_PRINTLN("HTTP_EVENT_ON_HEADER");
//       break;
//     case HTTP_EVENT_ON_DATA:
//       DEBUG_PRINTLN("HTTP_EVENT_ON_DATA");
//       break;
//     case HTTP_EVENT_ON_FINISH:
//       DEBUG_PRINTLN("HTTP_EVENT_ON_FINISH");
//       break;
//     case HTTP_EVENT_DISCONNECTED:
//       DEBUG_PRINTLN("HTTP_EVENT_DISCONNECTED");
//       break;
//   }
//   return ESP_OK;
// }

// int post_data_server(const char *post_data){
//   int response = 404;
//   esp_err_t res = ESP_OK;
//   esp_http_client_handle_t http_client;
//   esp_http_client_config_t config_client = {0};

//   config_client.url = post_url;
//   config_client.event_handler = _http_event_handler;
//   config_client.method = HTTP_METHOD_POST;

//   http_client = esp_http_client_init(&config_client);

//   esp_http_client_set_post_field(http_client, post_data, strlen(post_data));

//   esp_err_t err = esp_http_client_perform(http_client);
//   if (err == res) {
//     response = 200;
//   }
//   DEBUG_PRINT("esp_http_client_get_status_code: ");
//   DEBUG_PRINTLN(esp_http_client_get_status_code(http_client));

//   esp_http_client_cleanup(http_client);
//   return response;
// }

// void save_in_file(String _time, String _weight, String _feed, String _permeate, String _ph, String _ec){
//   String filename = "/" + get_date_stamp() + ".txt";
//   File file = SPIFFS.open(filename, "a+");
//   if(!file) {
//     DEBUG_PRINTLN("There was an error opening the file for writing");
//   }
//   // _time,_weight,_feed,_permeate,_ph,_ec
//   String _values = _time + "," + _weight + "," + _feed + "," + _permeate + "," + _ph + "," + _ec + "\n";
//   if(file.print(_values)) {
//     DEBUG_PRINTLN("File was written");
//   }
//   file.close();
//   return;
// }

// String get_time_stamp(){
//   if(!time_client.update()) {
//     time_client.forceUpdate();
//   }
//   // The formatted_date comes with the following format:
//   // 2018-05-28T16:00:13Z
//   // We need to extract date and time
//   formatted_date = time_client.getFormattedDate();
//   int split = formatted_date.indexOf("T");
//   time_stamp = formatted_date.substring(split+1, formatted_date.length()-1);
//   DEBUG_PRINT("HOUR: "); DEBUG_PRINTLN(time_stamp);
//   return time_stamp;
// }

// String get_date_stamp(){
//   if(!time_client.update()) {
//     time_client.forceUpdate();
//   }
//   formatted_date = time_client.getFormattedDate();
//   int split = formatted_date.indexOf("T");
//   day_stamp = formatted_date.substring(0, split);
//   DEBUG_PRINT("DAY: "); DEBUG_PRINTLN(day_stamp);
//   return day_stamp;
// }

// //Callback function to get the Email sending status
// void send_callback(SendStatus msg){
//   DEBUG_PRINTLN(msg.info());
// }

// void send_email(){
//   // Send email with all files (after 23h00)
//   DEBUG_PRINTLN("Sending email...");
//   // smtp_data.setDebug(true);
//   smtp_data.setLogin("smtp.gmail.com", 465, "mdautomatizada", "2020ProjetoMDautomatizada");
//   smtp_data.setSender("ESP32", "mdautomatizada@gmail.com");
//   //Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
//   smtp_data.setPriority("High");
//   smtp_data.setSubject("ESP32 Mail Sending");
//   smtp_data.setMessage("<div style=\"color:#ff0000;font-size:12px;\">Saved values - From ESP32</div>", true);
//   smtp_data.addRecipient("mdautomatizada@gmail.com");
//   smtp_data.setFileStorageType(MailClientStorageType::SPIFFS);

//   // Attach all files
//   File root = SPIFFS.open("/");
//   File file = root.openNextFile();
//   String path = "";
//   while(file){
//     path = file.name();
//     DEBUG_PRINT("FILE: "); DEBUG_PRINTLN(path);
//     smtp_data.addAttachFile(path);
//     file = root.openNextFile();
//   }
//   smtp_data.setSendCallback(send_callback);

//   //Start sending Email, can be set callback function to track the status
//   if(!MailClient.sendMail(smtp_data)){
//     DEBUG_PRINTLN("Error sending Email, " + MailClient.smtpErrorReason());
//   }
//   else{
//     // Send ok, remove files from SPIFFS
//     DEBUG_PRINTLN("Email sent!");
//     root = SPIFFS.open("/");
//     file = root.openNextFile();
//     while(file){
//       path = file.name();
//       DEBUG_PRINT(path);
//       if(SPIFFS.remove(path)){
//         DEBUG_PRINTLN(" - file deleted");
//       }
//       file = root.openNextFile();
//     }
//     send_again = false;
//   }

//   //Clear all data from Email object to free memory
//   smtp_data.empty();
//   return;
// }
