/*
  xdrv_87_esp32_gsmppp.ino - ESP32 PPP over 2G/3G/4G? modem

  Copyright (C) 2022 Barbudot

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

#ifdef ESP32
#if CONFIG_IDF_TARGET_ESP32
#ifdef USE_GSMPPP
/*********************************************************************************************\
\*********************************************************************************************/

#define XDRV_87           87

#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "sim800.h"
#include "bg96.h"
#include "sim7600.h"

#include "gsmppp_config.h"

#if defined(GSMPPP_MODEM_UART_FLOW_CONTROL_SW)
#define GSMPPP_MODEM_UART_FLOW_CONTROL  MODEM_FLOW_CONTROL_SW
#elif defined(GSMPPP_MODEM_UART_FLOW_CONTROL_HW)
#define GSMPPP_MODEM_UART_FLOW_CONTROL  MODEM_FLOW_CONTROL_HW
#else
#define GSMPPP_MODEM_UART_FLOW_CONTROL  MODEM_FLOW_CONTROL_NONE
#endif

#define D_LOG_GSMPPP    "GSM: "

struct gsmppp_data {
  EventGroupHandle_t event_group = nullptr;
  esp_netif_auth_type_t auth_type;
  esp_modem_dte_config_t config;
  modem_dte_t *dte = nullptr;
  modem_dce_t *dce = nullptr;
  esp_netif_config_t cfg;
  esp_netif_t *esp_netif = nullptr;
  void *modem_netif_adapter = nullptr;
  uint32_t rssi = 0, ber = 0;
  uint32_t voltage = 0, bcs = 0, bcl = 0;
} GSMPPP_Data;

static const int CONNECT_BIT = BIT0;
static const int STOP_BIT = BIT1;
static const int GOT_DATA_BIT = BIT2;

static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MODEM_EVENT_PPP_START:
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Modem PPP Started"));
        break;
    case ESP_MODEM_EVENT_PPP_STOP:
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Modem PPP Stopped"));
        xEventGroupSetBits(GSMPPP_Data.event_group, STOP_BIT);
        break;
    case ESP_MODEM_EVENT_UNKNOWN:
        AddLog(LOG_LEVEL_ERROR, PSTR(D_LOG_GSMPPP "Unknown line received: %s"), (char *)event_data);
        break;
    default:
        break;
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "PPP state changed event %d"), event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = *(esp_netif_t**)event_data;
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "User interrupted event from netif:0x%08X"), netif);
    }
}


static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_GSMPPP "IP event! %d"), event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Modem Connect to PPP Server"));
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "IP          : %_I"), &event->ip_info.ip);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Netmask     : %_I"), &event->ip_info.netmask);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Gateway     : %_I"), &event->ip_info.gw);
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Name Server1: %_I"), &dns_info.ip.u_addr.ip4);
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Name Server2: %_I"), &dns_info.ip.u_addr.ip4);
        xEventGroupSetBits(GSMPPP_Data.event_group, CONNECT_BIT);

        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "GOT ip event!!!"));
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Modem Disconnect from PPP Server"));
    } else if (event_id == IP_EVENT_GOT_IP6) {
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "GOT IPv6 event!"));

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Got IPv6 address " IPV6STR), IPV62STR(event->ip6_info.ip));
    }
}


void GsmPppInit(void) {
  if (!PinUsed(GPIO_GSMPPP_TX) && !PinUsed(GPIO_GSMPPP_RX)) {
    AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "No ETH MDC and/or ETH MDIO GPIO defined"));
    return;
  }

#ifdef GSMPPP_USERNAME
#if CONFIG_LWIP_PPP_PAP_SUPPORT
    GSMPPP_Data.auth_type = NETIF_PPP_AUTHTYPE_PAP;
#elif CONFIG_LWIP_PPP_CHAP_SUPPORT
    GSMPPP_Data.auth_type = NETIF_PPP_AUTHTYPE_CHAP;
#elif !defined(GSMPPP_MODEM_PPP_AUTH_NONE)
#error "Unsupported AUTH Negotiation"
#endif
#endif

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    GSMPPP_Data.event_group = xEventGroupCreate();

    /* create dte object */
    //GSMPPP_Data.
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    GSMPPP_Data.config.flow_control = GSMPPP_MODEM_UART_FLOW_CONTROL;
    GSMPPP_Data.config.tx_io_num = GSMPPP_MODEM_UART_TX_PIN;
    GSMPPP_Data.config.rx_io_num = GSMPPP_MODEM_UART_RX_PIN;
    GSMPPP_Data.config.rts_io_num = GSMPPP_MODEM_UART_RTS_PIN;
    GSMPPP_Data.config.cts_io_num = GSMPPP_MODEM_UART_CTS_PIN;
    GSMPPP_Data.config.rx_buffer_size = GSMPPP_MODEM_UART_RX_BUFFER_SIZE;
    GSMPPP_Data.config.tx_buffer_size = GSMPPP_MODEM_UART_TX_BUFFER_SIZE;
    GSMPPP_Data.config.event_queue_size = GSMPPP_MODEM_UART_EVENT_QUEUE_SIZE;
    GSMPPP_Data.config.event_task_stack_size = GSMPPP_MODEM_UART_EVENT_TASK_STACK_SIZE;
    GSMPPP_Data.config.event_task_priority = GSMPPP_MODEM_UART_EVENT_TASK_PRIORITY;
    GSMPPP_Data.config.dte_buffer_size = GSMPPP_MODEM_UART_RX_BUFFER_SIZE / 2;

    GSMPPP_Data.dte = esp_modem_dte_init(&GSMPPP_Data.config);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_set_event_handler(GSMPPP_Data.dte, modem_event_handler, ESP_EVENT_ANY_ID, NULL));

    // Init netif object
    GSMPPP_Data.cfg = ESP_NETIF_DEFAULT_PPP();
    GSMPPP_Data.esp_netif = esp_netif_new(&GSMPPP_Data.cfg);
    assert(GSMPPP_Data.esp_netif);

    GSMPPP_Data.modem_netif_adapter = esp_modem_netif_setup(GSMPPP_Data.dte);
    esp_modem_netif_set_default_handlers(GSMPPP_Data.modem_netif_adapter, GSMPPP_Data.esp_netif);

    GSMPPP_Data.dce = NULL;
    /* create dce object */
#if defined(GSMPPP_MODEM_DEVICE_SIM800)
        GSMPPP_Data.dce = sim800_init(GSMPPP_Data.dte);
#elif defined(GSMPPP_MODEM_DEVICE_BG96)
        GSMPPP_Data.dce = bg96_init(GSMPPP_Data.dte);
#elif defined(GSMPPP_MODEM_DEVICE_SIM7600)
        GSMPPP_Data.dce = sim7600_init(GSMPPP_Data.dte);
#else
#error "Unsupported DCE"
#endif
        assert(GSMPPP_Data.dce != NULL);
        ESP_ERROR_CHECK(GSMPPP_Data.dce->set_flow_ctrl(GSMPPP_Data.dce, GSMPPP_MODEM_UART_FLOW_CONTROL));
        ESP_ERROR_CHECK(GSMPPP_Data.dce->store_profile(GSMPPP_Data.dce));
        /* Print Module ID, Operator, IMEI, IMSI */
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Module: %s"), GSMPPP_Data.dce->name);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Operator: %s"), GSMPPP_Data.dce->oper);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "IMEI: %s"), GSMPPP_Data.dce->imei);
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "IMSI: %s"), GSMPPP_Data.dce->imsi);
        /* Get signal quality */
        ESP_ERROR_CHECK(GSMPPP_Data.dce->get_signal_quality(GSMPPP_Data.dce, &GSMPPP_Data.rssi, &GSMPPP_Data.ber));
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "rssi: %d, ber: %d"), GSMPPP_Data.rssi, GSMPPP_Data.ber);
        /* Get battery voltage */
        ESP_ERROR_CHECK(GSMPPP_Data.dce->get_battery_status(GSMPPP_Data.dce, &GSMPPP_Data.bcs, &GSMPPP_Data.bcl, &GSMPPP_Data.voltage));
        AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_GSMPPP "Battery voltage: %d mV"), GSMPPP_Data.voltage);
        /* setup PPPoS network parameters */
#if defined(GSMPPP_MODEM_PPP_AUTH_USERNAME) && (defined(CONFIG_LWIP_PPP_PAP_SUPPORT) || defined(CONFIG_LWIP_PPP_CHAP_SUPPORT))
        esp_netif_ppp_set_auth(GSMPPP_Data.esp_netif, GSMPPP_Data.auth_type, GSMPPP_MODEM_PPP_AUTH_USERNAME, GSMPPP_MODEM_PPP_AUTH_PASSWORD);
#endif
        /* attach the modem to the network interface */
        esp_netif_attach(GSMPPP_Data.esp_netif, GSMPPP_Data.modem_netif_adapter);
        /* Wait for IP address */
        xEventGroupWaitBits(GSMPPP_Data.event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

//IPAddress GsmPppIP(void) {
  //return ETH.localIP();
//}


/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

#if 0
#define D_CMND_ETHADDRESS   "Address"
#define D_CMND_ETHTYPE      "Type"
#define D_CMND_ETHCLOCKMODE "ClockMode"
#define D_CMND_ETHIPADDRESS D_CMND_IPADDRESS
#define D_CMND_ETHGATEWAY   D_JSON_GATEWAY
#define D_CMND_ETHNETMASK   D_JSON_SUBNETMASK
#define D_CMND_ETHDNS       D_JSON_DNSSERVER

const char kEthernetCommands[] PROGMEM = "Eth|"  // Prefix
  "ernet|" D_CMND_ETHADDRESS "|" D_CMND_ETHTYPE "|" D_CMND_ETHCLOCKMODE "|"
  D_CMND_ETHIPADDRESS "|" D_CMND_ETHGATEWAY "|" D_CMND_ETHNETMASK "|" D_CMND_ETHDNS ;

void (* const EthernetCommand[])(void) PROGMEM = {
  &CmndEthernet, &CmndEthAddress, &CmndEthType, &CmndEthClockMode,
  &CmndEthSetIpConfig, &CmndEthSetIpConfig, &CmndEthSetIpConfig, &CmndEthSetIpConfig };

#define ETH_PARAM_OFFSET 4                       // Offset of command index in above table of first CmndEthIpConfig

void CmndEthernet(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 1)) {
    Settings->flag4.network_ethernet = XdrvMailbox.payload;
    TasmotaGlobal.restart_flag = 2;
  }
  ResponseCmndStateText(Settings->flag4.network_ethernet);
}

void CmndEthAddress(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 31)) {
    Settings->eth_address = XdrvMailbox.payload;
    TasmotaGlobal.restart_flag = 2;
  }
  ResponseCmndNumber(Settings->eth_address);
}

void CmndEthType(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 2)) {
    Settings->eth_type = XdrvMailbox.payload;
    TasmotaGlobal.restart_flag = 2;
  }
  ResponseCmndNumber(Settings->eth_type);
}

void CmndEthClockMode(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 3)) {
    Settings->eth_clk_mode = XdrvMailbox.payload;
    TasmotaGlobal.restart_flag = 2;
  }
  ResponseCmndNumber(Settings->eth_clk_mode);
}

void CmndEthSetIpConfig(void) {
  uint32_t param_id = XdrvMailbox.command_code -ETH_PARAM_OFFSET;

  char cmnd_idx[2] = { 0 };
  if (3 == param_id) {                           // EthDnsServer
    if ((XdrvMailbox.index < 1) || (XdrvMailbox.index > 2)) {
      XdrvMailbox.index = 1;
    }
    cmnd_idx[0] = '0' + XdrvMailbox.index;
    param_id += XdrvMailbox.index -1;            // EthDnsServer2
  }

  if (XdrvMailbox.data_len) {
    uint32_t ipv4_address;
    if (ParseIPv4(&ipv4_address, XdrvMailbox.data)) {
      Settings->eth_ipv4_address[param_id] = ipv4_address;
      eth_config_change = 2;
    }
  }

  char network_address[22] = { 0 };
  if (0 == param_id) {
    if (!Settings->eth_ipv4_address[0]) {
      ext_snprintf_P(network_address, sizeof(network_address), PSTR(" (%_I)"), (uint32_t)ETH.localIP());
    }
  }
  Response_P(PSTR("{\"%s%s\":\"%_I%s\"}"), XdrvMailbox.command, cmnd_idx, Settings->eth_ipv4_address[param_id], network_address);
}
#endif // #if 0

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv87(uint8_t function) {
  bool result = false;

  switch (function) {
    case FUNC_EVERY_SECOND:
      //;
      break;
    case FUNC_COMMAND:
      //result = DecodeCommand(kEthernetCommands, EthernetCommand);
      break;
    case FUNC_INIT:
      GsmPppInit();
      break;
  }
  return result;
}

#endif  // USE_GSMPPP
#endif  // CONFIG_IDF_TARGET_ESP32
#endif  // ESP32
