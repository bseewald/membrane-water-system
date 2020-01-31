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
#include "esp_http_client.h"
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

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
#define WIFI_RETRY 30
const char* ssid     = "PROJETOAUTOFL";
const char* password = "******";

// ---------------------------------------------
// Server URL
// ---------------------------------------------
#define SERVER_RETRY 3
#define ERROR 404
bool send_again = false;

const char *post_url = "http://your-webserver.net/yourscript.php";

// ---------------------------------------------
// Ph and EC - I2C address
// ---------------------------------------------
// Ezo_board ph = Ezo_board(99, "PH");
// Ezo_board ec = Ezo_board(100, "EC");

// ---------------------------------------------
// UART 2 PINS
// ---------------------------------------------
#define RXD2 16
#define TXD2 17

HardwareSerial serial_balance(2);

// ---------------------------------------------
// DS18B20 Temperature Sensor
// ---------------------------------------------
#define ONE_WIRE_BUS 5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temp_sensors(&oneWire);
DeviceAddress temp_sensor_feed = { 0x28, 0x92, 0x98, 0x79, 0x97, 0x7, 0x3, 0xAD };
DeviceAddress temp_sensor_permeate = { 0x28, 0x26, 0x2B, 0x79, 0x97, 0x2, 0x3, 0x88 };

// ---------------------------------------------
// EEPROM Mem Map
// ---------------------------------------------
#define EEPROM_SIZE 6



// ---------------------------------------------
// NTP
// ---------------------------------------------
WiFiUDP ntpUDP;
NTPClient time_client(ntpUDP);
String formatted_date;
String time_stamp;

// ---------------------------------------------
// Variables
// ---------------------------------------------
enum reading_step {REQUEST, READ};
enum reading_step current_step = REQUEST;

int return_code = 0;
unsigned long next_poll_time = 0;
unsigned long check_wifi = 60000;

const unsigned long reading_delay = 1000;     // how long we wait to receive a response, in milliseconds
const unsigned long loop_delay = 300000;      // collect loop time: 5min
const unsigned long retry_delay = 15000;


void setup()
{
  // SerialPrint
  Serial.begin(115200);
  delay(10);

  // Balance
  serial_balance.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(10);

  // Temp
  temp_sensors.begin();

  // I2C
  // Wire.begin();

  EEPROM.begin(EEPROM_SIZE);

  // WiFi network
  while(!Serial);
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(ssid);

  WiFi.begin(ssid, password);
  int i=0;
  while(WiFi.status() != WL_CONNECTED && i++<=WIFI_RETRY)
  {
    delay(1000);
    i=i+1;
    if(i > WIFI_RETRY){
      DEBUG_PRINTLN("Problem with wifi. Restarting .....");
      ESP.restart();
    }
  }
  DEBUG_PRINTLN("WiFi connected"); DEBUG_PRINT("IP address: "); DEBUG_PRINTLN(WiFi.localIP());

  // Initialize a NTPClient to get time
  time_client.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600 / GMT -1 = -3600
  time_client.setTimeOffset(-10800);
}

void loop()
{
  String values = "";
  int response = ERROR;

  if((WiFi.status() != WL_CONNECTED) && (millis() > check_wifi)) {
    DEBUG_PRINTLN("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    check_wifi = millis() + 60000;
    DEBUG_PRINTLN("WiFi connected"); DEBUG_PRINT("IP address: "); DEBUG_PRINTLN(WiFi.localIP());
  }

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

        // ph.send_read_cmd();
        // ec.send_read_cmd();

        // TODO: PRINT<CR> ... The same operation as pressing the [PRINT] key
        char command[] = "PRINT\r";
        serial_balance.write((uint8_t *)command, sizeof(command));

        // Set when the response will arrive
        next_poll_time = millis() + reading_delay;
        current_step = READ;
      }
      break;

    case READ:
      if (millis() >= next_poll_time) {

        // ---------------------------------------------
        // Read balance, temperature, pH and EC values
        // ---------------------------------------------
        values += "time=" + get_time_stamp();

        // TODO: Read balance info
        String weighing = "";
        while(serial_balance.available() > 0) {
          uint8_t byte_from_serial = serial_balance.read();
          weighing += (char*)byte_from_serial;
        }
        DEBUG_PRINT("WEIGHING: "); DEBUG_PRINTLN(weighing);
        // values += "&weighing=" + weighing;

        float feed = temp_sensors.getTempC(temp_sensor_feed);
        float permeate = temp_sensors.getTempC(temp_sensor_permeate);
        DEBUG_PRINT("TEMP FEED: "); DEBUG_PRINTLN(feed);
        DEBUG_PRINT("TEMP PERMEATE: "); DEBUG_PRINTLN(permeate);
        values += "&feed=" + String(feed) + "&permeate=" + String(permeate);

        // PH.receive_read_cmd();
        // DEBUG_PRINT(PH.get_name()); DEBUG_PRINT(": ");

        // if(reading_succeeded(PH) == true){                     // if the pH reading has been received and it is valid
        //   DEBUG_PRINTLN(PH.get_last_received_reading(), 2);
        //   values += "&ph=" + String(PH.get_last_received_reading(), 2);
        // }

        // EC.receive_read_cmd();
        // DEBUG_PRINT(EC.get_name()); DEBUG_PRINT(": ");

        // if(reading_succeeded(EC) == true){                    // if the EC reading has been received and it is valid
        //   DEBUG_PRINTLN(EC.get_last_received_reading(), 0);
        //   values += "&ec=" + String(EC.get_last_received_reading(), 2);
        // }

        // Next collect: +5 min
        next_poll_time =  millis() + loop_delay;              // update the time for the next reading loop
        current_step = REQUEST;

        // ---------------------------------------------
        // Send URL to server
        // ---------------------------------------------
        DEBUG_PRINTLN(values);
        int i = 0;
        while(i < SERVER_RETRY){
          response = post_data_server(values.c_str());
          DEBUG_PRINTLN(response);
          if(response == 200)
            break;
          i++;
          delay(retry_delay);
        }
        if(response != 200){
          // TODO: save in memory and try again next time
          DEBUG_PRINTLN("SAVE IN FLASH");
          // EEPROM.write(0, ledState);
          // EEPROM.commit();
          send_again = true;
        }
      }
      break;
  }
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


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      DEBUG_PRINTLN("HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      DEBUG_PRINTLN("HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      DEBUG_PRINTLN("HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      DEBUG_PRINTLN("HTTP_EVENT_ON_HEADER");
      // DEBUG_PRINTLN("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      DEBUG_PRINTLN("HTTP_EVENT_ON_DATA");
      // DEBUG_PRINTLN("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      // if (!esp_http_client_is_chunked_response(evt->client)) {
      //   // Write out data
      //   printf("%.*s", evt->data_len, (char*)evt->data);
      // }
      break;
    case HTTP_EVENT_ON_FINISH:
      DEBUG_PRINTLN("HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      DEBUG_PRINTLN("HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}

int post_data_server(const char *post_data)
{
  int response = 404;
  esp_err_t res = ESP_OK;
  esp_http_client_handle_t http_client;
  esp_http_client_config_t config_client = {0};

  config_client.url = post_url;
  config_client.event_handler = _http_event_handler;
  config_client.method = HTTP_METHOD_POST;

  http_client = esp_http_client_init(&config_client);

  esp_http_client_set_post_field(http_client, post_data, strlen(post_data));

  esp_err_t err = esp_http_client_perform(http_client);
  if (err == res) {
    response = 200;
  }
  DEBUG_PRINT("esp_http_client_get_status_code: ");
  DEBUG_PRINTLN(esp_http_client_get_status_code(http_client));

  esp_http_client_cleanup(http_client);
  return response;
}

String get_time_stamp(){
  while(!time_client.update()) {
    time_client.forceUpdate();
  }
  // The formatted_date comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formatted_date = time_client.getFormattedDate();
  int split = formatted_date.indexOf("T");
  time_stamp = formatted_date.substring(split+1, formatted_date.length()-1);
  DEBUG_PRINT("HOUR: "); DEBUG_PRINTLN(time_stamp);
  return time_stamp;
}