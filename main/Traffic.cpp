#include <stdio.h>
#include <stdint.h>
#include "defines.h"
#include "forward.h"
#include "includes.h"
#include "projTypes.h"
#include "string.h"
#include "globals.h"
#include "cmds.h"
#include "freertos/timers.h"
#include "driver/adc.h"
#include <port/arpa/inet.h>

const char *TAG = "TFF";
extern void postLog(int code,int code1);

void processCmds(void * nc,cJSON * comands);
void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
void runLight(void * pArg);
void rxMultiCast(void * pArg);
void blinkLight(void *pArg);

using namespace std;

void blink(int cual)
{
	gpio_set_level((gpio_num_t)cual, 1);
	vTaskDelay(interval / portTICK_PERIOD_MS);
	gpio_set_level((gpio_num_t)cual, 0);
}

uint32_t readADC()
{
	u32 adc_reading=0;

    for (int i = 0; i < SAMPLES; i++)
    	adc_reading += adc1_get_raw((adc1_channel_t)adcchannel);

        adc_reading /= SAMPLES;
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    //    printf("Reading %d volt %d\n",adc_reading,voltage);
	return voltage;
}

void eraseMainScreen()
{
	display.setColor((OLEDDISPLAY_COLOR)0);
	display.fillRect(0,19,127,29);
	display.setColor((OLEDDISPLAY_COLOR)1);
	display.display();
}

uint32_t IRAM_ATTR millis()
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(uint16_t a)
{
	vTaskDelay(a /  portTICK_RATE_MS);
}

int getGPIO(int cual)
{
	int como=0;

	for (int a=0;a<4;a++)
	{
		como +=gpio_get_level((gpio_num_t)cual)?1:-1;
		delay(6);
	}
	if (como<0)
		como=0;
	if(como>1)
		como=1;
	return como;
}

void datacallback(esp_mqtt_client_handle_t self, esp_mqtt_event_handle_t event)
{
	mqttMsg mensa;
	mensa.mensaje=(char*)event->data;
	mensa.mensaje[event->data_len]=0;
	mensa.nc=self;
	mensa.sizer=event->data_len;
	xQueueSend( mqttQ, ( void * ) &mensa,( TickType_t ) 0 );

}

void mqttmanager(void * parg)
{
	mqttMsg mensa;

	mqttQ = xQueueCreate( 20, sizeof( mensa ) );
	while(1)
	{
		if( xQueueReceive( mqttQ, &mensa, portMAX_DELAY ))
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<MQTTD))
				printf("[MQTTD]Msg:%s\n",mensa.mensaje);
#endif
			root=cJSON_Parse( mensa.mensaje);
			if(root==NULL)
				printf("Not valid Json\n");
			else
			{
				processCmds(mensa.nc,root);
				cJSON_Delete(root);
			}
		}
		delay(100);//just in case it fails to wait forever do no eat the cpu
	}
}

void write_to_flash_lights() //save our configuration
{
	esp_err_t q ;
	q=nvs_set_blob(lighthandle,"lights",(void*)&sysLights,sizeof(sysLights));

	if (q !=ESP_OK)
	{
		printf("Error write Light %d\n",q);
		return;
	}
	 q = nvs_commit(lighthandle);
	 if (q !=ESP_OK)
	 	printf("Commit Error Light %d\n",q);
}

void write_to_flash_seq() //save our configuration
{
	esp_err_t q ;
	q=nvs_set_blob(seqhandle,"seq",(void*)&sysSequence,sizeof(sysSequence));

	if (q !=ESP_OK)
	{
		printf("Error write Seq %d\n",q);
		return;
	}
	 q = nvs_commit(seqhandle);
	 if (q !=ESP_OK)
	 	printf("Commit Error Seq %d\n",q);
}

void write_to_flash_cycles() //save our configuration
{
	esp_err_t q ;
	q=nvs_set_blob(seqhandle,"cycles",(void*)&allCycles,sizeof(allCycles));

	if (q !=ESP_OK)
	{
		printf("Error write Cycles %d\n",q);
		return;
	}
	 q = nvs_commit(seqhandle);
	 if (q !=ESP_OK)
	 	printf("Commit Error Cycles %d\n",q);
}

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q=nvs_set_blob(nvshandle,"config",(void*)&sysConfig,sizeof(sysConfig));

	if (q !=ESP_OK)
	{
		printf("Error write %d\n",q);
		return;
	}
	 q = nvs_commit(nvshandle);
	 if (q !=ESP_OK)
	 	printf("Commit Error %d\n",q);
}

void get_traffic_name()
{
	char local[20];
	string appn;
	u8  mac[6];

	if (sysConfig.ssid[0]==0)
		esp_wifi_get_mac(WIFI_IF_AP, mac);
	else
		esp_wifi_get_mac(WIFI_IF_STA, mac);

	sprintf(local,"%02x%02x",mac[4],mac[5]);//last tow bytes of MAC to identify the connection
	string macID=string(local);
	for(int i = 0; i < macID.length(); i++)
		macID[i] = toupper(macID[i]);

	appn=string(sysConfig.lightName);//meter name will be used as SSID if available else [APP]IoT-XXXX

	if (appn.length()<2)
	{
		AP_NameString = string(APP)+"-" + macID;
		appn=string(AP_NameString);
	}
	else
	{
		//	AP_NameString = appn +"-"+ macID;
		AP_NameString = appn ;
	}
	macID="";
	appn="";

}

// Convert a Mongoose string type to a string.
char *mgStrToStr(struct mg_str mgStr) {
	char *retStr = (char *) malloc(mgStr.len + 1);
	memcpy(retStr, mgStr.p, mgStr.len);
	retStr[mgStr.len] = 0;
	return retStr;
} // mgStrToStr

string getParameter(arg* argument, string cual)
{
	char paramr[300];
	if (argument->typeMsg ==1) //Json get parameter cual
	{
		cJSON *param= cJSON_GetObjectItem((cJSON*)argument->pMessage,cual.c_str());
		if(param)
			return string(param->valuestring);
		else
			return string("");
	}
	else //standard web server parameter
	{
		struct http_message * param=(struct http_message *)argument->pMessage;
		int a= mg_get_http_var(&param->query_string, cual.c_str(), paramr,sizeof(paramr));
		if(a>=0)
			paramr[a]=0;
		return string(paramr);
	}
	return "";
}

int findCommand(string cual)
{
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Find %s of %d cmds\n",cual.c_str(),MAXCMDS);
#endif

	for (int a=0;a<MAXCMDS;a++)
		if(cual==string(cmds[a].comando))
			return a;
	return -1;
}


void webCmds(void * nc,struct http_message * params)
{
	char *uri=mgStrToStr(params->uri);
	int cualf=findCommand(uri);
	if(cualf>=0)
	{
		arg *argument=(arg*)malloc(sizeof(arg));
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<CMDD))
			printf("[CMDD]Webcmdrsn %d %s\n",cualf,uri);
#endif
		argument->pMessage=(void*)params;
		argument->typeMsg=0;
		argument->pComm=nc;
		(*cmds[cualf].code)(argument);
		free(argument);
		//xTaskCreate(cmds[cualf].code,"cmds",10000,(void*)argument, (configMAX_PRIORITIES - 1),NULL );
	}
	free(uri);
}

void processCmds(void * nc,cJSON * comands)
{
	cJSON *monton= cJSON_GetObjectItem(comands,"batch");
	if(monton!=NULL)
	{
		int son=cJSON_GetArraySize(monton);
		for (int a=0;a<son;a++)
		{
			arg *argument=(arg*)malloc(sizeof(arg));
			cJSON *cmdIteml = cJSON_GetArrayItem(monton, a);
			cJSON *cmd= cJSON_GetObjectItem(cmdIteml,"cmd");
			if(cmd!=NULL)
			{
				int cualf=findCommand(string(cmd->valuestring));
				if(cualf>=0)
				{
					argument->pMessage=(void*)cmdIteml;
					argument->typeMsg=1;
					argument->pComm=nc;
					(*cmds[cualf].code)(argument);
				//	xTaskCreate(cmds[cualf].code,"cmds",10000,(void*)argument, MGOS_TASK_PRIORITY,NULL );
					free(argument);
				}
#ifdef DEBUGSYS
				else
					if(sysConfig.traceflag & (1<<CMDD))
						printf("[CMDD]Cmd Not found\n");
#endif
			}
		}
	}
}

void sendResponse(void * comm,int msgTipo,string que,int len,int code,bool withHeaders, bool retain)
{
	struct mg_connection *nc=(struct mg_connection*)comm;
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<PUBSUBD))
		printf("[PUBSUBD]Type %d Sending response %s len=%d\n",msgTipo,que.c_str(),que.length());
#endif
	if(msgTipo==1)
	{ // MQTT Response
		esp_mqtt_client_handle_t mcomm=( esp_mqtt_client_handle_t)comm;
		string final;
//		sprintf(textl,"%02d!",code);
		final=que;

		if (!mqttf)
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<MQTTD))
				printf("[MQTTD]No mqtt\n");
#endif
			return;
		}
		if(withHeaders)
		{
			for (int a=0;a<sonUid;a++)
			{
				spublishTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/"+montonUid[a]+"/MSG";
#ifdef DEBUGSYS
				if(sysConfig.traceflag & (1<<PUBSUBD))
					printf("[PUBSUBD]Publish %s Msg %s\n",spublishTopic.c_str(),final.c_str());
#endif
				esp_mqtt_client_publish(mcomm, (char*)spublishTopic.c_str(), (char*)final.c_str(),final.length(), 0, 0);
				spublishTopic="";
				delay(200);
			}
		}
		else
		{
				spublishTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/"+uidStr+"/MSG";
#ifdef DEBUGSYS
				if(sysConfig.traceflag & (1<<PUBSUBD))
					printf("[PUBSUBD]DirectPublish %s Msg %s\n",spublishTopic.c_str(),final.c_str());
#endif
				esp_mqtt_client_publish(mcomm, (char*)spublishTopic.c_str(), (char*)final.c_str(),final.length(), 0, 0);

		}
	}
	else
	{ //Web Response
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WEBD))
			printf("[WEBD]Web response\n");
#endif
		if(len==0)
		{
			que=" ";
			len=1;
		}
		mg_send_head(nc, 200, len, withHeaders?"Content-Type: text/html":"Content-Type: text/plain");
		mg_printf(nc, "%s", que.c_str());
		nc->flags |= MG_F_SEND_AND_CLOSE;
	}
}

void initSensors()
{
	gpio_config_t io_conf;
	uint64_t mask=1;  //If we are going to use Pins >=32 needs to shift left more than 32 bits which the compilers associates with a const 1<<x. Stupid

	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = (mask<<WIFILED|mask<<MQTTLED|mask<<SENDLED|mask<<RXLED);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en =GPIO_PULLUP_DISABLE;
	gpio_config(&io_conf);


	adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
	adc1_config_width(ADC_WIDTH_BIT_12);
	adcchannel=(adc1_channel_t)ADCCHAN;
	adc1_config_channel_atten(adcchannel, ADC_ATTEN_DB_11);


	if (sysLights.numLuces>0)
	{
		io_conf.pin_bit_mask =0;
		for (int a=0;a<sysLights.numLuces;a++)
				io_conf.pin_bit_mask = io_conf.pin_bit_mask |(mask<<sysLights.thePorts[a]);

		io_conf.intr_type = GPIO_INTR_DISABLE;
		io_conf.mode = GPIO_MODE_OUTPUT;
		io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
		io_conf.pull_up_en =GPIO_PULLUP_DISABLE;
		gpio_config(&io_conf);

		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
	}


}

void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData) {
	switch (ev) {
	case MG_EV_HTTP_REQUEST:
	{
		struct http_message *message = (struct http_message *) evData;
		webCmds((void*)nc,message);
		break;
	}

	default:
		break;
	}
} // End of mongoose_event_handler

void mongooseTask(void *data) {
	mongf=true;
	mg_mgr_init(&mgr, NULL);
	struct mg_connection *c = mg_bind(&mgr, ":80", mongoose_event_handler);
	if (c == NULL) {
		printf( "No connection from the mg_bind()\n");
			vTaskDelete(NULL);
	}
	mg_set_protocol_http_websocket(c);
#ifdef DEBUGSYS
	if(sysConfig.traceflag==(1<<BOOTD))
			printf("Started mongoose\n");
#endif
	while (FOREVER)
		mg_mgr_poll(&mgr, 10);

} // mongooseTask
/*
void mdnstask(void *args){
	char textl[60];
	time_t now;
	struct tm timeinfo;
	esp_err_t ret;
	time(&now);
	localtime_r(&now, &timeinfo);

		esp_err_t err = mdns_init();
		if (err) {
			printf( "Failed starting MDNS: %u\n", err);
		}
		else
		{
			if(sysConfig.traceflag&(1<<CMDD))
				printf("[CMDD]MDNS hostname %s\n",AP_NameString.c_str());
			mdns_hostname_set(AP_NameString.c_str()) ;
			mdns_instance_name_set(AP_NameString.c_str()) ;

			  mdns_txt_item_t serviceTxtData[4] = {
			        {(char*)"WiFi",(char*)"Yes"},
			        {(char*)"ApMode",(char*)"Yes"},
			        {(char*)"OTA",(char*)"Yes"},
			        {(char*)"Boot",(char*)""}
			    };

			sprintf(textl,"%d/%d/%d %d:%d:%d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);

			if(sysConfig.traceflag&(1<<CMDD))
				printf("[CMDD]MDNS time %s\n",textl);
			string s=string(textl);
			ret=mdns_service_add( AP_NameString.c_str(),"_doorIoT", "_tcp", 80,serviceTxtData, 4 );
			if(ret && (sysConfig.traceflag&(1<<CMDD)))
							printf("Failed add service  %d\n",ret);
			ret=mdns_service_txt_item_set("_doorIoT", "_tcp", "Boot", s.c_str());
			if(ret && (sysConfig.traceflag&(1<<CMDD)))
							printf("Failed add txt %d\n",ret);
			s="";
		}
		mdnsHandle=NULL;

	vTaskDelete(NULL);
}
*/
void initialize_sntp(void *args)
{
	 struct timeval tvStart;
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, (char*)"pool.ntp.org");
	sntp_init();
	time_t now = 0;


	int retry = 0;
	const int retry_count = 10;
//	setenv("TZ", "EST5", 1); //UTC is 5 hours ahead for Quito
//	tzset();

	struct tm timeinfo;// = { 0 };
		timeinfo.tm_hour=timeinfo.tm_min=timeinfo.tm_sec=0;
		timeinfo.tm_mday=1;
		timeinfo.tm_mon=0;
		timeinfo.tm_year=100;
		// set to 1/1/2000 0:0:0
		// timer *1000 im ms

	while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
	}
	if(retry_count==0)  //Failed.Use RTC date
		vTaskDelete(NULL);

	now=now-UIO*3600;
	   timeval tm;
	   tm.tv_sec=now;
	   tm.tv_usec=0;
	   settimeofday(&tm,NULL);
	localtime_r(&now, &timeinfo);
	gettimeofday(&tvStart, NULL);
	sntpf=true;
#ifdef DEBUGSYS
	if(sysConfig.traceflag&(1<<BOOTD))
		printf("[BOOTD]Internet Time %04d/%02d/%02d %02d:%02d:%02d YDays %d DoW:%d\n",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,
			timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_yday,timeinfo.tm_wday);
#endif
	TODAY=1<<timeinfo.tm_wday;
#ifdef DEBUGSYS
	if(sysConfig.traceflag&(1<<BOOTD))
		printf("[BOOTD]Today %x\n",TODAY);
#endif

	sysConfig.preLastTime=sysConfig.lastTime;
	time(&sysConfig.lastTime);
	write_to_flash();
	timef=1;
	postLog(0,sysConfig.bootcount);
	rtc.setEpoch(now);
//	if(!mdnsf)
	//	xTaskCreate(&mdnstask, "mdns", 4096, NULL, 5, &mdnsHandle); //Ota Interface Controller
	//release this task
	vTaskDelete(NULL);
}

void ConfigSystem(void *pArg)
{
	uint32_t del=(uint32_t)pArg;
	while(FOREVER)
	{
		gpio_set_level((gpio_num_t)WIFILED, 1);
		delay(del);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		delay(del);
	}
}

void newSSID(void *pArg)
{
	string temp;
	wifi_config_t sta_config;
	int len,cual=(int)pArg;

	len=0;
	esp_wifi_stop();

	temp=string(sysConfig.ssid[cual]);
	len=temp.length();

	if(displayf)
	{
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(64,42,"               ",10,TEXT_ALIGN_CENTER,DISPLAYIT,REPLACE);
			drawString(64,42,string(sysConfig.ssid[curSSID]),10,TEXT_ALIGN_CENTER,DISPLAYIT,REPLACE);
			xSemaphoreGive(I2CSem);
		}
	}
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Try SSID =%s= %d %d\n",temp.c_str(),cual,len);
#endif
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	temp=string(sysConfig.ssid[cual]);
	len=temp.length();
	memcpy((void*)sta_config.sta.ssid,temp.c_str(),len);
	sta_config.sta.ssid[len]=0;
	temp=string(sysConfig.pass[cual]);
	len=temp.length();
	memcpy((void*)sta_config.sta.password,temp.c_str(),len);
	sta_config.sta.bssid_set=0;
	sta_config.sta.password[len]=0;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	esp_wifi_start(); //if error try again indefinitly
	vTaskDelete(NULL);
}

void newSSIDfirm(string ap, string pass)
{
	wifi_config_t sta_config;
	int len;

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	cfg.event_handler = &esp_event_send;
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//	printf("Firmware connect to %s pass %s\n",ap.c_str(),pass.c_str());
	len=0;
	esp_wifi_stop();
	printf("Stop firm\n");
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//	printf("Fimr mode\n");
	len=ap.length();
	memcpy((void*)sta_config.sta.ssid,ap.c_str(),len);
	sta_config.sta.ssid[len]=0;
	len=pass.length();
	memcpy((void*)sta_config.sta.password,pass.c_str(),len);
	sta_config.sta.bssid_set=0;
	sta_config.sta.password[len]=0;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
//	printf("Config firm\n");
	esp_wifi_start(); //if error try again indefinitly
//	printf("Start firm\n");
}

esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
	system_event_ap_staconnected_t *conap;
	//system_event_sta_got_ip_t *ipgave;
	system_event_sta_disconnected_t *disco;
	conap=(system_event_ap_staconnected_t*)&event->event_info;
	//ipgave=(system_event_sta_got_ip_t*)&event->event_info;
	disco=(system_event_sta_disconnected_t*)&event->event_info;
	esp_err_t err;
	string local="Closed",temp;
//	system_event_ap_staconnected_t *staconnected;
	wifi_sta_list_t station_list;
	wifi_sta_info_t *stations ;
	ip4_addr_t addr;


#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<WIFID))
		printf("[WIFID]Wifi Handler %d Reason %d\n",event->event_id,disco->reason);
#endif
	switch(event->event_id)
	{
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      	ESP_LOGI(TAG,"Station disconnected %02x:%02x:%02x:%02x:%02x:%02x",conap->mac[0],conap->mac[1],
      			conap->mac[2],conap->mac[3],conap->mac[4],conap->mac[5]);
    	dhcp_search_ip_on_mac(conap->mac , &addr);//this IP died. Do something
    	esp_wifi_ap_get_sta_list(&station_list);
    	totalConnected=station_list.num;
      	//must send a warning message. TL is now down for a street.
      	break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
    	esp_wifi_ap_get_sta_list(&station_list);
    	stations=(wifi_sta_info_t*)station_list.sta;
    	dhcp_search_ip_on_mac(stations[station_list.num-1].mac , &addr);
    	//printf("AP Assigned %d.%d.%d.%d \n",IP2STR(&addr));
    	connectedToAp[station_list.num-1]=addr.addr;
    	totalConnected=station_list.num;
    	break;

	case SYSTEM_EVENT_STA_GOT_IP:
		gpio_set_level((gpio_num_t)WIFILED, 1);
		connf=true;
		localIp=event->event_info.got_ip.ip_info.ip;
		get_traffic_name();
#ifdef DEBUGSYS
		if(sysConfig.traceflag&(1<<BOOTD))
			printf( "[BOOTD]Got IP: %d.%d.%d.%d \n", IP2STR(&event->event_info.got_ip.ip_info.ip));
#endif
		gpio_set_level((gpio_num_t)WIFILED, 1);

		if(blinkHandle)
	   {
		   vTaskDelete(blinkHandle);
		   blinkHandle=NULL;
		   gpio_set_level((gpio_num_t)sysLights.defaultLight,1);
	   }

		if(sysConfig.mode) //Server Mode Only
		{
			if(!mqttf)
			{
#ifdef DEBUGSYS
				if(sysConfig.traceflag&(1<<BOOTD))
					printf("[BOOTD]Connect to mqtt\n");
#endif
				xTaskCreate(&mqttmanager,"mgr",10240,NULL,  5, &mqttHandle);		// User interface while in development. Erased in RELEASE

				clientCloud = esp_mqtt_client_init(&settings);
				 if(clientCloud)
					esp_mqtt_client_start(clientCloud);
				 else{
					 printf("Fail mqtt initCloud\n");
					 vTaskDelete(mqttHandle);
				 }
			}

			if(!mongf)
			{
#ifdef DEBUGSYS
				if(sysConfig.traceflag&(1<<BOOTD))
					printf("[BOOTD]Start Mongoose\n");
#endif
				setLogo("Traffic");
				xTaskCreate(&mongooseTask, "mongooseTask", 10240, NULL, 5, &mongoHandle); //  web commands Interface controller
				xTaskCreate(&initialize_sntp, "sntp", 2048, NULL, 3, NULL); //will get date
			}
		}

		// Main routine for Commands
		xTaskCreate(&rxMultiCast, "rxMulti", 4096, NULL, 4, &rxHandle);
		break;

	case SYSTEM_EVENT_AP_START:  // Handle the AP start event
	//	tcpip_adapter_ip_info_t ip_info;
	//	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
	//	printf("System not Configured. Use local AP and IP:" IPSTR "\n", IP2STR(&ip_info.ip));
        err=esp_wifi_connect();
        if(err)
        	printf("Error Connect AP %d\n",err);
//        else
//		if(!mongf)
//		{
//			printf("Mongoose Start AP\n");
//			xTaskCreate(&mongooseTask, "mongooseTask", 10240, NULL, 5, NULL);
//			xTaskCreate(&initialize_sntp, "sntp", 2048, NULL, 3, NULL);
//			xTaskCreate(&ConfigSystem, "cfg", 1024, (void*)100, 3, NULL);
//		}
		break;

	case SYSTEM_EVENT_STA_START:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Connect firmware %s\n",firmwf?"Y":"N");
#endif
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_STOP:
		connf=false;
		gpio_set_level((gpio_num_t)RXLED, 0);
		gpio_set_level((gpio_num_t)SENDLED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 0);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
	//	staconnected = &event->event_info.sta_connected;
	//	printf("AP Sta connect MAC %02x:%02x:%02x:%02x:%02x:%02x\n", staconnected->mac[0],staconnected->mac[1],staconnected->mac[2],staconnected->mac[3],
		//		staconnected->mac[4],staconnected->mac[5]);
		break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
	case SYSTEM_EVENT_ETH_DISCONNECTED:
		connf=false;
		gpio_set_level((gpio_num_t)RXLED, 0);
		gpio_set_level((gpio_num_t)SENDLED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 0);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		if(runHandle)
		{
			vTaskDelete(runHandle);
			runHandle=NULL;
		}

		if(rxHandle){
			close(theSock);
			vTaskDelete(rxHandle);
			rxHandle=NULL;
		}

		if(blinkHandle){
			vTaskDelete(blinkHandle);
			blinkHandle=NULL;
		//	printf("Launch Red blink %d\n",sysLights.defaultLight);
			//put the Light in Danger Mode  Blink Red light. lost host and sync
			xTaskCreate(&blinkLight, "blink", 4096, (void*)sysLights.defaultLight,(UBaseType_t) 3, &blinkHandle); //will get date
		}
		else
		{
			//printf("Direct Launch Red blink %d\n",sysLights.defaultLight);
					//put the Light in Danger Mode  Blink Red light. lost host and sync
					xTaskCreate(&blinkLight, "blink", 4096, (void*)sysLights.defaultLight, (UBaseType_t)3, &blinkHandle); //will get date
		}

		if(cycleHandle)
		{
			vTaskDelete(cycleHandle);
			cycleHandle=NULL;
		}
			REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
			gpio_set_level((gpio_num_t)sysLights.defaultLight, 1);

#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Reconnect %d\n",curSSID);
#endif
		curSSID++;
		if(curSSID>4)
			curSSID=0;

		temp=string(sysConfig.ssid[curSSID]);

#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Temp[%d]==%s=\n",curSSID,temp.c_str());
#endif
		if(temp=="")
			curSSID=0;

		xTaskCreate(&newSSID,"newssid",2048,(void*)curSSID, MGOS_TASK_PRIORITY, NULL);

		break;

	case SYSTEM_EVENT_STA_CONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Connected SSID[%d]=%s\n",curSSID,sysConfig.ssid[curSSID]);
#endif
		sysConfig.lastSSID=curSSID;
		write_to_flash();
		break;

	default:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]default WiFi %d\n",event->event_id);
#endif
		break;
	}
	return ESP_OK;
} // wifi_event_handler


void initI2C()
{
	i2cp.sdaport=(gpio_num_t)SDAW;
	i2cp.sclport=(gpio_num_t)SCLW;
	i2cp.i2cport=I2C_NUM_0;
	miI2C.init(i2cp.i2cport,i2cp.sdaport,i2cp.sclport,100000,&I2CSem);//Will reserve a Semaphore for Control
}

esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    esp_err_t err;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            	esp_mqtt_client_subscribe(client, cmdTopic.c_str(), 0);
        		gpio_set_level((gpio_num_t)MQTTLED, 1);
        		mqttCon=client;
#ifdef DEBUGSYS
        		if(sysConfig.traceflag & (1<<MQTTD))
        			printf("[MQTTD]Connected %s(%d)\n",(char*)event->user_context,(u32)client);
#endif
            break;
        case MQTT_EVENT_DISCONNECTED:
#ifdef DEBUGSYS
        		if(sysConfig.traceflag & (1<<MQTTD))
        			printf( "[MQTTD]MQTT_EVENT_DISCONNECTED %s(%d)\n",(char*)event->user_context,(u32)client);
#endif
        		gpio_set_level((gpio_num_t)MQTTLED, 0);
        		mqttf=false;
        		mqttCon=0;
            break;

        case MQTT_EVENT_SUBSCRIBED:
#ifdef DEBUGSYS
            	if(sysConfig.traceflag & (1<<MQTTD))
            		printf("[MQTTD]Subscribed Cloud\n");
#endif
            	mqttf=true;
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
#ifdef DEBUGSYS
        	if(sysConfig.traceflag & (1<<MQTTD))
        		printf( "[MQTTD]MQTT_EVENT_UNSUBSCRIBED %s(%d)\n",(char*)event->user_context,(u32)client);
        		  esp_mqtt_client_subscribe(client, cmdTopic.c_str(), 0);
#endif
            break;
        case MQTT_EVENT_PUBLISHED:
#ifdef DEBUGSYS
        	if(sysConfig.traceflag & (1<<MQTTD))
        		printf( "[MQTTD]MQTT_EVENT_PUBLISHED %s(%d)\n",(char*)event->user_context,(u32)client);
#endif
            break;
        case MQTT_EVENT_DATA:
#ifdef DEBUGSYS
        	if(sysConfig.traceflag & (1<<MQTTD))
        	{
        		printf("[MQTTD]MSG for %s(%d)\n",(char*)event->user_context,(u32)client);
        		printf("[MQTTD]TOPIC=%.*s\r\n", event->topic_len, event->topic);
        		printf("[MQTTD]DATA=%.*s\r\n", event->data_len, event->data);
        	}
#endif
            	datacallback(client,event);
            break;
        case MQTT_EVENT_ERROR:
#ifdef DEBUGSYS
        	if(sysConfig.traceflag & (1<<MQTTD))
        		printf("[MQTTD]MQTT_EVENT_ERROR %s(%d)\n",(char*)event->user_context,(u32)client);
#endif
        	err=esp_mqtt_client_start(client);
            if(err)
                printf("Error Start Disconnect %s. None functional now!\n",(char*)event->user_context);
            break;
    }
    return ESP_OK;
}

u8 lastAddr(string que)
{
	  char * pch;
	  u8 res=0;
	  pch = strtok ((char*)que.c_str(),".");
	  while (pch != NULL){
		  res=atoi(pch);
	    pch = strtok (NULL, ".");
	  }
	  return res;
}

void blinkLight(void *pArg)
{
	int cual=(int)pArg;
	while(true)
	{
		gpio_set_level((gpio_num_t)cual,1);
		delay(interval);
		gpio_set_level((gpio_num_t)cual,0);
		delay(interval);
	}
}

void process_cmd(cmd_struct cual)
{
	time_t now;
	struct tm timeinfo;
    string algo;
    firmware_type elfw;

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Process Cmd %s towho %d Fromwho %d Node %d\n",tcmds[cual.cmd],cual.towho,cual.fromwho,cual.nodeId);
#endif
	if(((cual.towho==255) || (cual.towho==sysConfig.whoami)) && cual.nodeId==sysConfig.nodeid)
	{
		if(sysConfig.mode==0)
		{
			   switch (cual.cmd)
			   { //Valid Incoming Cmds related to this station
			   	   case START:
			   	   case STOP:
			   	   case PING:
			   	   case SENDC:
			   	   case TEST:
			   	   case DELAY:
			   	   case QUIET:
			   	   case INTERVAL:
			   	   case NEWID:
			   	   case RESETC:
			   	   case RESET:
			   	   case KILL:
			   	   case RUN:
			   	   case OFF:
			   	   case ON:
			   	   case RUALIVE:
			   	   case LEDS:
			   	   case FWARE:
						if(sysConfig.showLeds)
							blink(RXLED);
					break;
			   	   default:
			   		   break;
			   }
		}
		else //as server all
			if(sysConfig.showLeds)
				blink(RXLED);

		entran++;
	   switch (cual.cmd)
	   {
		   case START:
			   rxtxf =true;
#ifdef DEBUGSYS
				if(sysConfig.traceflag & (1<<TRAFFICD))
					printf("[TRAFFICD][%d-%d]Start\n",cual.towho,sysConfig.whoami);
#endif
			   xTaskCreate(&mcast_example_task, "mcast_task", 4096, NULL, 5, NULL);
			   break;
		   case STOP:
			   rxtxf =false; //He will kill himself so close and mem be freed
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Stop\n",cual.towho,sysConfig.whoami);
#endif
			   break;
		   case ACK:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]ACK received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
			   break;
		   case NAK:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]NAKC received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
			   break;
		   case DONE:
			   if(sysConfig.mode)
			   {
#ifdef DEBUGSYS
				   if(sysConfig.traceflag & (1<<TRAFFICD))
					   printf("[TRAFFICD][%d-%d]DONE received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
				   xQueueSend( cola, ( void * ) &cual.fromwho,( TickType_t ) 0 );
			   }
			   break;
		   case PING:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Ping received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
				sendMsg(PONG,cual.fromwho,0,0,NULL,0);
			   break;
		   case PONG:
#ifdef DEBUGSYS
			   if(sysConfig.mode)
			   {
				   if(sysConfig.traceflag & (1<<TRAFFICD))
					   printf("[TRAFFICD][%d-%d]Pong from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
	   	   	   }
#endif
			   break;
		   case COUNTERS:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Send Counters received\n",cual.towho,sysConfig.whoami);
#endif
				sendMsg(SENDC,cual.fromwho,salen,entran,NULL,0);
			   break;
		   case SENDC:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Counters in from %d Out %d In %d\n",cual.towho,sysConfig.whoami,cual.fromwho,cual.free1,cual.free2);
#endif
			   break;
		   case TEST:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Incoming from %d SeqNum %d Lapse %d\n",cual.towho,sysConfig.whoami,cual.fromwho,cual.seqnum,cual.lapse);
#endif
			   break;
		   case DELAY:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Delay from %d to %d \n",cual.towho,sysConfig.whoami,cual.fromwho,cual.free1);
#endif
			   howmuch=cual.free1;
			   sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case QUIET:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Quiet from %d to  %d \n",cual.towho,sysConfig.whoami,cual.fromwho,cual.free1);
#endif
			   displayf=cual.free1;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case INTERVAL:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Interval from %d to %d \n",cual.towho,sysConfig.whoami,cual.fromwho,cual.free2);
#endif
			   interval=cual.free1;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case NEWID:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]NewId from %d to %d \n",cual.towho,sysConfig.whoami,sysConfig.whoami,cual.free1);
#endif
			   sysConfig.whoami=cual.free1;
			   write_to_flash();
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case RESETC:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]ResetCounters from %d \n",cual.towho,sysConfig.whoami,sysConfig.whoami);
#endif
			   entran=salen=0;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case RESET:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<CMDD))
				   printf("[CMDD]Reset unit after 5 seconds\n");
#endif
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   vTaskDelay(5000 / portTICK_PERIOD_MS);
			   esp_restart();
			   break;
		   case KILL:
			   if(runHandle!=NULL)
			   {
				   vTaskDelete(runHandle),
				   runHandle=NULL;
			   }
				gpio_set_level((gpio_num_t)sysLights.defaultLight, 1);
			   break;
		   case RUN:
			   if(runHandle!=NULL)
			   {
				   vTaskDelete(runHandle),
				   runHandle=NULL;
			   }
			   memcpy(&now,cual.buff,sizeof(now));

			   timeval tm;
			   tm.tv_sec=now;
			   tm.tv_usec=0;
			   settimeofday(&tm,NULL);
			   time(&now);
			   localtime_r(&now, &timeinfo);
		//	   printf("Received time from Controller %s",asctime(&timeinfo));

#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD]Start Cycle for %d time\n",cual.free1*FACTOR);
#endif
				xTaskCreate(&runLight,"light",4096,(void*)cual.free1, MGOS_TASK_PRIORITY, &runHandle);				//Manages all display to LCD
			   break;
		   case OFF:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD]Turn Off TLight\n");
#endif
			   if(runHandle!=NULL)
			   {
				   vTaskDelete(runHandle),
				   runHandle=NULL;
			   }
				REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
				break;
		   case ON:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD]Turn On TLight\n");
#endif
				gpio_set_level((gpio_num_t)sysLights.defaultLight, 1); //Default light On
				break;
		   case RUALIVE:
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<ALIVED))
				   printf("[ALIVED]Heartbeat Received\n");
#endif
				sendMsg(IMALIVE,EVERYBODY,0,0,NULL,0);
				break;
		   case IMALIVE:
			   if(sysConfig.mode)
			   {
#ifdef DEBUGSYS
				   if(sysConfig.traceflag & (1<<ALIVED))
					   printf("[ALIVED]Alive In from %d\n",cual.fromwho);
#endif
					//   printf( "[Remote IP:" IPSTR "]\n", IP2STR(&cual.myip));

				   activeNodes.nodesReported[cual.fromwho]=1;
				   time(&now);
				   activeNodes.lastTime[cual.fromwho]=now;

			   }
			   break;
		   case BLINK:
			   if(blinkHandle)
			   {
				   vTaskDelete(blinkHandle);
				   blinkHandle=NULL;
			   }
				REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
				xTaskCreate(&blinkLight,"blight",4096,(void*)sysLights.blinkLight, MGOS_TASK_PRIORITY, &blinkHandle);				//Manages all display to LCD
				break;
		   case LEDS:
			   sysConfig.showLeds=cual.free1;
			   write_to_flash();
			   break;
		   case FWARE:
			   if(runHandle!=NULL)
			   {
				   vTaskDelete(runHandle),
				   runHandle=NULL;
			   }
			   if(blinkHandle)
			   {
				   vTaskDelete(blinkHandle);
				   blinkHandle=NULL;
			   }
			   if(mqttHandle)
			   {
				   vTaskDelete(mqttHandle);
				   mqttHandle=NULL;
			   }

			   if(mongoHandle)
			   {
				   vTaskDelete(mongoHandle);
				   mongoHandle=NULL;
			   }

			   if(mdnsHandle)
			   {
				   vTaskDelete(mdnsHandle);
				   mdnsHandle=NULL;
			   }
			//   printf("Firmware Command In\n");
			   strcpy(elfw.ap,cual.buff);
			   strcpy(elfw.pass,&cual.buff[strlen(elfw.ap)+1]);
				esp_event_loop_set_cb(NULL,NULL);
				esp_wifi_stop(); //Stop Main Connection to AP
		//		printf("Stop\n");
		//		delay(1000);
				esp_wifi_deinit();
			//	printf("Deinit\n");
		//		delay(1000);
				//Will now connect to Internet AP
				// then download firmware
			   xTaskCreate(&set_FirmUpdateCmd,"dispMgr",10240,&elfw, MGOS_TASK_PRIORITY, NULL);
			   if(rxHandle) //harakiri
			   {
				   vTaskDelete(rxHandle);
				   rxHandle=NULL;
			   }
			   break;
		   default:
			   break;
	   }
	}
}

void runLight(void * pArg)
{
	cuantoDura=(int)pArg*FACTOR2;
	int demora=0,son=0;
	int restar=0;

	for (int a=0;a<sysLights.numLuces;a++)
		{
			if (!sysLights.lasLuces[a].typ)
				restar+=sysLights.lasLuces[a].valor;
		}

	cuantoDura-=(restar*FACTOR2);
#ifdef DEBUGSYS
	   if(sysConfig.traceflag & (1<<WEBD))
		   printf("[WEBD]RunLights Time %d\n",cuantoDura);
#endif

	for (int a=0;a<sysLights.numLuces;a++)
	{
		if(sysLights.lasLuces[a].typ)
			demora=cuantoDura*sysLights.lasLuces[a].valor/100;
		else
			demora=sysLights.lasLuces[a].valor*FACTOR2;

		globalLuz=a;
		globalLuzDuration=demora;

		if(a<sysLights.numLuces-1)
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<WEBD))
				printf("[WEBD]noLast Light %d bits %x delay %d type %d\n",a,sysLights.lasLuces[a].ioports,demora,sysLights.lasLuces[a].typ);
#endif
			if(sysLights.lasLuces[a].opt!=1) //Just turn ON
			{
				REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
				REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[a].ioports);//clear all set bits
				delay(demora);
			}
			else
			{
				son=demora/400;
				for (int b=0;b<son;b++)
				{
					REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
					delay(200);
					REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[a].ioports);//clear all set bits
					delay(200);
				}

			}
		}
		else
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<WEBD))
				printf("[WEBD]Last Light %d bits %x delay %d\n",a,sysLights.lasLuces[a].ioports,demora);
#endif
			REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
			REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[a].ioports);//clear all set bits
		}
	}
	if(!sysConfig.clone) // If not a clone send the DONE for the whole CLONE Group
		sendMsg(DONE,EVERYBODY,0,0,NULL,0);
	runHandle=NULL;
	vTaskDelete(NULL);
}

static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq ;
    struct in_addr iaddr;
    memset(&imreq,0,sizeof(imreq));
    memset(&iaddr,0,sizeof(iaddr));
    int err = 0;
    // Configure source interface

    tcpip_adapter_ip_info_t ip_info ;
    memset(&ip_info,0,sizeof(ip_info));
    if(sysConfig.mode!=1)
    	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    else
    	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP address info. Error 0x%x", err);
        goto err;
    }
    ESP_LOGI(TAG,"MULTIAssigned Ip %d:%d:%d:%d",IP2STR(&ip_info.ip));
    inet_addr_from_ipaddr(&iaddr, &ip_info.ip);
//#endif
    // Configure multicast address to listen to
    err = inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(TAG, "Configured IPV4 multicast address '%s' is invalid.", MULTICAST_IPV4_ADDR);
        goto err;
    }
    ESP_LOGI(TAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", MULTICAST_IPV4_ADDR);
    }

    	struct in_addr        localInterface;
		tcpip_adapter_ip_info_t if_info;
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &if_info);
		localInterface.s_addr =(in_addr_t) if_info.ip.addr;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
		{
			ESP_LOGE(TAG,"Setting local interface %d %s\n",errno,strerror(errno));
			close(sock);
			return 1;
		  }

//    if (assign_source_if) {
//        // Assign the IPv4 multicast source interface, via its IP
//        // (only necessary if this socket is IPV4 only)
//        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
//                         sizeof(struct in_addr));
//        if (err < 0) {
//            ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d %s", errno,strerror(errno));
//            goto err;
//        }
//    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        goto err;
    }

 err:
    return err;
}

int create_multicast_ipv4_socket(int port)
{
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));

    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket. Error %d %s", errno,strerror(errno));
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d %s", errno,strerror(errno));
        goto err;
    }


    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d %s", errno,strerror(errno));
        goto err;
    }


    // this is also a listening socket, so add it to the multicast
    // group for listening...
    err = socket_add_ipv4_multicast_group(sock, true);
    if (err < 0) {
    	ESP_LOGE(TAG, "Error group %d %s", errno,strerror(errno));
        goto err;
    }

    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}

void sendMsg(int cmd,int aquien,int f1,int f2,char * que,int len)
{
	int sock,err;
    struct in_addr        localInterface;
    struct sockaddr_in    groupSock;

	answer.centinel=THECENTINEL;
	answer.cmd=cmd;
	answer.nodeId=sysConfig.nodeid;
	answer.towho=aquien;
	answer.fromwho=sysConfig.whoami;
	answer.lapse=millis();
	answer.free1=f1;
	answer.free2=f2;
	answer.seqnum++;
	answer.myip=localIp;
	time(&answer.theTime);
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]SendMsg %s towho %d\n",tcmds[cmd],aquien);
#endif
	if(que && (len>0))
		memcpy(&answer.buff,que,len);
	tcpip_adapter_get_ip_info(sysConfig.mode?TCPIP_ADAPTER_IF_AP:TCPIP_ADAPTER_IF_STA, &answer.ipstuff);
	salen++;
		sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (sock < 0)
		{
			ESP_LOGE(TAG, "Failed to create socketl. Error %d %s", errno, strerror(errno));
			return;
		}

		//SET THE MULTICAST ADDRESS AND PORT
		 memset((char *) &groupSock, 0, sizeof(groupSock));
		 groupSock.sin_family = AF_INET;
		 groupSock.sin_addr.s_addr = inet_addr(MULTICAST_IPV4_ADDR);
		 groupSock.sin_port = htons(UDP_PORT);

		 // send ourselves the same message since we also are a station. Default is 0 so no message. Set it to 1
		char loopch=1;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,(char *)&loopch, sizeof(loopch)) < 0)
		{
			ESP_LOGE(TAG,"Setting IP_MULTICAST_LOOP:%d %s",errno,strerror(errno));
			close(sock);
			return;
		}

		//set the Interface we want to use.
		tcpip_adapter_ip_info_t if_info;
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &if_info);
		localInterface.s_addr =(in_addr_t) if_info.ip.addr;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
		{
			ESP_LOGE(TAG,"Setting local interface %d %s\n",errno,strerror(errno));
			close(sock);
			return;
		  }

		uint8_t ttl = MULTICAST_TTL; //Time To Live max

		err=setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
		if (err < 0)
		{
			ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL sockel. Error %d %s", errno,strerror(errno));
			close (sock);
			return;
		}

		//Send it
		err=sendto(sock, &answer, sizeof(answer), 0,(struct sockaddr*)&groupSock,sizeof(groupSock));
		if (err < 0)
		{
			ESP_LOGE(TAG, "IPV4 sendto failed. errno: %d %s", errno, strerror(errno));
			close(sock);
			return;
		}
		if(sysConfig.showLeds)
			blink(SENDLED);
		close(sock);
}

void rxMultiCast(void *pArg)
{
	cmd_struct comando;
    char raddr_name[32];
    struct sockaddr_in6 raddr; // Large enough for both IPv4 or IPv6
    u32 lastts=0;

	theSock = create_multicast_ipv4_socket(UDP_PORT);
	if (theSock < 0) {
		ESP_LOGE(TAG, "RX Failed to create IPv4 multicast socket");
	}

	while(1)
	{
	//	dale:
		memset(raddr_name,0,sizeof(raddr_name));

		socklen_t socklen = sizeof(raddr);
		int len = recvfrom(theSock, &comando, sizeof(comando), MSG_WAITALL,
						   (struct sockaddr *)&raddr, &socklen);

		if (len < 0) {

			ESP_LOGE(TAG, "multicast recvfrom failed: errno %d", errno);
			exit(1);
		}
		entrats=millis()-lastts;
		lastts=millis();

		// Get the sender's address as a string

		if (raddr.sin6_family == PF_INET) {
			inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr.s_addr,
						raddr_name, sizeof(raddr_name)-1);
		}

		string algo=string(raddr_name);
	//	printf("In from %s\n",algo.c_str());

		if (comando.centinel!=THECENTINEL)
			printf("Invalid centinel\n");
		else
			process_cmd(comando);
		}
  }


void mcast_example_task(void *pvParameters)
{
    int sock,err;
    cmd_struct comando;
    struct in_addr        localInterface;
    struct sockaddr_in    groupSock;

    ESP_LOGI(TAG, "RxTask started");

    while (true) {

        //sending thread. Get a standard socket not MC
        sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
          if (sock < 0) {
              ESP_LOGE(TAG, "Test Failed to create socket. Error %d", errno);
              exit(1);
          }


  		//SET THE MULTICAST ADDRESS AND PORT
  		 memset((char *) &groupSock, 0, sizeof(groupSock));
  		 groupSock.sin_family = AF_INET;
  		 groupSock.sin_addr.s_addr = inet_addr(MULTICAST_IPV4_ADDR);
  		 groupSock.sin_port = htons(UDP_PORT);

  		 // send ourselves the same message since we also are a station. Default is 0 so no message. Set it to 1
  		char loopch=1;
  		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,(char *)&loopch, sizeof(loopch)) < 0)
  		{
  			ESP_LOGE(TAG,"Test Setting IP_MULTICAST_LOOP:%d %s",errno,strerror(errno));
  			close(sock);
  			return;
  		}

  		//set the Interface we want to use.
  		tcpip_adapter_ip_info_t if_info;
  		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &if_info);
  		localInterface.s_addr =(in_addr_t) if_info.ip.addr;
  		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
  		{
  			ESP_LOGE(TAG,"Test Setting local interface %d %s\n",errno,strerror(errno));
  			close(sock);
  			return;
  		  }

  		uint8_t ttl = MULTICAST_TTL; //Time To Live max

  		err=setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
  		if (err < 0)
  		{
  			ESP_LOGE(TAG, "Test Failed to set IP_MULTICAST_TTL sockel. Error %d %s", errno,strerror(errno));
  			close (sock);
  			return;
  		}

          comando.centinel=THECENTINEL;
          comando.cmd=TEST;
          if (sysConfig.mode!=1)
        	  comando.towho=0;
          else
        	  comando.towho=255;
          comando.fromwho=sysConfig.whoami;

        while (true) {

            while(!rxtxf){
                ESP_LOGI(TAG, "RxTask stoped");
            //	freeaddrinfo(res);
            	close(sock);
            	vTaskDelete(NULL);
            }

            vTaskDelay(howmuch / portTICK_PERIOD_MS);
            comando.seqnum++;
            comando.lapse=millis();

            //send it
    		err=sendto(sock, &comando, sizeof(comando), 0,(struct sockaddr*)&groupSock,sizeof(groupSock));

//			err = sendto(sock, &comando, sizeof(comando), 0, res->ai_addr, res->ai_addrlen);
			if (err < 0) {
				ESP_LOGE(TAG, "IPV4 sendto failed. errno: %d", errno);
				exit(1);
			}
			if(sysConfig.showLeds)
				blink(SENDLED);
			salen++;
			if (!displayf)
			{
				printf("!");
				fflush(stdout);
			}
			else
				ESP_LOGI(TAG, "MCOut from %d to %d seqnum %d", comando.fromwho,comando.towho,comando.seqnum);
        }
    }

}
/*
u8 mac[8];
	char textl[20];
	string temp;
	int len;
	wifi_init_config_t 				cfg=WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t 					sta_config,configap;
	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));



	if (aqui.ssid[curSSID][0]!=0)
	{
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		temp=string(aqui.ssid[curSSID]);
		len=temp.length();
		memcpy(sta_config.sta.ssid,temp.c_str(),len+1);
		temp=string(aqui.pass[curSSID]);
		len=temp.length();
		memcpy(sta_config.sta.password,temp.c_str(),len+1);
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
	}

	else
	{
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
		esp_wifi_get_mac(ESP_IF_WIFI_STA, (u8*)&mac);
		sprintf(textl,"MeterIoT%02x%02x",mac[6],mac[7]);
		memcpy(configap.ap.ssid,textl,12);
		memcpy(configap.ap.password,"csttpstt\0",9);
		configap.ap.ssid[12]=0;
		configap.ap.password[9]=0;
		configap.ap.ssid_len=0;
		configap.ap.authmode=WIFI_AUTH_WPA_PSK;
		configap.ap.ssid_hidden=false;
		configap.ap.max_connection=4;
		configap.ap.beacon_interval=100;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &configap));
	}

	*/

void initWiFi()
{
	wifi_init_config_t 				cfg=WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t 					configap;
	char mac[8],textl[20];
	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	esp_wifi_set_ps(WIFI_PS_NONE); //otherwise multicast does not work well or at all

	if (string(sysConfig.ssid[0])=="")
	{
		printf("Start config puro\n");
		memset(&configap,0,sizeof(configap));
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
		esp_wifi_get_mac(ESP_IF_WIFI_STA, (u8*)&mac);
		sprintf(textl,"LightIoT%02x%02x",mac[6],mac[7]);
		printf("APName %s\n",textl);
		memcpy(&configap.ap.ssid,textl,12);
		memcpy(&configap.ap.password,textl,12);
		configap.ap.ssid[12]=0;
		configap.ap.password[12]=0;
		configap.ap.ssid_len=12;
		configap.ap.authmode=WIFI_AUTH_WPA_PSK;
		configap.ap.ssid_hidden=false;
		configap.ap.max_connection=4;
		configap.ap.beacon_interval=100;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &configap));
		ESP_ERROR_CHECK(esp_wifi_start());
		printf("Fin\n");
		return;
	}
	else

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	esp_wifi_get_mac(ESP_IF_WIFI_AP, (uint8_t*)&mac);
	memset(&configap,0,sizeof(configap));
	strcpy((char *)configap.sta.ssid , sysConfig.ssid[1]);
	strcpy((char *)configap.sta.password, sysConfig.pass[1]);
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &configap));
	strcpy((char *)configap.ap.ssid,sysConfig.ssid[0]);
	strcpy((char *)configap.ap.password,sysConfig.pass[0]);
	configap.ap.ssid_len=strlen((char *)configap.ap.ssid);
	configap.ap.authmode=WIFI_AUTH_WPA_PSK;
	configap.ap.ssid_hidden=false;
	configap.ap.max_connection=10;
	configap.ap.beacon_interval=100;
	ESP_LOGI(TAG,"AP %s",sysConfig.ssid[0]);
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &configap));
	ESP_ERROR_CHECK(esp_wifi_start());
}

void initWiFiSta()
{
	wifi_config_t sta_config;


	if (string(sysConfig.ssid[0])!="")
	{
		ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL));

		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		cfg.event_handler = &esp_event_send;
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
		ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
		esp_wifi_set_ps(WIFI_PS_NONE);
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

		int len;
		string temp=string(sysConfig.ssid[0]);
		len=temp.length();
		memcpy((void*)sta_config.sta.ssid,temp.c_str(),len);
		sta_config.sta.ssid[len]=0;
		temp=string(sysConfig.pass[0]);
		len=temp.length();
		memcpy((void*)sta_config.sta.password,temp.c_str(),len);
		sta_config.sta.bssid_set=0;
		sta_config.sta.password[len]=0;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
		ESP_ERROR_CHECK(esp_wifi_start());
		temp="";
	}
	else
	{
		printf("System not Configured\n");
		initWiFi();
	}

	// WiFi led
	gpio_set_direction((gpio_num_t)WIFILED, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)WIFILED, 0);
}

void initScreen()
{

	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		display.init();
		display.flipScreenVertically();
		display.clear();
		drawString(64,16,"WiFi",24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		drawString(64,40,"               ",10,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		drawString(64,40,string(sysConfig.ssid[curSSID]),10,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		xSemaphoreGive(I2CSem);
	}

}


void initVars()
{
	char textl[30];

	//We do it this way so we can have a single global.h file with EXTERN variables(when not main app)
	// and be able to compile routines in an independent file


	cola = xQueueCreate( 10, sizeof( u16 ) ); //DONE queue
	sonUid=0;

	//blinking stuff
	interval=100;
	howmuch=1000;
	FACTOR=sysConfig.reserved;
	if(FACTOR==0)
		FACTOR=1;
	FACTOR2=sysConfig.reserved2;
	if(FACTOR2==0)
		FACTOR2=10;
	runHandle=NULL;
	cycleHandle=NULL;
	blinkHandle=NULL;
	mongoHandle=NULL;
	mdnsHandle=NULL;
	mqttHandle=NULL;
	kalive=true;

	semaphoresOff=false;
	for (int a=0;a<20;a++){
		activeNodes.lastTime[a]=0;
		activeNodes.nodesReported[a]=-1;
	}
	activeNodes.reported=0;

	uint16_t a=esp_random();
	sprintf(textl,"Traffic%04d",a);
	idd=string(textl);
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<BOOTD))
		printf("[BOOTD]Id %s\n",textl);
#endif

	settings.host=sysConfig.mqtt;
	settings.port = sysConfig.mqttport;
	settings.client_id=strdup(idd.c_str());
	settings.username=sysConfig.mqttUser;
	settings.password=sysConfig.mqttPass;
	settings.event_handle = mqtt_event_handler;
	settings.user_context =sysConfig.mqtt; //name of server
	settings.transport=0?MQTT_TRANSPORT_OVER_SSL:MQTT_TRANSPORT_OVER_TCP;
	settings.buffer_size=2048;
	settings.disable_clean_session=true;

	strcpy(APP,"TrafficIoT");

	strcpy(lookuptable[0].key,"BOOTD");
	strcpy(lookuptable[1].key,"WIFID");
	strcpy(lookuptable[2].key,"MQTTD");
	strcpy(lookuptable[3].key,"PUBSUBD");
	strcpy(lookuptable[4].key,"MONGOOSED");
	strcpy(lookuptable[5].key,"CMDD");
	strcpy(lookuptable[6].key,"WEBD");
	strcpy(lookuptable[7].key,"GEND");
	strcpy(lookuptable[8].key,"TRAFFICD");
	strcpy(lookuptable[9].key,"ALIVED");
	strcpy(lookuptable[10].key,"MQTTT");
	strcpy(lookuptable[11].key,"HEAPD");

//	for (int i=NKEYS/2;i<NKEYS;i++) //Do the - version of trace
//		{
//			strcpy(lookuptable[i].key,"-");
//			strcat(lookuptable[i].key,lookuptable[i-NKEYS/2].key);
//			printf("Look %d=%s\n",i,lookuptable[i].key);
//			lookuptable[i].val=i*-1;
//			lookuptable[i-NKEYS/2].val=i-NKEYS/2;
//		}

	strcpy(lookuptable[12].key,"-BOOTD");
	strcpy(lookuptable[13].key,"-WIFID");
	strcpy(lookuptable[14].key,"-MQTTD");
	strcpy(lookuptable[15].key,"-PUBSUBD");
	strcpy(lookuptable[16].key,"-MONGOOSED");
	strcpy(lookuptable[17].key,"-CMDD");
	strcpy(lookuptable[18].key,"-WEBD");
	strcpy(lookuptable[19].key,"-GEND");
	strcpy(lookuptable[20].key,"-TRAFFICD");
	strcpy(lookuptable[21].key,"-ALIVED");
	strcpy(lookuptable[22].key,"-MQTTT");
	strcpy(lookuptable[23].key,"-HEAPD");

	//enum {START,STOP,ACK,NAK,DONE,PING,PONG,SENDC,COUNTERS,TEST,INTERVAL,DELAY,QUIET,RESETC,RESET,NEWID,RUN,OFF,ON,RUALIVE,IMALIVE,KILL};


	strcpy(tcmds[0],"START");
	strcpy(tcmds[1],"STOP");
	strcpy(tcmds[2],"ACK");
	strcpy(tcmds[3],"NAK");
	strcpy(tcmds[4],"DONE");
	strcpy(tcmds[5],"PING");
	strcpy(tcmds[6],"PONG");
	strcpy(tcmds[7],"SENDC");
	strcpy(tcmds[8],"COUNTERS");
	strcpy(tcmds[9],"TEST");
	strcpy(tcmds[10],"INTERVAL");
	strcpy(tcmds[11],"DELAY");
	strcpy(tcmds[12],"QUIET");
	strcpy(tcmds[13],"RESETC");
	strcpy(tcmds[14],"RESET");
	strcpy(tcmds[15],"NEWID");
	strcpy(tcmds[16],"RUN");
	strcpy(tcmds[17],"OFF");
	strcpy(tcmds[18],"ON");
	strcpy(tcmds[19],"RUALIVE");
	strcpy(tcmds[20],"IMALIVE");
	strcpy(tcmds[21],"KILL");
	strcpy(tcmds[22],"BLINK");
	strcpy(tcmds[23],"LEDS");
	strcpy(tcmds[24],"FIRMW");

	for (int a=0;a<NKEYS;a++)
		if(a<(NKEYS/2))
			lookuptable[a].val=a;
		else
			lookuptable[a].val=a*-1;

	strcpy(kbdTable[0],"Blink");
	strcpy(kbdTable[1],"Factor");
	strcpy(kbdTable[2],"Ports");
	strcpy(kbdTable[3],"Light");
	strcpy(kbdTable[4],"Cycle");
	strcpy(kbdTable[5],"Schedule");
	strcpy(kbdTable[6],"Connected");
	strcpy(kbdTable[7],"Id");
	strcpy(kbdTable[8],"Firmware");
	strcpy(kbdTable[9],"Logclear");
	strcpy(kbdTable[10],"Log");
	strcpy(kbdTable[11],"Quit");
	strcpy(kbdTable[12],"Trace");
	strcpy(kbdTable[13],"Temperature");
	strcpy(kbdTable[14],"Status");
	strcpy(kbdTable[15],"MqttId");
	strcpy(kbdTable[16],"AccessPoint");
	strcpy(kbdTable[17],"Delay");
	strcpy(kbdTable[18],"Interval");
	strcpy(kbdTable[19],"Mode");
	strcpy(kbdTable[20],"Start");
	strcpy(kbdTable[21],"Stop");
	strcpy(kbdTable[22],"Ping");
	strcpy(kbdTable[23],"Counters");
	strcpy(kbdTable[24],"ResetCount");
	strcpy(kbdTable[25],"Reset");
	strcpy(kbdTable[26],"NewId");
	strcpy(kbdTable[27],"Statistics");
	strcpy(kbdTable[28],"Zero");
	strcpy(kbdTable[29],"Display");
	strcpy(kbdTable[30],"Settings");
	strcpy(kbdTable[31],"Beat");
	strcpy(kbdTable[32],"StopCycle");
	strcpy(kbdTable[33],"Alive");
	strcpy(kbdTable[34],"Streets");
	strcpy(kbdTable[35],"Help");
	strcpy(kbdTable[36],"Quiet");
	//Set up Mqtt Variables
	spublishTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/MSG";
	cmdTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/CMD";

	strcpy(MQTTSERVER,"m13.cloudmqtt.com");

	strcpy(meses[0],"Ene");
	strcpy(meses[1],"Feb");
	strcpy(meses[2],"Mar");
	strcpy(meses[3],"Abr");
	strcpy(meses[4],"May");
	strcpy(meses[5],"Jun");
	strcpy(meses[6],"Jul");
	strcpy(meses[7],"Ago");
	strcpy(meses[8],"Sep");
	strcpy(meses[9],"Oct");
	strcpy(meses[10],"Nov");
	strcpy(meses[11],"Dic");

	daysInMonth [0] =31;
	daysInMonth [1] =28;
	daysInMonth [2] =31;
	daysInMonth [3] =30;
	daysInMonth [4] =31;
	daysInMonth [5] =30;
	daysInMonth [6] =31;
	daysInMonth [7] =31;
	daysInMonth [8] =30;
	daysInMonth [9] =31;
	daysInMonth [10] =30;
	daysInMonth [11] =31;

	// set pairs of "command name" with Function to be called
	// OJO commandos son con el backslash incluido ej: /mt_HttpStatus y no mt_HttpStatus a secas!!!!

	strcpy((char*)&cmds[0].comando,"/tf_test");			cmds[0].code=set_test;			//done...needs testing in a good esp32

	barX[0]=0;
	barX[1]=6;
	barX[2]=12;

	barH[0]=5;
	barH[1]=10;
	barH[2]=15;

	displayf=true;
	mongf=false;
	//compile_date[] = __DATE__ " " __TIME__;

	logText[0]="System booted";
	logText[1]="Log CLeared";
	logText[2]="Firmware Updated";
	logText[3]="General Error";
	logText[4]="Door Open-Close";
	logText[5]="Log";
	logText[6]="Door reset";
	logText[7]="Ap Set";
	logText[8]="Internal";
	logText[9]="Door Activated";
	logText[10]="Named";
	logText[11]="Open canceled";
	logText[12]="Door Stuck";
	logText[13]="Sleep Mode";
	logText[14]="Active Mode";
	logText[15]="Break Mode";
	logText[16]="Guard Disconnected";
	logText[17]="Heap Guard";

}

void initRtc()
{
	DateTime algo;

	rtc.begin(i2cp.i2cport);		// RTC
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		algo=rtc.now();
		xSemaphoreGive(I2CSem);
	}
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<BOOTD))
		printf("[BOOTD]RTC Year %d Month %d Day %d Hora %d Min %d Sec %d Week %d\n",algo.year(),algo.month(),algo.date(),algo.hour(),algo.minute(),algo.second(),algo.dayOfWeek());
#endif
	mesg=oldMesg=algo.month()-1;                       // Global Month
	diag=oldDiag=algo.date()-1;                         // Global Day
	horag=oldHorag=algo.hour()-1-5;                      // Global Hour - 5 UIO
	yearg=algo.year();
	if(oldMesg>12 || oldDiag>31 || oldHorag>23) //Sanity check
		oldMesg=oldDiag=1;
//	rtcf=true;

	//Now load system time for internal use

	u32 now=algo.getEpoch()-(5*3600);
	struct timeval tm;
	tm.tv_sec=now;
	tm.tv_usec=0;
	settimeofday(&tm,0); //Now local time is set. It could be changed from the SNTP if we get a connections else we use this date

	struct tm timeinfo;
	time_t tt;
	time(&tt);
	localtime_r(&tt, &timeinfo);
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<BOOTD))
		printf("[BOOTD]RTC->UNIX Date %s yDay %d\n",asctime(&timeinfo),timeinfo.tm_yday);
#endif
}


int init_log()
{
	const char *base_path = "/spiflash";
	static wl_handle_t	s_wl_handle=WL_INVALID_HANDLE;
	esp_vfs_fat_mount_config_t mount_config={0,0,0};

	mount_config.max_files=1;
	mount_config.format_if_mount_failed=true;

	llogf=false;

	logQueue=xQueueCreate(10,sizeof(logq));
	// Create Queue
	if(logQueue==NULL)
		return -1;

	logSem= xSemaphoreCreateBinary();
	if(logSem)
		xSemaphoreGive(logSem);  //SUPER important else its born locked
	else
		printf("Cant allocate Log Sem\n");

	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		printf( "Failed to mount FATFS %d \n", err);
		return -1;
	}
	bitacora = fopen("/spiflash/log.txt", "r+");
	if (bitacora == NULL) {
		bitacora = fopen("/spiflash/log.txt", "a");
		if(bitacora==NULL)
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag&(1<<BOOTD))
			printf("[BOOTD]Could not open file\n");
#endif
			return -1;
		}
#ifdef DEBUGSYS
		else
			if(sysConfig.traceflag&(1<<BOOTD))
				printf("[BOOTD]Opened file append\n");
#endif
	}
	llogf=true;

	return ESP_OK;
}

void read_flash()
{
		esp_err_t q ;
		size_t largo=sizeof(sysConfig);
			q=nvs_get_blob(nvshandle,"config",(void*)&sysConfig,&largo);

		if (q !=ESP_OK)
			printf("Error read %d\n",q);
}

void read_flash_seq()
{
		esp_err_t q ;
		size_t largo=sizeof(sysSequence);
			q=nvs_get_blob(seqhandle,"seq",(void*)&sysSequence,&largo);

		if (q !=ESP_OK)
			printf("Error read Seq %d\n",q);
}

void read_flash_cycles()
{
		esp_err_t q ;
		size_t largo=sizeof(allCycles);
			q=nvs_get_blob(seqhandle,"cycles",(void*)&allCycles,&largo);

		if (q !=ESP_OK)
			printf("Error read Cycles %d\n",q);
}

void read_flash_lights()
{
		esp_err_t q ;
		size_t largo=sizeof(sysLights);
			q=nvs_get_blob(lighthandle,"lights",(void*)&sysLights,&largo);

		if (q !=ESP_OK)
			printf("Error read Lights %d\n",q);
}

void init_temp()
{
	//Temp sensors
	numsensors=DS_init(DSPIN,bit12,&sensors[0][0]);
	if(numsensors==0)
		numsensors=DS_init(DSPIN,bit12,&sensors[0][0]); //try again

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<BOOTD))
	{
		for (int a=0;a<numsensors;a++)
		{
			printf("[BOOTD]Sensor %d Id=",a);
			for (int b=0;b<8;b++)
				printf("%02x",sensors[a][b]);
			delay(750);
			if(numsensors>0)
				printf(" Temp:%.02fC\n",DS_get_temp(&sensors[a][0]));
		}
	}
#endif
}

void reinitScreen()
{
	displayf=false;
	display.init();
	display.flipScreenVertically();
	display.clear();
	displayf=true;
}

bool loadScheduler()
{
	nextSchedule=0;
	u32 faltan=86400; //secs in a day. Should be Zero when finished
	time_t now;
	time(&now);
	 for (int a=0;a<sysSequence.numSequences;a++)
	 {
		 if (sysSequence.sequences[a].weekDay & TODAY)
		 {
//			 printf("Seq %d WeedDay %x Stop %d Start %d  Son %d\n",a,sysSequence.sequences[a].weekDay,
//							 sysSequence.sequences[a].stopSeq,sysSequence.sequences[a].startSeq,
//									 sysSequence.sequences[a].stopSeq-sysSequence.sequences[a].startSeq);
			 faltan -= (sysSequence.sequences[a].stopSeq-sysSequence.sequences[a].startSeq);
			 scheduler.seqNum[nextSchedule]=a; //Sequence number
			 scheduler.duration[nextSchedule]=sysSequence.sequences[a].stopSeq-sysSequence.sequences[a].startSeq; //Duration
			 nextSchedule++;
		 }
	 }
	 scheduler.howmany=nextSchedule;
	 if(faltan!=0)
	 {
		 printf("Schedule not complete %d\n",faltan);
		 nextSchedule=0;
		 return false;
	 }
	 return true;
}

void makeNodeTime(string cual, node_struct* losnodos)
{
	string s;
	uint16_t res=0;

	  char * pch;
//	  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
	  pch = strtok ((char*)cual.c_str(),"-");
	  losnodos->nodeid[res]=atoi(pch);
	  while (pch != NULL)
	  {
	//	  printf("Node %s-",pch);
		  pch = strtok (NULL,",");
		  losnodos->timeval[res]=atoi(pch);
		//  printf("Time %s\n",pch);
		  res++;
		  pch = strtok (NULL, "-");
		  if(pch!=NULL)
			  losnodos->nodeid[res]=atoi(pch);
	  }
	  losnodos->howmany=res;
	}

void doneCallback( TimerHandle_t xTimer )
{
	u16 soyYo=100;
	if(xTimer==doneTimer)
	{
		 //  vTimerSetTimerID( xTimer, ( void * ) 1 ); //time out
		//  xSemaphoreGive(ackSem);//force a done to the waiting task
		  xQueueSend( cola, &soyYo,( TickType_t ) 0 ); //use a high number to signal Timeout

		//  printf("Timeout\n");
	}
}

void cycleManager(void * pArg)
{
	char theNodes[50];
	int voy=0,van=0;
	node_struct intersections;
	char textl[10];
	time_t now;

	int cual=(int)pArg; //cycle to use
	int este=sysSequence.sequences[cual].cycleId;

	while(!rxtxf)
	{
		cycleHandle=NULL;
		vTaskDelete(NULL);
	}

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<TRAFFICD))
		printf("[TRAFFICD]Schedule %d Cycle %d\n",cual,este);
#endif

	strcpy(theNodes,allCycles.nodeSeq[este]);
	makeNodeTime(theNodes,&intersections);

	doneTimer=xTimerCreate("DoneMsh",1,pdFALSE,( void * ) 0,&doneCallback); //just create it. Time will be changed per Node
		if(scheduleTimer==NULL)
			printf("Failed to create timer\n");
	while(true) //will be killed by timer call back every time schedule changes. Try to die gracefully and not abruptly
	{
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<TRAFFICD))
			printf("[TRAFFICD]Send %s %d secs\n",sysConfig.calles[intersections.nodeid[voy]],intersections.timeval[voy]);
#endif
		//sendmsg to nodes with time, wait for Done Semaphore

		if(displayf)
		{
			if(voy==0)
			{
				van++;
				if(van>5)
				{
					van=0;
					reinitScreen();
					setLogo(string(sysConfig.calles[intersections.nodeid[0]]));
				}
			}
			if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				gCycleTime=intersections.timeval[voy];
				sprintf(textl,"   %3ds   ",gCycleTime);
			//	gCycleTime-=3;
				eraseMainScreen();
				drawString(64, 20, string(sysConfig.calles[intersections.nodeid[voy]]),24, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
				drawString(90, 0, textl, 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				xSemaphoreGive(I2CSem);
			}
		}
			globalNode=intersections.nodeid[voy];
			globalDuration=intersections.timeval[voy]*FACTOR;

			time(&now);
			sendMsg(RUN,intersections.nodeid[voy],intersections.timeval[voy],0,(char*)&now,sizeof(now)); //Send date/time as server
			if(intersections.timeval[voy]<5)
				printf("TIMER Fault %d\n",intersections.timeval[voy]);

			xTimerGenericCommand(doneTimer,tmrCOMMAND_CHANGE_PERIOD,(intersections.timeval[voy]+2)*FACTOR,0,0);//MUST wait for done so 2 secs more and no timeout

			u32 st=millis();
			u16 soyYo;
			while(true)
			{
				if( xQueueReceive( cola, &soyYo, portMAX_DELAY ))
				{
					if(soyYo==intersections.nodeid[voy])
					{
						gCycleTime=-1;
						xTimerStop(doneTimer,0);
						u32 ulCount = ( uint32_t ) pvTimerGetTimerID( doneTimer );
						if(ulCount)
						{
#ifdef DEBUGSYS
							if(sysConfig.traceflag & (1<<TRAFFICD))
								printf("[TRAFFICD]DONE timeout %d\n",ulCount);
#endif
							vTimerSetTimerID( doneTimer, ( void * ) 0 ); //clear time out
							sendMsg(KILL,intersections.nodeid[voy],0,0,NULL,0);
							//Log timeout, send waring if x times,etc
						}

						u32 fueron=millis()-st;
#ifdef DEBUGSYS
						if(sysConfig.traceflag & (1<<TRAFFICD))
							printf("[TRAFFICD]DONE received %d\n",fueron);
#endif
						break;
					}
					else
					{
						if(soyYo>30){
							printf("Timeout for %d. Assumed its done\n",intersections.nodeid[voy]);
							break;
						}
						else
							printf("Talking out of turn. Possible configuration problem\n");
					}
				}
			}

			voy++;
			if (voy>=intersections.howmany)
				voy=0;

	}
}

void timerCallback( TimerHandle_t xTimer )
{
int este;
	if(xTimer==scheduleTimer)
	{
	//	printf("Timer expired. Current %d Howmany %d \n",scheduler.voy,scheduler.howmany);
		scheduler.voy++;
		if (scheduler.voy>scheduler.howmany-1)
			scheduler.voy=0;
		if(cycleHandle!=NULL)
			vTaskDelete(cycleHandle);
		cycleHandle=NULL;
		este=scheduler.seqNum[scheduler.voy];
		//reload timer new value
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<TRAFFICD))
			printf("\n\n\n[TRAFFICD]Next %d Timer in %d\n",scheduler.voy,scheduler.duration[scheduler.voy]);
#endif

		int sq= scheduler.seqNum[scheduler.voy];
		int cyc=sysSequence.sequences[sq].cycleId;
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<TRAFFICD))
			printf("[TRAFFICD]Next Timer %d\n",scheduler.duration[scheduler.voy]*FACTOR);
#endif
		xTimerGenericCommand(scheduleTimer,tmrCOMMAND_CHANGE_PERIOD,(scheduler.duration[scheduler.voy]*FACTOR) /portTICK_PERIOD_MS,NULL,0);
		if (allCycles.totalTime[cyc]!=0)
		{
			if(semaphoresOff)
			{
				sendMsg(ON,EVERYBODY,0,0,NULL,0);
				semaphoresOff=false;
			}
			xTaskCreate(&cycleManager,"cycle",4096,(void*)este, MGOS_TASK_PRIORITY, &cycleHandle);				//Manages all display to LCD
		}
		else
		{
			//send OFF message
			if(!semaphoresOff)
			{
#ifdef DEBUGSYS
				if(sysConfig.traceflag & (1<<TRAFFICD))
					printf("[TRAFFICD]Send Off Message\n");
#endif
				sendMsg(OFF,EVERYBODY,0,0,NULL,0);
				semaphoresOff=true;
			}
		}
		xTimerStart(scheduleTimer,0);
	}

}

void controller(void* arg)
{
//wait for time flag
int este,cual;
time_t now;
struct tm timeinfo ;

	while(!timef || !rxtxf)
		delay(100);
	//we got the Day to compare against TODAY
	//Set default light on

	if(!loadScheduler())
	{
		printf("Scheduler not enabled.\n");
		while(true)
			delay(10000);
	}
//	setenv("TZ", "EST5", 1); //UTC is 5 hours ahead for Quito
	now=time(NULL)-5*3600;
	localtime_r(&now, &timeinfo);
	timeinfo.tm_mday=1;
	timeinfo.tm_mon=0;
	timeinfo.tm_year=2000 - 1900;
	timeinfo.tm_isdst=-1;
	now=mktime(&timeinfo);

	scheduler.voy=0;
	for (int a=0;a<scheduler.howmany;a++)
	{
		cual=scheduler.seqNum[a];
		if(sysSequence.sequences[cual].startSeq<=now && now<sysSequence.sequences[cual].stopSeq)
		{
			scheduler.voy=a;
			//restart now-start a duration
			 u32 pasaron= now-sysSequence.sequences[cual].startSeq;
		//	printf("Now %d Start %d Duration %d pasaron %d\n",(u32)now,(u32)sysSequence.sequences[cual].startSeq,scheduler.duration[scheduler.voy],pasaron);
			 scheduler.duration[scheduler.voy]-=pasaron;
			// printf("Final %d\n",scheduler.duration[scheduler.voy]);
			break;
		}
	}
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<TRAFFICD))
		printf("[TRAFFICD]Start in timer %d\n",scheduler.voy);
#endif
	este=scheduler.seqNum[scheduler.voy];

	int sq= scheduler.seqNum[este];
	int cyc=sysSequence.sequences[sq].cycleId;

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<TRAFFICD))
		printf("[TRAFFICD]Timer %d Cycle %d in %d\n",scheduler.voy,cyc,allCycles.totalTime[cyc]);
#endif
	cycleHandle=NULL;

	if(allCycles.totalTime[cyc]>3)
		xTaskCreate(&cycleManager,"cycle",4096,(void*)este, MGOS_TASK_PRIORITY, &cycleHandle);				//Manages all display to LCD
	else
	{
		if(allCycles.totalTime[cyc]==0)
			sendMsg(OFF,EVERYBODY,0,0,NULL,0);
		else
			sendMsg(BLINK,EVERYBODY,0,0,NULL,0);
		semaphoresOff=true;
	}

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<TRAFFICD))
		printf("[TRAFFICD]First Timer %d Factor %d\n",scheduler.duration[scheduler.voy]*FACTOR,FACTOR);
#endif
	scheduleTimer=xTimerCreate("Schedule",(scheduler.duration[scheduler.voy]*FACTOR) /portTICK_PERIOD_MS,pdFALSE,( void * ) 0,&timerCallback);
		if(scheduleTimer==NULL)
			printf("Failed to create timer\n");
		else
			xTimerStart(scheduleTimer,0);
		vTaskDelete(NULL);
}

void heartBeat(void *pArg)
{
	while(true)
	{
		if(kalive)
			sendMsg(RUALIVE,EVERYBODY,0,0,NULL,0);
		if(sysConfig.keepAlive<10000)
			sysConfig.keepAlive=10000;
		delay(sysConfig.keepAlive);
	}
}

void app_main(void)
{
	//esp_log_level_set("*", ESP_LOG_ERROR); //shut up
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

	err = nvs_open("config", NVS_READWRITE, &nvshandle);
	if(err!=ESP_OK)
		printf("Error opening NVS File\n");
	else
		read_flash();

	tcpip_adapter_init();
	gpio_set_direction((gpio_num_t)0, GPIO_MODE_INPUT);
	delay(3000);
	reboot= rtc_get_reset_reason(1); //Reset cause for CPU 1

	traceflag=(debugflags)sysConfig.traceflag;

	err = nvs_open("lights", NVS_READWRITE, &lighthandle);
	if(err!=ESP_OK)
		printf("Error opening Lights File\n");
	else
		read_flash_lights();

if(sysConfig.mode)  //Scheduler only in Controller Mode
{
	err = nvs_open("seq", NVS_READWRITE, &seqhandle);
	if(err!=ESP_OK)
		printf("Error opening Seq File\n");
	else
	{
		read_flash_seq();
		read_flash_cycles();
	}
}

if (sysConfig.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
{
	printf("Erase Configuration\n");
	nvs_erase_all(nvshandle);
	nvs_erase_all(seqhandle);
	nvs_erase_all(lighthandle);
	nvs_commit(seqhandle);
	nvs_commit(lighthandle);
	nvs_commit(nvshandle);
	erase_config();
}
	printf("VersionEsp32-1.0.1\n");

	curSSID=sysConfig.lastSSID;
	initVars(); 			// used like this instead of var init to be able to have independent file per routine(s)
	initI2C();  			// for Screen
	initScreen();			// Screen
    init_temp();			// Temperature sensors
	init_log();				// Log file management
	initSensors();
	initRtc();

	gpio_set_level((gpio_num_t)sysLights.defaultLight, 1);

	keepAlive=sysConfig.keepAlive;//minute

	//Save new boot count and reset code
	sysConfig.bootcount++;
	sysConfig.lastResetCode=reboot;
	write_to_flash();

	rxtxf=false; //default stop

	memset(&answer,0,sizeof(answer));
	// Start Main Tasks
	xTaskCreate(&displayManager,"dispMgr",10240,NULL, MGOS_TASK_PRIORITY, NULL);				//Manages all display to LCD
	xTaskCreate(&kbd,"kbd",8192,NULL, MGOS_TASK_PRIORITY, NULL);								// User interface while in development. Erased in RELEASE
//	xTaskCreate(&logManager,"log",6144,NULL, MGOS_TASK_PRIORITY, NULL);						// Log Manager

//	ESP_LOGI(TAG, "Node Mode %s", sysConfig.mode?"Server":"Client");

	if(sysConfig.mode==0)
	    initWiFiSta();
	else
	    initWiFi();

	if(sysConfig.mode){
		xTaskCreate(&controller,"vm",10240,NULL, 5, NULL);									// If we are a Controller
		xTaskCreate(&heartBeat, "hearB", 4096, NULL, 4, NULL);
	}

}
