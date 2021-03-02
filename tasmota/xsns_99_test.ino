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

bool test_sensor_ready = false;

const char TEST_SENSOR_RESPONSE[] PROGMEM = "{\"Sensor99\":{\"status\":%i}}";


/********************************************************************************************/

uint16_t Test_Read() {
  return 0;
}

void Test_Detect(void) {
    test_sensor_ready = true;
}

void Test_Every_250MSecond(void) {
}

void Test_Every_Second(void) {
}

void Test_Show(bool json) {
}

bool Test_Command(void)
{
  bool serviced = false;
  uint8_t paramcount = 0;
  int sttaus = 0;
  if (XdrvMailbox.data_len > 0) {
    paramcount=1;
    serviced = true;
    AddLog(LOG_LEVEL_INFO, PSTR("TST: %d args - return %d"),XdrvMailbox.data_len,serviced);
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("TST: no args - return %d"),serviced);
    return serviced;
  }

  Response_P(TEST_SENSOR_RESPONSE, 0);


  #if 0
  char argument[XdrvMailbox.data_len];
  for (uint32_t ca=0;ca<XdrvMailbox.data_len;ca++) {
    if ((' ' == XdrvMailbox.data[ca]) || ('=' == XdrvMailbox.data[ca])) { XdrvMailbox.data[ca] = ','; }
    if (',' == XdrvMailbox.data[ca]) { paramcount++; }
  }
  UpperCase(XdrvMailbox.data,XdrvMailbox.data);
  if (!strcmp(ArgV(argument, 1),"RESET"))  {  MCP230xx_Reset(1); return serviced; }
  if (!strcmp(ArgV(argument, 1),"RESET1")) {  MCP230xx_Reset(1); return serviced; }
  if (!strcmp(ArgV(argument, 1),"RESET2")) {  MCP230xx_Reset(2); return serviced; }
  if (!strcmp(ArgV(argument, 1),"RESET3")) {  MCP230xx_Reset(3); return serviced; }
  if (!strcmp(ArgV(argument, 1),"RESET4")) {  MCP230xx_Reset(4); return serviced; }
#ifdef USE_MCP230xx_OUTPUT
  if (!strcmp(ArgV(argument, 1),"RESET5")) {  MCP230xx_Reset(5); return serviced; }
  if (!strcmp(ArgV(argument, 1),"RESET6")) {  MCP230xx_Reset(6); return serviced; }
#endif // USE_MCP230xx_OUTPUT

  if (!strcmp(ArgV(argument, 1),"INTPRI")) {
    if (paramcount > 1) {
      uint8_t intpri = atoi(ArgV(argument, 2));
      if ((intpri >= 0) && (intpri <= 20)) {
        Settings.mcp230xx_int_prio = intpri;
        Response_P(MCP230XX_INTCFG_RESPONSE,"PRI",99,Settings.mcp230xx_int_prio);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
        return serviced;
      }
    } else { // No parameter was given for INTPRI so we return the current configured value
      Response_P(MCP230XX_INTCFG_RESPONSE,"PRI",99,Settings.mcp230xx_int_prio);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
      return serviced;
    }
  }

  if (!strcmp(ArgV(argument, 1),"INTTIMER")) {
    if (paramcount > 1) {
      uint8_t inttim = atoi(ArgV(argument, 2));
      if ((inttim >= 0) && (inttim <= 3600)) {
        Settings.mcp230xx_int_timer = inttim;
        MCP230xx_CheckForIntCounter(); // update register on whether or not we should be counting interrupts
        Response_P(MCP230XX_INTCFG_RESPONSE,"TIMER",99,Settings.mcp230xx_int_timer);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
        return serviced;
      }
    } else { // No parameter was given for INTTIM so we return the current configured value
      Response_P(MCP230XX_INTCFG_RESPONSE,"TIMER",99,Settings.mcp230xx_int_timer);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
      return serviced;
    }
  }

  if (!strcmp(ArgV(argument, 1),"INTDEF")) {
    if (paramcount > 1) {
      uint8_t pin = atoi(ArgV(argument, 2));
      if (pin < mcp230xx_pincount) {
        if (pin == 0) {
          if (!strcmp(ArgV(argument, 2), "0")) validpin=true;
        } else {
          validpin = true;
        }
      }
      if (validpin) {
        if (paramcount > 2) {
          uint8_t intdef = atoi(ArgV(argument, 3));
          if ((intdef >= 0) && (intdef <= 15)) {
            Settings.mcp230xx_config[pin].int_report_defer=intdef;
            if (Settings.mcp230xx_config[pin].int_count_en) {
              Settings.mcp230xx_config[pin].int_count_en=0;
              MCP230xx_CheckForIntCounter();
              AddLog(LOG_LEVEL_INFO, PSTR("*** WARNING *** - Disabled INTCNT for pin D%i"),pin);
            }
            Response_P(MCP230XX_INTCFG_RESPONSE,"DEF",pin,Settings.mcp230xx_config[pin].int_report_defer);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
            return serviced;
          } else {
            serviced=false;
            return serviced;
          }
        } else {
          Response_P(MCP230XX_INTCFG_RESPONSE,"DEF",pin,Settings.mcp230xx_config[pin].int_report_defer);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
          return serviced;
        }
      }
      serviced = false;
      return serviced;
    } else {
      serviced = false;
      return serviced;
    }
  }

  if (!strcmp(ArgV(argument, 1),"INTCNT")) {
    if (paramcount > 1) {
      uint8_t pin = atoi(ArgV(argument, 2));
      if (pin < mcp230xx_pincount) {
        if (pin == 0) {
          if (!strcmp(ArgV(argument, 2), "0")) validpin=true;
        } else {
          validpin = true;
        }
      }
      if (validpin) {
        if (paramcount > 2) {
          uint8_t intcnt = atoi(ArgV(argument, 3));
          if ((intcnt >= 0) && (intcnt <= 1)) {
            Settings.mcp230xx_config[pin].int_count_en=intcnt;
            if (Settings.mcp230xx_config[pin].int_report_defer) {
              Settings.mcp230xx_config[pin].int_report_defer=0;
              AddLog(LOG_LEVEL_INFO, PSTR("*** WARNING *** - Disabled INTDEF for pin D%i"),pin);
            }
            if (Settings.mcp230xx_config[pin].int_report_mode < 3) {
              Settings.mcp230xx_config[pin].int_report_mode=3;
              AddLog(LOG_LEVEL_INFO, PSTR("*** WARNING *** - Disabled immediate interrupt/telemetry reporting for pin D%i"),pin);
            }
            if ((Settings.mcp230xx_config[pin].int_count_en) && (!Settings.mcp230xx_int_timer)) {
              AddLog(LOG_LEVEL_INFO, PSTR("*** WARNING *** - INTCNT enabled for pin D%i but global INTTIMER is disabled!"),pin);
            }
            MCP230xx_CheckForIntCounter(); // update register on whether or not we should be counting interrupts
            Response_P(MCP230XX_INTCFG_RESPONSE,"CNT",pin,Settings.mcp230xx_config[pin].int_count_en);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
            return serviced;
          } else {
            serviced=false;
            return serviced;
          }
        } else {
          Response_P(MCP230XX_INTCFG_RESPONSE,"CNT",pin,Settings.mcp230xx_config[pin].int_count_en);  // "{\"MCP230xx_INT%s\":{\"D_%i\":%i}}";
          return serviced;
        }
      }
      serviced = false;
      return serviced;
    } else {
      serviced = false;
      return serviced;
    }
  }

  if (!strcmp(ArgV(argument, 1),"INTRETAIN")) {
    if (paramcount > 1) {
      uint8_t pin = atoi(ArgV(argument, 2));
      if (pin < mcp230xx_pincount) {
        if (pin == 0) {
          if (!strcmp(ArgV(argument, 2), "0")) validpin=true;
        } else {
          validpin = true;
        }
      }
      if (validpin) {
        if (paramcount > 2) {
          uint8_t int_retain = atoi(ArgV(argument, 3));
          if ((int_retain >= 0) && (int_retain <= 1)) {
            Settings.mcp230xx_config[pin].int_retain_flag=int_retain;
            Response_P(MCP230XX_INTCFG_RESPONSE,"INT_RETAIN",pin,Settings.mcp230xx_config[pin].int_retain_flag);
            MCP230xx_CheckForIntRetainer();
            return serviced;
          } else {
            serviced=false;
            return serviced;
          }
        } else {
          Response_P(MCP230XX_INTCFG_RESPONSE,"INT_RETAIN",pin,Settings.mcp230xx_config[pin].int_retain_flag);
          return serviced;
        }
      }
      serviced = false;
      return serviced;
    } else {
      serviced = false;
      return serviced;
    }
  }

  uint8_t pin = atoi(ArgV(argument, 1));

  if (pin < mcp230xx_pincount) {
    if (0 == pin) {
      if (!strcmp(ArgV(argument, 1), "0")) validpin=true;
    } else {
      validpin=true;
    }
  }
  if (validpin && (paramcount > 1)) {
    if (!strcmp(ArgV(argument, 2), "?")) {
      uint8_t port = 0;
      if (pin > 7) { port = 1; }
      uint8_t portdata = MCP230xx_readGPIO(port);
      char pulluptxtr[7],pinstatustxtr[7];
      char intmodetxt[9];
      sprintf(intmodetxt,IntModeTxt(Settings.mcp230xx_config[pin].int_report_mode));
      sprintf(pulluptxtr,ConvertNumTxt(Settings.mcp230xx_config[pin].pullup));
#ifdef USE_MCP230xx_OUTPUT
      uint8_t pinmod = Settings.mcp230xx_config[pin].pinmode;
      sprintf(pinstatustxtr,ConvertNumTxt(portdata>>(pin-(port*8))&1,pinmod));
      Response_P(MCP230XX_SENSOR_RESPONSE,pin,pinmod,pulluptxtr,intmodetxt,pinstatustxtr);
#else // not USE_MCP230xx_OUTPUT
      sprintf(pinstatustxtr,ConvertNumTxt(portdata>>(pin-(port*8))&1));
      Response_P(MCP230XX_SENSOR_RESPONSE,pin,Settings.mcp230xx_config[pin].pinmode,pulluptxtr,intmodetxt,pinstatustxtr);
#endif //USE_MCP230xx_OUTPUT
      return serviced;
    }
#ifdef USE_MCP230xx_OUTPUT
    if (Settings.mcp230xx_config[pin].pinmode >= 5 && paramcount == 2) {
      uint8_t pincmd = Settings.mcp230xx_config[pin].pinmode - 5;
      uint8_t relay_no = 0;
      for (relay_no = 0; relay_no < mcp230xx_pincount ; relay_no ++) {
        if ( mcp230xx_outpinmapping[relay_no] == pin) break;
      }
      relay_no = TasmotaGlobal.devices_present - mcp230xx_oldoutpincount + relay_no +1;
      if ((!strcmp(ArgV(argument, 2), "ON")) || (!strcmp(ArgV(argument, 2), "1"))) {
        ExecuteCommandPower(relay_no, 1, SRC_IGNORE);
        return serviced;
      }
      if ((!strcmp(ArgV(argument, 2), "OFF")) || (!strcmp(ArgV(argument, 2), "0"))) {
        ExecuteCommandPower(relay_no, 0, SRC_IGNORE);
        return serviced;
      }
      if ((!strcmp(ArgV(argument, 2), "T")) || (!strcmp(ArgV(argument, 2), "2")))  {
        ExecuteCommandPower(relay_no, 2, SRC_IGNORE);
        return serviced;
      }
    }
#endif // USE_MCP230xx_OUTPUT
    uint8_t pinmode = 0;
    uint8_t pullup = 0;
    uint8_t intmode = 0;
    if (paramcount > 1) {
      pinmode = atoi(ArgV(argument, 2));
    }
    if (paramcount > 2) {
      pullup = atoi(ArgV(argument, 3));
    }
    if (paramcount > 3) {
      intmode = atoi(ArgV(argument, 4));
    }
#ifdef USE_MCP230xx_OUTPUT
    if ((pin < mcp230xx_pincount) && (pinmode > 0) && (pinmode < 7) && (pullup < 2) && (paramcount > 2)) {
#else // not use OUTPUT
    if ((pin < mcp230xx_pincount) && (pinmode > 0) && (pinmode < 5) && (pullup < 2) && (paramcount > 2)) {
#endif // USE_MCP230xx_OUTPUT
      Settings.mcp230xx_config[pin].pinmode=pinmode;
      Settings.mcp230xx_config[pin].pullup=pullup;
      if ((pinmode > 1) && (pinmode < 5)) {
        if ((intmode >= 0) && (intmode <= 3)) {
          Settings.mcp230xx_config[pin].int_report_mode=intmode;
        }
      } else {
        Settings.mcp230xx_config[pin].int_report_mode=3; // Int mode not valid for pinmodes other than 2 through 4
      }
      MCP230xx_ApplySettings();
      uint8_t port = 0;
      if (pin > 7) { port = 1; }
      uint8_t portdata = MCP230xx_readGPIO(port);
      char pulluptxtc[7], pinstatustxtc[7];
      char intmodetxt[9];
      sprintf(pulluptxtc,ConvertNumTxt(pullup));
      sprintf(intmodetxt,IntModeTxt(Settings.mcp230xx_config[pin].int_report_mode));
#ifdef USE_MCP230xx_OUTPUT
      sprintf(pinstatustxtc,ConvertNumTxt(portdata>>(pin-(port*8))&1,Settings.mcp230xx_config[pin].pinmode));
#else  // not USE_MCP230xx_OUTPUT
      sprintf(pinstatustxtc,ConvertNumTxt(portdata>>(pin-(port*8))&1));
#endif // USE_MCP230xx_OUTPUT
      Response_P(MCP230XX_SENSOR_RESPONSE,pin,pinmode,pulluptxtc,intmodetxt,pinstatustxtc);
      return serviced;
    }
  } else {
    serviced=false; // no valid pin was used
    return serviced;
  }
  #endif
  return serviced;
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns99(uint8_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    Test_Detect();
  }
  else if (test_sensor_ready) {
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
        if (XSNS_99 == XdrvMailbox.index) {
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
