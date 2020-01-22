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
const char* password = "******";

// ---------------------------------------------
// I2C address and name
// ---------------------------------------------
Ezo_board PH = Ezo_board(99, "PH");
Ezo_board EC = Ezo_board(100, "EC");

// ---------------------------------------------
// Variables
// ---------------------------------------------
enum reading_step {REQUEST, READ};
enum reading_step current_step = REQUEST;

int return_code = 0;
unsigned long next_poll_time = 0;

const unsigned long reading_delay = 1000;     // how long we wait to receive a response, in milliseconds
const unsigned long loop_delay = 300000;      // collect loop time: 5min


void setup()
{
  // I2C and serial ports
  Wire.begin();
  Serial.begin(115200);

  // WiFi network
  while (!Serial) ;
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
}

void loop()
{
  // Collect info from sensors
  switch(current_step) {
    case REQUEST:
      if (millis() >= next_poll_time) {
        // TODO: temperature

        PH.send_read_cmd();
        EC.send_read_cmd();

        next_poll_time = millis() + reading_delay;         //set when the response will arrive
        current_step = READ;
      }
      break;

    case READ:
      if (millis() >= next_poll_time) {

        PH.receive_read_cmd();
        DEBUG_PRINT(PH.get_name()); DEBUG_PRINT(": ");

        if(reading_succeeded(PH) == true){                     // if the pH reading has been received and it is valid
          DEBUG_PRINTLN(PH.get_last_received_reading(), 2);
          //TODO: put value in URL API
        }

        EC.receive_read_cmd();
        DEBUG_PRINT(EC.get_name()); DEBUG_PRINT(": ");

        if(reading_succeeded(EC) == true){                    // if the EC reading has been received and it is valid
          DEBUG_PRINTLN(EC.get_last_received_reading(), 0);
          //TODO: put value in URL API
        }

        // Next collect: +5 min
        next_poll_time =  millis() + loop_delay;              // update the time for the next reading loop
        current_step = REQUEST;
      }
      break;
  }

    // TODO: send URL to a server
}

bool reading_succeeded(Ezo_board &Sensor) {                  // this function makes sure that when we get a reading we know if it was valid or if we got an error

  switch (Sensor.get_error()) {
    case Ezo_board::SUCCESS:
      return true;

    case Ezo_board::FAIL:
      Serial.print("Failed ");
      return false;

    case Ezo_board::NOT_READY:                              // if the reading was taken to early, the command has not yet finished calculating
      Serial.print("Pending ");
      return false;

    case Ezo_board::NO_DATA:
      Serial.print("No Data ");
      return false;

    default:                                                // if none of the above happened
     return false;
  }
}
