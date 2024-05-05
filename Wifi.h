#pragma once
#include "EspString.h"
#include "esp_wifi.h"
#include <vector>
// #include "PingWatchdog.h"


class Wifi
{

public:
	Wifi();

	static Wifi wifi;

	/// @brief must be called before the Start* methods
	/// @param channel 
	void EnableEspNow(uint8_t channel);


	void StartAPMode(String &rsSsid, String &rsPass, String &rsHostname);
	void AddSTACredentials(String &rsSsid, String &rsPass, String &rsUser);
	void StartSTAMode(String &rsHostname);
	void StartSTAMode(String &rsSsid, String &rsPass, String &rsHostname);
	void StartSTAModeEnterprise(String &rsSsid, String &rsUser, String &rsPass, String &rsCA, String &rsHostname);
	void StartAPSTAMode(String &rsApSsid, String &rsApPass, String &rsApHostname);
	void StartAPSTAMode(String &rsApSsid, String &rsApPass, String &rsApHostname, String &rsSsid, String &rsUser, String &rsPass);
	void StartTimeSync(String &rsNtpServer);

	void addDNSServer(String &ip);
	void setIPInfo(String &ip, String &gw, String &netmask);

	String GetLocalAddress();
	void GetLocalAddress(char *sBuf);
	void GetGWAddress(char *sBuf);
	esp_ip4_addr_t GetGWAddress();
	void GetNetmask(char *sBuf);
	void GetMac(__uint8_t uMac[6]);
	void GetApInfo(int8_t &riRssi, uint8_t &ruChannel);

	wifi_interface_t GetEspNowInterface() { return mEspNowInterface; };
	uint8_t GetEspNowChannel() { return mEspNowChannel; };


	bool IsConnected() { return mbConnected; };
	struct in_addr getHostByName(String &hostName);

private:
	void Init();
	void Start(wifi_mode_t mode);
	void OnEvent(esp_event_base_t base, int32_t id, void *event_data);

	friend void wifiEventHandler(void *ctx, esp_event_base_t base, int32_t id, void *event_data);


private:
	// StateDisplay* mpStateDisplay;

	String ip;
	String gw;
	String netmask;

	__uint8_t muMode;
	String msApSsid;
	String msApPass;
	String msCA;
	String msHostname;

	typedef struct {
		String msSsid;
		String msPass;
		String msUser;
	} StationWifiCreds;
	std::vector<StationWifiCreds> mStationWifiCreds;
	uint8_t muCurrentStationWifiCreds = 0;

	__uint8_t muConnectedClients;
	bool mbConnected;

	int dnsCount = 0;
	char *dnsServer = nullptr;

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	esp_netif_t *mStaNetif = nullptr;
	esp_netif_t *mApNetif = nullptr;

	// esp now
	uint8_t mEspNowChannel = 0;
	wifi_interface_t mEspNowInterface = WIFI_IF_AP;
};

