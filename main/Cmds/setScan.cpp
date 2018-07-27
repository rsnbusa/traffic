/*
 * setScan.cpp
 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
#include "setScan.h"
extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);

string scanNetwork()
{
	char textl[100];
	string algo;
	if(aqui.traceflag & (1<<CMDD))
		PRINT_MSG("[CMDD]Scanning...");
//	wifi_scan_config_t scan_config;

	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
	        .show_hidden = true
	};
	scan_config.show_hidden=true;
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	uint16_t ap_num =5;
	wifi_ap_record_t ap_records[5];
	esp_wifi_scan_get_ap_records(&ap_num, ap_records);
	algo="";
	for(int i = 0; i < ap_num; i++)
	{
		sprintf(textl,"%s|%d|%d|%d",
				(char *)ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi,
				ap_records[i].authmode);
		algo+=string(textl);
		if(i<ap_num-1)
			algo+="!";
	}
	return algo;

}

void set_scanCmd(void * pArg){
	arg *argument=(arg*)pArg;
	string webString;

	if(!set_commonCmd(argument,false))
		return;

	webString=scanNetwork();
	sendResponse( argument->pComm,argument->typeMsg, webString,webString.length(),MINFO,false,false);            // send to someones browser when asked
	if(aqui.traceflag & (1<<CMDD))
		PRINT_MSG("[CMDD]Web Scan\n");
	webString="";
//	free(pArg);
//	vTaskDelete(NULL);
}

u8 DayOfWeek(int y, u8 m, u8 d) {   // y > 1752, 1 <= m <= 12
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	y -= m < 3;
	return ((y + y/4 - y/100 + y/400 + t[m-1] + d) % 7) + 1; // 01 - 07, 01 = Sunday
}




