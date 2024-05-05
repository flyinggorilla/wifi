// #include "sdkconfig.h"
#include "Wifi.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <esp_event.h>
#include <esp_now.h>
#include "esp_crc.h"
#include <esp_sntp.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

static char tag[] = "Wifi";

Wifi Wifi::wifi;

// NEW
// #include "freertos/event_groups.h"
// static EventGroupHandle_t s_wifi_event_group;

Wifi::Wifi()
{
	muConnectedClients = 0;
	mbConnected = false;
}

void wifiEventHandler(void *ctx, esp_event_base_t base, int32_t id, void *event_data)
{
	return ((Wifi *)ctx)->OnEvent(base, id, event_data);
}

String Wifi::GetLocalAddress()
{
	char sBuf[20];
	GetLocalAddress(sBuf);
	return String(sBuf);
}

void Wifi::GetLocalAddress(char *sBuf)
{

	esp_netif_ip_info_t ip;
	esp_netif_get_ip_info(mStaNetif, &ip);
	sprintf(sBuf, "%d.%d.%d.%d", IP2STR(&ip.ip));
}

void Wifi::GetGWAddress(char *sBuf)
{
	esp_netif_ip_info_t ip;
	esp_netif_get_ip_info(mStaNetif, &ip);
	sprintf(sBuf, "%d.%d.%d.%d", IP2STR(&ip.gw));
}

esp_ip4_addr_t Wifi::GetGWAddress()
{
	esp_netif_ip_info_t ip;
	esp_netif_get_ip_info(mStaNetif, &ip);
	return ip.gw;
}

void Wifi::GetNetmask(char *sBuf)
{
	esp_netif_ip_info_t ip;
	esp_netif_get_ip_info(mStaNetif, &ip);
	sprintf(sBuf, "%d.%d.%d.%d", IP2STR(&ip.netmask));
}

void Wifi::GetMac(__uint8_t uMac[6])
{
	esp_wifi_get_mac(WIFI_IF_STA, uMac);
}

void Wifi::GetApInfo(int8_t &riRssi, uint8_t &ruChannel)
{
	wifi_ap_record_t info;

	esp_wifi_sta_get_ap_info(&info);
	riRssi = info.rssi;
	ruChannel = info.primary;
}

void Wifi::StartAPMode(String &rsSsid, String &rsPass, String &rsHostname)
{
	msApSsid = rsSsid;
	msApPass = rsPass;
	msHostname = rsHostname;
	Start(WIFI_MODE_AP);
}

void Wifi::AddSTACredentials(String &rsSsid, String &rsPass, String &rsUser)
{
	mStationWifiCreds.push_back({rsSsid, rsPass, rsUser});
}

void Wifi::StartSTAMode(String &rsHostname)
{
	if (mStationWifiCreds.size() == 0)
	{
		ESP_LOGE(tag, "No STA credentials added!");
		return;
	}
	muCurrentStationWifiCreds = 0;
	msHostname = rsHostname;
	Start(WIFI_MODE_STA);
}

void Wifi::StartSTAMode(String &rsSsid, String &rsPass, String &rsHostname)
{
	mStationWifiCreds.push_back({rsSsid, rsPass, ""});
	muCurrentStationWifiCreds = 0;
	msHostname = rsHostname;
	Start(WIFI_MODE_STA);
}

void Wifi::StartSTAModeEnterprise(String &rsSsid, String &rsUser, String &rsPass, String &rsCA, String &rsHostname)
{
	mStationWifiCreds.push_back({rsSsid, rsPass, ""});
	muCurrentStationWifiCreds = 0;
	msCA = rsCA;
	msHostname = rsHostname;
	Start(WIFI_MODE_STA);
}

void Wifi::StartAPSTAMode(String &rsApSsid, String &rsApPass, String &rsApHostname)
{
	if (mStationWifiCreds.size() == 0)
	{
		ESP_LOGE(tag, "No STA credentials added!");
		return;
	}
	muCurrentStationWifiCreds = 0;
	msApSsid = rsApSsid;
	msApPass = rsApPass;
	msHostname = rsApHostname;
	Start(WIFI_MODE_APSTA);
}

void Wifi::StartAPSTAMode(String &rsApSsid, String &rsApPass, String &rsApHostname, String &rsSsid, String &rsUser, String &rsPass)
{
	mStationWifiCreds.push_back({rsSsid, rsPass, ""});
	muCurrentStationWifiCreds = 0;
	msHostname = rsApHostname;
	msApSsid = rsApSsid;
	msApPass = rsApPass;
	Start(WIFI_MODE_APSTA);
}

void Wifi::Init()
{

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&wifiEventHandler,
														this,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&wifiEventHandler,
														this,
														&instance_got_ip));
}

void Wifi::Start(wifi_mode_t mode)
{
	Init();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(mode));

	if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
	{
		ESP_LOGI(tag, "Starting Access Point with SSID: %s", msApSsid.c_str());
		mApNetif = esp_netif_create_default_wifi_ap();
		ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_hostname(mApNetif, msHostname.c_str()));

		wifi_config_t config;
		memset(&config, 0, sizeof(config));
		memcpy(config.ap.ssid, msApSsid.c_str(), msApSsid.length());
		config.ap.ssid_len = 0;
		memcpy(config.ap.password, msApPass.c_str(), msApPass.length());
		config.ap.channel = mEspNowChannel;
		config.ap.authmode = WIFI_AUTH_OPEN;
		config.ap.ssid_hidden = 0;
		config.ap.max_connection = 8;
		config.ap.beacon_interval = 100;
		ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_AP, &config));
		mEspNowInterface = WIFI_IF_AP;

		// log channel and interface
		ESP_LOGI(tag, "AP Channel: %d", config.ap.channel);
	}

	if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
	{
		StationWifiCreds &creds = mStationWifiCreds[muCurrentStationWifiCreds];
		ESP_LOGI(tag, "Connection Station to SSID: %s", creds.msSsid.c_str());
		ESP_LOGD(tag, "Connect(<%s><%s><%s><%d>)", creds.msSsid.c_str(), creds.msUser.c_str(), creds.msPass.c_str(), msCA.length());
		ESP_LOGD(tag, "-----------------------");
		ESP_LOGD(tag, "%s", msCA.c_str());
		ESP_LOGD(tag, "-----------------------");
		char sHelp[20];
		GetMac((__uint8_t *)sHelp);
		ESP_LOGI(tag, " macaddress: %x:%x:%x:%x:%x:%x", sHelp[0], sHelp[1], sHelp[2], sHelp[3], sHelp[4], sHelp[5]);
		ESP_LOGD(tag, "-----------------------");

		mStaNetif = esp_netif_create_default_wifi_sta();
		ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_hostname(mStaNetif, msHostname.c_str()));

		if (ip.length() > 0 && gw.length() > 0 && netmask.length() > 0)
		{
			esp_netif_dhcpc_stop(mStaNetif);
			esp_netif_ip_info_t ipInfo;
			inet_pton(AF_INET, ip.c_str(), &ipInfo.ip);
			inet_pton(AF_INET, gw.c_str(), &ipInfo.gw);
			inet_pton(AF_INET, netmask.c_str(), &ipInfo.netmask);
			esp_netif_set_ip_info(mStaNetif, &ipInfo);
		}

		wifi_config_t config;
		memset(&config, 0, sizeof(config));
		if (mode == WIFI_MODE_STA)
		{
			mEspNowInterface = WIFI_IF_STA;
		}
		config.sta.channel = mEspNowChannel;

		memcpy(config.sta.ssid, creds.msSsid.c_str(), creds.msSsid.length());
		if (!creds.msUser.length())
			memcpy(config.sta.password, creds.msPass.c_str(), creds.msPass.length());
		ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &config));

		// log channel and interface based on config
		ESP_LOGI(tag, "STA Channel: %d", config.sta.channel);

		if (creds.msUser.length())
		{
			ESP_LOGW(tag, "Enterprise WPA2/3 not implemented yet!");
		}
	}

	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
	// ESP_LOGW(tag, "Configuring WIFI Channel to: %d", mEspNowChannel);
	// ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_channel(mEspNowChannel, WIFI_SECOND_CHAN_NONE));

	// wifi_scan_config_t scan_config = {
	// 	.ssid = NULL,
	// 	.bssid = NULL,
	// 	.channel = 0,
	// 	.show_hidden = false,
	// };
	ESP_LOGI(tag, "Starting Wifi Scan");
	esp_wifi_disconnect();
	esp_wifi_scan_start(nullptr, false);

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
	ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
}

void Wifi::addDNSServer(String &ip)
{
	ip_addr_t dnsserver;
	ESP_LOGD(tag, "Setting DNS[%d] to %s", dnsCount, ip.c_str());
	inet_pton(AF_INET, ip.c_str(), &dnsserver);
	::dns_setserver(dnsCount, &dnsserver);
	dnsCount++;
}

struct in_addr Wifi::getHostByName(String &hostName)
{
	struct in_addr retAddr;
	struct hostent *he = gethostbyname(hostName.c_str());
	if (he == nullptr)
	{
		retAddr.s_addr = 0;
		ESP_LOGD(tag, "Unable to resolve %s - %d", hostName.c_str(), h_errno);
	}
	else
	{
		retAddr = *(struct in_addr *)(he->h_addr_list[0]);
		// ESP_LOGD(tag, "resolved %s to %.8x", hostName, *(uint32_t *)&retAddr);
	}
	return retAddr;
}

void Wifi::setIPInfo(String &ip, String &gw, String &netmask)
{
	this->ip = ip;
	this->gw = gw;
	this->netmask = netmask;
}

void Wifi::OnEvent(esp_event_base_t base, int32_t id, void *event_data)
{
	// esp_err_t rc = ESP_OK;
	if (base == WIFI_EVENT)
	{
		switch (id)
		{
		case WIFI_EVENT_ITWT_SETUP: // Target Wake Time (TWT) is a fascinating feature in Wi-Fi 6 that enhances power efficiency and performance.
									// Let’s break it down: TWT allows an Access Point (AP) and stations (devices) to coordinate their “wake-up” times.
		{
			ESP_LOGW(tag, "--- WIFI_EVENT_ITWT_SETUP");
			break;
		}
		case WIFI_EVENT_ITWT_TEARDOWN:
		{
			ESP_LOGW(tag, "--- WIFI_EVENT_ITWT_TEARDOWN");
			break;
		}
		case WIFI_EVENT_ITWT_PROBE:
		{
			ESP_LOGW(tag, "--- WIFI_EVENT_ITWT_PROBE");
			break;
		}
		case WIFI_EVENT_ITWT_SUSPEND:
		{
			ESP_LOGW(tag, "--- WIFI_EVENT_ITWT_SUSPEND");
			break;
		}
		case WIFI_EVENT_SCAN_DONE:
		{
			uint16_t apCount = 0;
			esp_wifi_scan_get_ap_num(&apCount);
			ESP_LOGI(tag, "Wifi Networks found: %d", apCount);
			while (true) {
				wifi_ap_record_t ap_info;
				esp_err_t ret = esp_wifi_scan_get_ap_record(&ap_info);
				if (ret != ESP_OK) break;
				ESP_LOGI(tag, "Wifi network found: SSID:%s Channel:%d RSSI:%d", ap_info.ssid, ap_info.primary, ap_info.rssi);

				// loop through mStationWifiCreds and check if we have a match
				for (int i = 0; i < mStationWifiCreds.size(); i++) {
					StationWifiCreds &creds = mStationWifiCreds[i];
					if (creds.msSsid.equals((char*)ap_info.ssid)) {
						ESP_LOGI(tag, "Found matching SSID: %s", creds.msSsid.c_str());
						muCurrentStationWifiCreds = i;
						// set the channel
						// ESP_ERROR_CHECK(esp_wifi_set_channel(ap_info->primary, WIFI_SECOND_CHAN_NONE));
						break;
					}
				}
			}
			esp_wifi_clear_ap_list();

			wifi_config_t config = {};
			//config.sta.channel = mEspNowChannel;
			StationWifiCreds &creds = mStationWifiCreds[muCurrentStationWifiCreds];
			if (creds.msSsid.length() > sizeof(config.sta.ssid)-1) {
				ESP_LOGE(tag, "SSID too long: %s", creds.msSsid.c_str());
				break;
			}
			if (creds.msPass.length() > sizeof(config.sta.password)-1) {
				ESP_LOGE(tag, "Password too long: %s", creds.msSsid.c_str());
				break;
			}
			memcpy(config.sta.ssid, creds.msSsid.c_str(), creds.msSsid.length());
			if (!creds.msUser.length())
				memcpy(config.sta.password, creds.msPass.c_str(), creds.msPass.length());

			// log config ssid, password, channel at once
			ESP_LOGI(tag, "Configuring Station for SSID: %s, Channel: %d", config.sta.ssid, config.sta.channel);

			ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &config));


			esp_wifi_connect();

			break;
		}
		case WIFI_EVENT_AP_START:
			ESP_LOGI(tag, "Access Point started!");
			mbConnected = true;
			break;

		case WIFI_EVENT_AP_STOP:
			ESP_LOGD(tag, "--- WIFI_EVENT_AP_STOP");
			mbConnected = false;
			break;
		case WIFI_EVENT_AP_STACONNECTED:
			muConnectedClients++;
			ESP_LOGD(tag, "--- WIFI_EVENT_AP_STACONNECTED - %d clients", muConnectedClients);
			break;
		case WIFI_EVENT_AP_STADISCONNECTED:
			if (muConnectedClients)
				muConnectedClients--;
			ESP_LOGD(tag, "--- WIFI_EVENT_AP_STADISCONNECTED - %d clients", muConnectedClients);
			break;
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGD(tag, "--- WIFI_EVENT_STA_CONNECTED");
			//TODO save "last good known Station credentials"
			break;
		case WIFI_EVENT_STA_DISCONNECTED:
		{
			ESP_LOGD(tag, "--- WIFI_EVENT_STA_DISCONNECTED");
			wifi_event_sta_disconnected_t eventDisconnected = *(wifi_event_sta_disconnected_t *)(event_data);
			ESP_LOGE(tag, "add reason output of wifi disconnect! #%d", eventDisconnected.reason);

			/*
			typedef enum {
				WIFI_REASON_UNSPECIFIED              = 1,
				WIFI_REASON_AUTH_EXPIRE              = 2,
				WIFI_REASON_AUTH_LEAVE               = 3,
				WIFI_REASON_ASSOC_EXPIRE             = 4,
				WIFI_REASON_ASSOC_TOOMANY            = 5,
				WIFI_REASON_NOT_AUTHED               = 6,
				WIFI_REASON_NOT_ASSOCED              = 7,
				WIFI_REASON_ASSOC_LEAVE              = 8,
				WIFI_REASON_ASSOC_NOT_AUTHED         = 9,
				WIFI_REASON_DISASSOC_PWRCAP_BAD      = 10,
				WIFI_REASON_DISASSOC_SUPCHAN_BAD     = 11,
				WIFI_REASON_BSS_TRANSITION_DISASSOC  = 12,
				WIFI_REASON_IE_INVALID               = 13,
				WIFI_REASON_MIC_FAILURE              = 14,
				WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT   = 15,
				WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,
				WIFI_REASON_IE_IN_4WAY_DIFFERS       = 17,
				WIFI_REASON_GROUP_CIPHER_INVALID     = 18,
				WIFI_REASON_PAIRWISE_CIPHER_INVALID  = 19,
				WIFI_REASON_AKMP_INVALID             = 20,
				WIFI_REASON_UNSUPP_RSN_IE_VERSION    = 21,
				WIFI_REASON_INVALID_RSN_IE_CAP       = 22,
				WIFI_REASON_802_1X_AUTH_FAILED       = 23,
				WIFI_REASON_CIPHER_SUITE_REJECTED    = 24,

				WIFI_REASON_INVALID_PMKID            = 53,

				WIFI_REASON_BEACON_TIMEOUT           = 200,
				WIFI_REASON_NO_AP_FOUND              = 201,
				WIFI_REASON_AUTH_FAIL                = 202,
				WIFI_REASON_ASSOC_FAIL               = 203,
				WIFI_REASON_HANDSHAKE_TIMEOUT        = 204,
				WIFI_REASON_CONNECTION_FAIL          = 205,
				WIFI_REASON_AP_TSF_RESET             = 206,
				WIFI_REASON_ROAMING                  = 207,
			} wifi_err_reason_t;
			*/

			mbConnected = false;
			esp_wifi_connect();
			break;
		}
		case WIFI_EVENT_STA_START:
		{
			ESP_LOGI(tag, "Starting Wifi Station");
			// ESP_LOGD(tag, "SETTING HOSTNAME: %s", msHostname.c_str() == NULL ? "NULL" : msHostname.c_str());
			// ESP_ERROR_CHECK(esp_netif_set_hostname(TCPIP_ADAPTER_IF_STA, msHostname.c_str()));
			const char *csActualHostname = nullptr;
			ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_get_hostname(mStaNetif, &csActualHostname));
			ESP_LOGI(tag, "hostname: \"%s\"", csActualHostname == nullptr ? "<error, no hostname set!>" : csActualHostname);
			esp_err_t errConnect = esp_wifi_connect();
			switch (errConnect)
			{
			case 0:
				break;
			case ESP_ERR_WIFI_SSID:
			{
				StationWifiCreds &creds = mStationWifiCreds[muCurrentStationWifiCreds];
				ESP_LOGW(tag, "Invalid SSID: %s", creds.msSsid.c_str() == NULL ? "NULL" : creds.msSsid.c_str());
				break;
			}
			default:
				ESP_LOGE(tag, "Error connecting Wifi: %s", esp_err_to_name(errConnect));
				break;
			}
			break;
		}
		case WIFI_EVENT_STA_STOP:
			ESP_LOGD(tag, "--- WIFI_EVENT_STA_STOP");
			break;
		default:
			break;
		}
	}
	else if (base == IP_EVENT)
	{
		switch (id)
		{
		case IP_EVENT_STA_GOT_IP:
		{
			ESP_LOGD(tag, "--- IP_EVENT_STA_GOT_IP");
			mbConnected = true;
			// if (mpStateDisplay)
			//	mpStateDisplay->SetConnected(true, this);

			// EXAMPLE CODE
			ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
			ESP_LOGI(tag, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

			esp_netif_ip_info_t ip;
			esp_netif_get_ip_info(mStaNetif, &ip);
			break;
		}
			/*if (mpConfig)
				mpConfig->muLastSTAIpAddress = ip.ip.addr; */

			/*if (!mpPing) {
				mpPing = new PingWatchdog();
				mpPing->Start(GetGWAddress());
			} */

		default:
			break;
		}
	}
}

void Wifi::StartTimeSync(String &rsNtpServer)
{
	esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
	esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	esp_sntp_setservername(0, rsNtpServer.c_str());
	esp_sntp_init();
}

void Wifi::EnableEspNow(uint8_t channel)
{
	mEspNowChannel = channel;
}
