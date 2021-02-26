/*
  xsns_84_tof10120.ino - TOF10120 sensor support for Tasmota

  Copyright (C) 2021  Cyril Pawelko and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_TEST_SENSOR
/*********************************************************************************************\
 *
\*********************************************************************************************/

#define XSNS_99                     99

bool test_sensor.ready = false;


/********************************************************************************************/

uint16_t Test_Read() {
  return 0;
}

void Test_Detect(void) {
    test_sensor.ready = true;
}

void Test_Every_250MSecond(void) {
}

void Test_Every_Second(void) {
}

void Test_Show(bool json) {
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns84(uint8_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    Test_Detect();
  }
  else if (test_sensor.ready) {
    switch (function) {
      case FUNC_EVERY_250_MSECOND:
        Test_Every_250MSecond();
        break;
     case FUNC_EVERY_SECOND:
        Test_Every_Second();
        break;
      case FUNC_JSON_APPEND:
        Test_Show(1);
        break;
      case FUNC_COMMAND_SENSOR:
        if (XSNS_29 == XdrvMailbox.index) {
          result = Test_Command();
        }
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        Test_Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_TOF10120
