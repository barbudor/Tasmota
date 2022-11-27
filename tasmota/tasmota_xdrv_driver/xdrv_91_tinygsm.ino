/*
  xdrv_91_tinygsm.ino - Module for Broadband operations

  Copyright (C) 2022 Barbudor
  Dependant on https://github.com/vshymanskyy/TinyGSM/

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

#ifdef USE_TINYGSM

#define XDRV_91           91

/********************************************************************************************************
 * Compile Time  Configuration
 */

#if !defined(TINY_GSM_MODEM_SIM800) \
  && !defined(TINY_GSM_MODEM_SIM808) \
  && !defined(TINY_GSM_MODEM_SIM868) \
  && !defined(TINY_GSM_MODEM_SIM900) \
  && !defined(TINY_GSM_MODEM_SIM7000) \
  && !defined(TINY_GSM_MODEM_SIM7000SSL) \
  && !defined(TINY_GSM_MODEM_SIM7080) \
  && !defined(TINY_GSM_MODEM_SIM5360) \
  && !defined(TINY_GSM_MODEM_SIM7600) \
  && !defined(TINY_GSM_MODEM_UBLOX) \
  && !defined(TINY_GSM_MODEM_SARAR4) \
  && !defined(TINY_GSM_MODEM_M95) \
  && !defined(TINY_GSM_MODEM_BG96) \
  && !defined(TINY_GSM_MODEM_A6) \
  && !defined(TINY_GSM_MODEM_A7) \
  && !defined(TINY_GSM_MODEM_M590) \
  && !defined(TINY_GSM_MODEM_MC60) \
  && !defined(TINY_GSM_MODEM_MC60E) \
  && !defined(TINY_GSM_MODEM_ESP8266) \
  && !defined(TINY_GSM_MODEM_XBEE) \
  && !defined(TINY_GSM_MODEM_SEQUANS_MONARCH)
#error ERROR: One of TINY_GSM_MODEM_xxx must be defined in platformio_tasmota_cenv.ini
#endif

#ifndef TINYGSM_BAUDRATE
#define TINYGSM_BAUDRATE    9600
#endif

/********************************************************************************************************
 * Library Include
 */

#include <TinyGsmClient.h>

/********************************************************************************************************
 * Global private data
 */

struct TINYGSM_DATA {
  TasmotaSerial *tas_serial;
  // pins
  uint8_t     pin_tx, pin_rx;
} *TinyGSM = nullptr;


/********************************************************************************************************
 * Low level operations
 */


/********************************************************************************************************
 * Driver initialisation
 */

void TinyGSMInit(void) 
{
  if (PinUsed(GPIO_TINYGSM_TX) && PinUsed(GPIO_TINYGSM_RX)) {
    // allocate data structure
    TinyGSM = (struct TINYGSM_DATA*)calloc(1, sizeof(struct TINYGSM_DATA));
    if (TinyGSM) {
      TinyGSM->pin_tx = Pin(GPIO_TINYGSM_TX);
      TinyGSM->pin_rx = Pin(GPIO_TINYGSM_RX);
      // allocate serial
      TinyGSM->tas_serial = new TasmotaSerial(TinyGSM->pin_rx, TinyGSM->pin_tx);
      if (TinyGSM->tas_serial->begin(TINYGSM_BAUDRATE)) {
        if (TinyGSM->tas_serial->hardwareSerial()) { ClaimSerial(); }
      } else {
        AddLog(LOG_LEVEL_INFO, PSTR("TGSM: Serial initialization error"));
        free(TinyGSM);
        TinyGSM = nullptr;
        return;
      }


    } else {
      AddLog(LOG_LEVEL_INFO, PSTR("TGSM: Out of memory"));
    }
  }
}

/********************************************************************************************************
 * Driver operations
 */

void TinyGSMLoop()
{
}

/********************************************************************************************************
 * Driver Results
 */

void TinyGSMShow(bool json)
{
  if (json) {
  }
#ifdef USE_WEBSERVER
  else {
  }
#endif
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv91(uint32_t function) {
  bool result = false;

  if (FUNC_PRE_INIT == function) {
    TinyGSMInit();
  } else if (TinyGSM) {
    switch (function) {
      case FUNC_EVERY_50_MSECOND:
      //case FUNC_EVERY_250_MSECOND:
        TinyGSMLoop();
        break;
      case FUNC_JSON_APPEND:
        TinyGSMShow(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        TinyGSMShow(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_TINYGSM
