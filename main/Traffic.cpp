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

const char *TAG = "TFF";
extern void postLog(int code,int code1);
extern void timerManager(void *pArg);
extern void drawBars();

//forward local declarations
void processCmds(void * nc,cJSON * comands);
void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
void runLight(void * pArg);
void rxMessage(void * pArg);
void repeater(void * pArg);
void blinkLight(void *pArg);

using namespace std;

uint32_t IRAM_ATTR millis()
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(u32 a)
{
	vTaskDelay(a /  portTICK_RATE_MS);
}

void makeMd5(void *que, int len, void * donde)
{
	mbedtls_md_starts(&md5);
	mbedtls_md_update(&md5, (const unsigned char *) que, len);
	mbedtls_md_finish(&md5, (unsigned char *)donde);
}

void blink(int cual)
{
	gpio_set_level((gpio_num_t)cual, 1);
	delay(interval);
	gpio_set_level((gpio_num_t)cual, 0);
}

void eraseMainScreen()
{
	display.setColor((OLEDDISPLAY_COLOR)0);
	display.fillRect(0,19,127,32);
	display.setColor((OLEDDISPLAY_COLOR)1);
	display.display();
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

//mqtt incoming handler. Create a mqttMsg queue entry
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

	mqttQ = xQueueCreate( 20, sizeof( mensa ) ); //Our queue

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

int write_blob(nvs_handle theHandle, string theName, void *desde,int len,bool rec,void *md5start)
{
	int diff=(u8*)md5start-(u8*)desde;
	makeMd5(desde,diff,md5start);

	int q=nvs_set_blob(theHandle,theName.c_str(),desde,len);
	if (q !=ESP_OK)
	{
		printf("Error write %s %d\n",theName.c_str(),q);
		return ESP_FAIL;
	}

	q = nvs_commit(theHandle);
	if (q !=ESP_OK)
	{
		printf("Commit Error %s %d\n",theName.c_str(),q);
		return ESP_FAIL;
	}
	if (rec)
	 {
		q=nvs_set_blob(backhandle,theName.c_str(),desde,len);
		if(q!=ESP_OK)
		{
			printf("Recovery write Error %s Sequence\n",theName.c_str());
			return ESP_FAIL;
		}
		else
		{
			q=nvs_commit(backhandle);
			if(q!=ESP_OK)
			{
				printf("Recovery Sequence Commit %s error %d\n",theName.c_str(),q);
				return ESP_FAIL;
			}
		}
	 }
	return ESP_OK;
}

void write_to_flash_lights(bool rec) //save our configuration
{
	write_blob(lighthandle,"lights",(void*)&sysLights,sizeof(sysLights),rec,(void*)&sysLights.md5);
}

void write_to_flash_seq(bool rec) //save our configuration
{
	write_blob(seqhandle,"seq",(void*)&sysSequence,sizeof(sysSequence),rec,(void*)&sysSequence.md5);
}

void write_to_flash_cycles(bool rec) //save our configuration
{
	write_blob(seqhandle,"cycles",(void*)&allCycles,sizeof(allCycles),rec,(void*)&allCycles.md5);
}

void write_to_flash(bool rec) //save our configuration
{
	write_blob(nvshandle,"config",(void*)&sysConfig,sizeof(sysConfig),rec,(void*)&sysConfig.md5);
}

void get_traffic_name()
{
	char local[20];
	string appn;
	u8  mac[6];

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

int getParameter(arg* argument, string cual,char *donde)
{
	char paramr[50];

	if (argument->typeMsg ==1) //Json get parameter cual
	{
		cJSON *param= cJSON_GetObjectItem((cJSON*)argument->pMessage,cual.c_str());
		if(param)
		{
			memcpy(donde,param->valuestring,strlen(param->valuestring));
			return ESP_OK;
		}
		else
			return ESP_FAIL;
	}

	else //standard web server parameter
	{
		httpd_req_t *req=(httpd_req_t *)argument->pMessage;
		int buf_len = httpd_req_get_url_query_len(req) + 1;
		if (buf_len > 1)
		{
			char *buf = (char*)malloc(buf_len);
			if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
			{
				if (httpd_query_key_value(buf, cual.c_str(), paramr, sizeof(paramr)) == ESP_OK)
				{
					free(buf);
					memcpy(donde,paramr,strlen(paramr));
					return ESP_OK;
				}
				else
					free(buf);
			}
		 }
		return ESP_FAIL;
	}
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
	return ESP_FAIL;
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

void sendAlert(string que, int len)
{

	if(!mqttf)
		return;

	string lpublishTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/ALERT";

	#ifdef DEBUGSYS
					if(sysConfig.traceflag & (1<<PUBSUBD))
						printf("[PUBSUBD]Alert Publish %s Msg %s\n",lpublishTopic.c_str(),que.c_str());
	#endif

	esp_mqtt_client_publish(gClient, (char*)lpublishTopic.c_str(), (char*)que.c_str(),que.length(), 0, 0);
}

void sendResponse(void * comm,int msgTipo,string que,int len,int code,bool withUid, bool retain)
{
	if(!mqttf)
		return;


#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<PUBSUBD))
		printf("[PUBSUBD]Type %d Sending response %s len=%d\n",msgTipo,que.c_str(),que.length());
#endif
	if(msgTipo==1)
	{ // MQTT Response
		esp_mqtt_client_handle_t mcomm=( esp_mqtt_client_handle_t)comm;
		string final;
		final=que;

		if (!mqttf)
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<MQTTD))
				printf("[MQTTD]No mqtt\n");
#endif
			return;
		}
		if(withUid)
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
		httpd_resp_send((httpd_req*)comm, que.c_str(), que.length());
	}
}

void initPorts()
{
	gpio_config_t io_conf;
	uint64_t mask=1;  //If we are going to use Pins >=32 needs to shift left more than 32 bits which the compilers associates with a const 1<<x. Stupid

	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = (mask<<WIFILED|mask<<MQTTLED|mask<<SENDLED|mask<<RXLED);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en =GPIO_PULLUP_DISABLE;
	gpio_config(&io_conf);

	if (sysLights.numLuces>0)
	{
		io_conf.pin_bit_mask =0;
		for (int a=0;a<sysLights.numLuces;a++)
				io_conf.pin_bit_mask = io_conf.pin_bit_mask |(mask<<sysLights.outPorts[a]);

		io_conf.intr_type = GPIO_INTR_DISABLE;
		io_conf.mode = GPIO_MODE_OUTPUT;
		io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
		io_conf.pull_up_en =GPIO_PULLUP_DISABLE;
		gpio_config(&io_conf);
		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits

		//inputs
		io_conf.pin_bit_mask =0;
		for (int a=0;a<sysLights.numLuces;a++)
				io_conf.pin_bit_mask = io_conf.pin_bit_mask |(mask<<sysLights.inPorts[a]);

		io_conf.intr_type = GPIO_INTR_DISABLE;
		io_conf.mode = GPIO_MODE_INPUT;
		io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
		io_conf.pull_up_en =GPIO_PULLUP_DISABLE;
 		gpio_config(&io_conf); //need to set these ports as inputs.
	}
}

void initialize_sntp(void *args)
{
	struct timeval tvStart;
	time_t now = 0;
	int retry = 0;
	struct tm timeinfo;

	if (sntpf)
		vTaskDelete(NULL);

	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, (char*)"pool.ntp.org");
	sntp_init();

	const int retry_count = 10;
//	setenv("TZ", "EST5", 1); //UTC is 5 hours ahead for Quito
//	tzset();

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
	{
		timef=true;
		vTaskDelete(NULL);
	}

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
	write_to_flash(true);
	timef=1;
	postLog(0,sysConfig.bootcount);
	rtc.setEpoch(now);
	vTaskDelete(NULL);
}

void ConfigSystem(void *pArg)
{
//	uint32_t del=(uint32_t)pArg;
	while(FOREVER)
	{
		gpio_set_level((gpio_num_t)WIFILED, 1);
		delay((uint32_t)pArg);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		delay((uint32_t)pArg);
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
	if(sysConfig.mode==SERVER)
	{
		if(displayf)
		{
		if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				drawString(64,42,"               ",10,TEXT_ALIGN_CENTER,DISPLAYIT,REPLACE);
				drawString(64,42,string(sysConfig.ssid[curSSID]),10,TEXT_ALIGN_CENTER,DISPLAYIT,REPLACE);
				xSemaphoreGive(I2CSem);
			}
		}
	}

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Try SSID =%s= %d %d\n",temp.c_str(),cual,len);
#endif
//	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	temp=string(sysConfig.ssid[cual]);
	len=temp.length();
	memcpy((void*)sta_config.sta.ssid,temp.c_str(),len);
//	strcpy(sta_config.sta.ssid,sysConfig.ssid[cual]);
//	strcpy(sta_config.sta.password,sysConfig.pass[cual]);
	sta_config.sta.ssid[len]=0;
	temp=string(sysConfig.pass[cual]);
	len=temp.length();
	memcpy((void*)sta_config.sta.password,temp.c_str(),len);
//	sta_config.sta.bssid_set=0;
//	sta_config.sta.password[len]=0;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	esp_wifi_start(); //if error try again indefinitely
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
//	printf("Firmware mode\n");
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

esp_err_t http_get_handler(httpd_req_t *req)
{
	arg *argument=(arg*)malloc(sizeof(arg));
#ifdef DEBUGSYS
	char *name=strtok(req->uri,(const char*)"?");
	if (name!=NULL)
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Webcmdrsn %s\n",name);
#endif
	argument->pMessage=req;
	argument->typeMsg=0;
	argument->pComm=req;
	(*(functp)req->user_ctx)(argument);
	free(argument);
    return ESP_OK;
}

void httpTask(void* pArg)
{
	int err;
	server=NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	config.max_uri_handlers=30;
	if (httpd_start(&server, &config) == ESP_OK)
	{
		for(int a=0;a<MAXCMDS;a++)
		{
			err=httpd_register_uri_handler(server, &loscmds[a]);
			if(err!=ESP_OK)
			printf("Error handler %d\n",err);
		}
	 }
	 vTaskDelete(NULL);
}

void login(void *pArg)
{
	int van=0;

	while(1)
	{
		sendMsg(LOGIN,EVERYBODY,sysConfig.nodeid,sysConfig.stationid,sysConfig.stationName,strlen(sysConfig.stationName));
		if(xSemaphoreTake(loginSemaphore, ( TickType_t ) 10000)) //every second try again
		{
			//we have a LoginAck for us. exit
#ifdef DEBUGSYS
			if(sysConfig.traceflag&(1<<WIFID))
				printf("[WIFID]Login Acked\n");
#endif
			loginf=true;
			loginHandle=NULL;
			vTaskDelete(NULL);
		}
		printf("Time out semaphore loginack\n");
		van++;
//		if(van>10)
//			vTaskDelete(NULL);
	}
}
void station_setup(system_event_t *event)
{
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

	if(sysConfig.mode==SERVER) //Server Mode Only
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
			 else
			 {
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
			xTaskCreate(&httpTask, "mongooseTask", 10240, NULL, 5, &mongoHandle); //  web commands Interface controller
			xTaskCreate(&initialize_sntp, "sntp", 2048, NULL, 3, NULL); //will get date
		}
	}

	// Main routine for Commands
	if(sysConfig.mode==CLIENT ){
		xTaskCreate(&login, "login", 4096, NULL, 4, NULL);
		xTaskCreate(&rxMessage, "rxMulti", 4096, (void*)0, 4, &rxHandle);
	}

}

void station_disconnected(system_event_t *event)
{
	string temp;

	connf=false;
	gpio_set_level((gpio_num_t)RXLED, 0);
	gpio_set_level((gpio_num_t)SENDLED, 0);
	gpio_set_level((gpio_num_t)MQTTLED, 0);
	gpio_set_level((gpio_num_t)WIFILED, 0);
	printf("RunHandle\n");
	if(runHandle)
	{
		vTaskDelete(runHandle);
		runHandle=NULL;
	}

	if(sysConfig.mode==CLIENT)
	{
		if(rxHandle){
			vTaskDelete(rxHandle);
			rxHandle=NULL;
		}
	}

	if(blinkHandle){
		vTaskDelete(blinkHandle);
		blinkHandle=NULL;
		xTaskCreate(&blinkLight, "blink", 4096, (void*)sysLights.defaultLight,(UBaseType_t) 3, &blinkHandle); //will get date
	}
	else
	{
		xTaskCreate(&blinkLight, "blink", 4096, (void*)sysLights.defaultLight, (UBaseType_t)3, &blinkHandle); //will get date
	}

	if(cycleHandle)
	{
		vTaskDelete(cycleHandle);
		cycleHandle=NULL;
	}

		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
		gpio_set_level((gpio_num_t)sysLights.defaultLight, 1);

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<WIFID))
		printf("[WIFID]Reconnect %d\n",curSSID);
#endif
	if(sysConfig.mode>0)
	{
		curSSID++;
		if(curSSID>1)
			curSSID=1;
		temp=string(sysConfig.ssid[curSSID]);
		if(temp=="")
			curSSID=1;
	}
	else
	{
		curSSID=0;
		temp=string(sysConfig.ssid[curSSID]);
	}

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<WIFID))
		printf("[WIFID]Temp[%d]==%s=\n",curSSID,temp.c_str());
#endif

//	xTaskCreate(&newSSID,"newssid",4096,(void*)curSSID, MGOS_TASK_PRIORITY, NULL);
}

esp_err_t wifi_event_handler_client(void *ctx, system_event_t *event)
{
	string 									local="Closed";

	switch(event->event_id)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Client Got IP\n");
#endif
		station_setup(event);
		break;

	case SYSTEM_EVENT_STA_START:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Client STA Start\n");
#endif
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_STOP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Client STA Stop\n");
#endif
		connf=false;
		gpio_set_level((gpio_num_t)RXLED, 0);
		gpio_set_level((gpio_num_t)SENDLED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 0);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
		break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
	case SYSTEM_EVENT_ETH_DISCONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Client STA Disconnected\n");
#endif
		station_disconnected(event);
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_CONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Client STA Connected \n");
#endif
		sysConfig.lastSSID=curSSID;
		write_to_flash(true);
		break;

	default:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Client default WiFi %d\n",event->event_id);
#endif
		break;
	}
	return ESP_OK;
}

esp_err_t wifi_event_handler_Server(void *ctx, system_event_t *event)
{
	wifi_sta_list_t 						station_list;
	wifi_sta_info_t 						*stations ;
	ip4_addr_t 								addr;

	switch(event->event_id)
	{
    case SYSTEM_EVENT_AP_STADISCONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server AP Disco\n");
#endif
      	break;

    case SYSTEM_EVENT_AP_STAIPASSIGNED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server AP Ip Assigned\n");
#endif
    	esp_wifi_ap_get_sta_list(&station_list);
    	stations=(wifi_sta_info_t*)station_list.sta;
    	dhcp_search_ip_on_mac(stations[station_list.num-1].mac , &addr);
    	connectedToAp[station_list.num-1]=addr.addr;
    	totalConnected=station_list.num;
    	break;

	case SYSTEM_EVENT_STA_GOT_IP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server Got Ip\n");
#endif
		station_setup(event);
		break;

	case SYSTEM_EVENT_AP_START:  // Handle the AP start event
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server AP Start\n");
#endif
		if(!rxmessagef)
		{
			printf("Launch Rxmessage %d\n",rxmessagef);
			xTaskCreate(&rxMessage, "rxMulti", 4096, (void*)0, 4, &rxHandle);
		}
		break;

	case SYSTEM_EVENT_AP_STOP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server AP Stopped\n");
#endif
		esp_wifi_start();
		break;

	case SYSTEM_EVENT_STA_START:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server STA Connect \n");
#endif
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_STOP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server STA Stopped\n");
#endif
		connf=false;
		gpio_set_level((gpio_num_t)RXLED, 0);
		gpio_set_level((gpio_num_t)SENDLED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 0);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server AP STA Connected\n");
#endif
		break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
	case SYSTEM_EVENT_ETH_DISCONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server STA Disconnected\n");
#endif
		station_disconnected(event);
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_CONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server STA Connected \n");
#endif
		sysConfig.lastSSID=curSSID;
		write_to_flash(true);
		break;

	default:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Server default WiFi %d\n",event->event_id);
#endif
		break;
	}
	return ESP_OK;
} // wifi_event_handler

void setup_repeater_ap()
{
	//Now that we have the Controller connected, start the AP
	wifi_config_t 				configap;
	tcpip_adapter_ip_info_t 	ip_info;

	//Connected to the Controller. Send our Login and verify answer
	xTaskCreate(&login, "login", 4096, NULL, 4, &loginHandle);

	if(string(sysConfig.ssid[0])!="") //in Controller and Repeater is SSID name
	{
		strcpy((char *)configap.ap.ssid,sysConfig.ssid[0]);
		strcpy((char *)configap.ap.password,sysConfig.pass[0]);
	}
	else
	{
		strcpy((char *)configap.ap.ssid,"Repeater");
		strcpy((char *)configap.ap.password,"2345678");
	}

	srand(time(NULL));   // Initialization, should only be called once.
	uint16_t r=esp_random() % 200;

	configap.ap.ssid_len=			strlen((char *)configap.ap.ssid);
	configap.ap.authmode=			WIFI_AUTH_WPA_PSK;
	configap.ap.ssid_hidden=		false;
	configap.ap.max_connection=		14;

	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
	tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
	IP4_ADDR(&ip_info.ip,192,168,r,1);
	IP4_ADDR(&ip_info.gw,192,168,r,1);
	IP4_ADDR(&ip_info.netmask,255,255,255,0);
	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
	tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &configap));
	ESP_ERROR_CHECK(esp_wifi_connect());
	printf("Repeater AP set connect\n");
}

esp_err_t wifi_event_handler_Repeater(void *ctx, system_event_t *event)
{
	wifi_sta_list_t 						station_list;
	wifi_sta_info_t 						*stations ;
	ip4_addr_t 								addr;
	system_event_sta_got_ip_t				elvent=(system_event_sta_got_ip_t)event->event_info.got_ip;


	switch(event->event_id)
	{
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
    	esp_wifi_ap_get_sta_list(&station_list);
    	stations=(wifi_sta_info_t*)station_list.sta;
    	dhcp_search_ip_on_mac(stations[station_list.num-1].mac , &addr);
    	connectedToAp[station_list.num-1]=addr.addr;
    	totalConnected=station_list.num;

#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater Station IP Assigned %s. Total %d\n",inet_ntoa(addr.addr),totalConnected);
#endif

    	break;

	case SYSTEM_EVENT_STA_GOT_IP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater Got Ip from Controller %s\n",inet_ntoa(elvent.ip_info.ip.addr));
#endif
		setup_repeater_ap(); //Start the AP
		//Repeater now active. Start our relayer
		if(!repeaterConf)
			xTaskCreate(&repeater, "repeater", 8092, (void*)0, 4, &rxHandle);
		repeaterConf=true;
		break;

	case SYSTEM_EVENT_AP_START:  // Handle the AP start event
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater AP Started\n");
#endif
		break;

	case SYSTEM_EVENT_AP_STOP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater AP Stopped\n");
#endif
		//kill our relayer
//		if(rxHandle)
//			vTaskDelete(rxHandle);
	//	esp_wifi_start();
		break;


	case SYSTEM_EVENT_STA_START:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater STA start Connect to Controller \n");
#endif
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_STOP:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater STA Stopped from Controller\n");
#endif
		connf=false;
		gpio_set_level((gpio_num_t)RXLED, 0);
		gpio_set_level((gpio_num_t)SENDLED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 0);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater Station Connected\n");
#endif
		break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
	case SYSTEM_EVENT_ETH_DISCONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater Controller Disconnected\n");
#endif
		if(repeaterConf)
		{
			repeaterConf=false;
			if(rxHandle) //Repeater active? kill it
			{
				vTaskDelete(rxHandle);
				rxHandle=NULL;
				printf("Kill Repeater Task\n");
			}
			// are we trying to login In? Kill it
			if(loginHandle)
			{
				vTaskDelete(loginHandle);
				loginHandle=NULL;
			}

			rebootf=true;
			station_disconnected(event);
//			if(!rebootf)
//			{ //First time the Controller is detected down
//				// stop the AP section so that they send Login
//				rebootf=true;
//				esp_wifi_get_config(ESP_IF_WIFI_AP,&config);
//				esp_wifi_get_config(ESP_IF_WIFI_STA,&config);
//				ESP_ERROR_CHECK(esp_wifi_stop());
//				delay(500);
//				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &config));
//				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &config));
//				ESP_ERROR_CHECK(esp_wifi_start());
//				//Must do a Stop then a Start again
//			}
		}
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_STA_CONNECTED:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater Controller Connected \n");
#endif
		if(rebootf)
		{
			printf("Must send login again\n");
		}
		break;

	default:
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Repeater default WiFi %d\n",event->event_id);
#endif
		break;
	}
	return ESP_OK;
}

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
    gClient=client;
    esp_err_t err;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            	esp_mqtt_client_subscribe(client, cmdTopic.c_str(), 0);
        		gpio_set_level((gpio_num_t)MQTTLED, 1);
        //		mqttCon=client;
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
        	//	mqttCon=0;
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

void show_leds(int cual)
{
	if(sysConfig.mode==CLIENT)
	{
	   switch (cual)
	   { //Valid Incoming Cmds related to this station
		   case PING:
		   case QUIET:
		   case NEWID:
		   case RESET:
		   case KILL:
		   case RUN:
		   case OFF:
		   case ON:
		   case RUALIVE:
		   case LEDS:
		   case FWARE:
		   case WALK:
		   case EXECW:
		   case LOGIN:
		   case CLONE:
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
}

void cmd_ack(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]ACK received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));

#endif
}

void cmd_nak(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]NAKC received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
}

void cmd_done(cmd_struct cual)
{
	   if(sysConfig.mode==SERVER)
	   {
#ifdef DEBUGSYS
		   if(sysConfig.traceflag & (1<<TRAFFICD))
			   printf("[TRAFFICD][%d-%d]DONE received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
		   xQueueSend( cola, ( void * ) &cual.fromwho,( TickType_t ) 0 );
	   }
}

void cmd_ping(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Ping received from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
#endif
				sendMsg(PONG,0,0,0,NULL,0);
}

void cmd_pong(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.mode==SERVER)
			   {
				   if(sysConfig.traceflag & (1<<TRAFFICD))
					   printf("[TRAFFICD][%d-%d]Pong from %d->" IPSTR "\n",cual.towho,sysConfig.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
	   	   	   }
#endif
}

void cmd_quiet(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]Quiet from %d to  %d \n",cual.towho,sysConfig.whoami,cual.fromwho,cual.free1);
#endif
			   displayf=cual.free1;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
}

void cmd_newid(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<TRAFFICD))
				   printf("[TRAFFICD][%d-%d]NewId from %d to %d \n",cual.towho,sysConfig.whoami,sysConfig.whoami,cual.free1);
#endif
	sysConfig.whoami=cual.free1;
	write_to_flash(true);
	sendMsg(ACK,cual.fromwho,0,0,NULL,0);
}

void cmd_reset(cmd_struct cual)
{
#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<CMDD))
				   printf("[CMDD]Reset unit after 5 seconds\n");
#endif
	sendMsg(ACK,cual.fromwho,0,0,NULL,0);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	esp_restart();
}

void cmd_kill(cmd_struct cual)
{
	if(runHandle!=NULL)
	{
	   vTaskDelete(runHandle),
	   runHandle=NULL;
	}
	gpio_set_level((gpio_num_t)sysLights.defaultLight, 1);
}

void cmd_runlight(cmd_struct cual) //THE routine. All for this process!!!!
{
	time_t		now;
	struct tm	timeinfo;
	timeval		tm;

   if(runHandle!=NULL) //do not duplicate task
   {
	   vTaskDelete(runHandle),
	   runHandle=NULL;
   }

   memcpy(&now,cual.buff,sizeof(now));

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
}

void cmd_off(cmd_struct cual)
{
#ifdef DEBUGSYS
   if(sysConfig.traceflag & (1<<TRAFFICD))
	   printf("[TRAFFICD]Turn Off TLight\n");
#endif
	if(runHandle!=NULL)
	{
	   vTaskDelete(runHandle),
	   runHandle=NULL;
	}
	REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
}

void cmd_on(cmd_struct cual)
{
#ifdef DEBUGSYS
   if(sysConfig.traceflag & (1<<TRAFFICD))
	   printf("[TRAFFICD]Turn On TLight\n");
#endif
	gpio_set_level((gpio_num_t)sysLights.defaultLight, 1); //Default light On
}

void cmd_are_you_alive(cmd_struct cual)
{
#ifdef DEBUGSYS
   if(sysConfig.traceflag & (1<<ALIVED))
	   printf("[ALIVED]Heartbeat Received\n");
#endif
	sendMsg(IMALIVE,EVERYBODY,sysConfig.nodeid,sysConfig.stationid,NULL,0);//stationid will be used for Alive.MUST be unique
}

void cmd_i_am_alive(cmd_struct cual)
{
	time_t now;
   if(sysConfig.mode==SERVER)
   {
#ifdef DEBUGSYS
	   if(sysConfig.traceflag & (1<<ALIVED))
		   printf("[ALIVED]Alive In from %d node %d station %d\n",cual.fromwho,cual.free1,cual.free2);
#endif
		//   printf( "[Remote IP:" IPSTR "]\n", IP2STR(&cual.myip));

	   activeNodes.nodesReported[cual.free2]=1;
	   activeNodes.dead[cual.free2]=false; //obviously
	   time(&now);
	   activeNodes.lastTime[cual.free2]=now;
	   //task to check every KEEPALIVE interval for time difference of current time-lastime(x)>KEEPALIVE, he is dead, do something
	   //station 0 is not relevant, since its the controller
   }
}

void cmd_blink(cmd_struct cual)
{
	if(blinkHandle)
	{
	   vTaskDelete(blinkHandle);
	   blinkHandle=NULL;
	}
	REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
	xTaskCreate(&blinkLight,"blight",4096,(void*)sysLights.blinkLight, MGOS_TASK_PRIORITY, &blinkHandle);
}

void cmd_leds(cmd_struct cual)
	{
	sysConfig.showLeds=cual.free1;
	write_to_flash(true);
}

void cmd_firmware(cmd_struct cual)
{
    firmware_type elfw;

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
}

void cmd_walk(cmd_struct cual)
{
	if(sysConfig.mode==SERVER)
	{
	#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]Walk In from %d NodeId %d Button %d\n",cual.fromwho,cual.nodeId, cual.free1);
	#endif
	//Inform the Street cual.free1;
	sendMsg(EXECW,cual.free1,0,0,NULL,0);
	}
}

void cmd_execute_walk(cmd_struct cual)
{
#ifdef DEBUGSYS
   if(sysConfig.traceflag & (1<<WIFID))
	   printf("[WIFID]Walk Execute from %d NodeId %d\n",cual.fromwho,cual.nodeId);
#endif
   globalWalk=true;
}

void cmd_clone(cmd_struct cual)
{
	#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<WIFID))
	   printf("[WIFID]Clone from %d NodeId %d Clone %s\n",cual.fromwho,cual.nodeId,sysConfig.clone?"Y":"N");
	#endif
	if(sysConfig.clone){
		if(memcmp((void*)&cual.buff,(void*)&sysLights,sizeof(sysLights))!=0)
		{
	#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<WIFID))
			printf("[WIFID]New Lights Configuration via Clone\n");
	#endif
			memcpy(&sysLights,&cual.buff,sizeof(sysLights));
			write_to_flash_lights(true);
		}
	}
}

void cmd_send_clone(cmd_struct cual)
{
#ifdef DEBUGSYS
   if(sysConfig.traceflag & (1<<WIFID))
	   printf("[WIFID]SendClone from %d NodeId %d Clone %s\n",cual.fromwho,cual.nodeId,sysConfig.clone?"Y":"N");
#endif
   if(!sysConfig.clone)
	   sendMsg(CLONE,sysConfig.whoami,0,0,(char*)&sysLights,sizeof(sysLights));
}

int find_station(u8 stat)
{
	for (int a=0;a<numLogins;a++)
		if(logins[a].stationl==stat)
			return a;
	return ESP_FAIL;
}

void cmd_login(cmd_struct cual)
{
	int donde;
	char textl[60];

	if(sysConfig.mode==SERVER) //Only server
	{
	#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<WIFID))
				   printf("[WIFID]Login %d Node %d Station(%d) %s Total %d\n",cual.fromwho,cual.free1,cual.free2,cual.buff,numLogins);
	#endif
		donde=find_station(cual.free2);
		if(donde==ESP_FAIL) //new login
		{ //new login
			logins[numLogins].nodel=cual.fromwho;
			logins[numLogins].stationl=cual.free2;
			memcpy(&logins[numLogins].namel,cual.buff,strlen(cual.buff));
			time(&logins[numLogins].timestamp);
			numLogins++;
		}
		else //was dead now alive or reboot login
		{
			struct tm  ts;
			time_t now;
			bool relogin=false;
			time(&now);
			localtime_r(&now, &ts);
			if(!activeNodes.dead[donde])
				//relogin, reboot or something. Not detected as dead
				relogin=true;
			activeNodes.dead[donde]=false;//resurrected
			time(&logins[donde].timestamp);
			sprintf(textl,"Station(%d) %s is %s at %s",logins[donde].stationl,logins[donde].namel,relogin?"Reboot":"Online",asctime(&ts));
			sendAlert(string(textl),strlen(textl));
		}
		sendMsg(ACKL,cual.fromwho,cual.free2,0,NULL,0);//ack the login station
	}
}

void cmd_alarm(cmd_struct cual)
{
char textl[70];

	if(sysConfig.mode==SERVER) //Only server
	{
		strcpy(textl,cual.buff);

	#ifdef DEBUGSYS
			   if(sysConfig.traceflag & (1<<WIFID))
				   printf("[WIFID]Alarm from %d Light(%d)%s Station %d %s\n",cual.fromwho,cual.free1,bulbColors[cual.free1],cual.free2,textl);
	#endif
			   burnt[cual.free2] |=1<<cual.free1; // bit setting
		sendAlert(string(textl),strlen(textl));
}
}

void cmd_acklogin(cmd_struct cual)
{
#ifdef DEBUGSYS
		   if(sysConfig.traceflag & (1<<WIFID))
			   printf("[WIFID]LoginAck %d Node %d Station(%d) %s\n",cual.fromwho,cual.free1,cual.free2,cual.buff);
#endif
		   if(cual.free1==sysConfig.stationid)
			   if(xSemaphoreGive(loginSemaphore)!= pdTRUE)
				   printf("Failed to give Ack Semaphore\n");

}

void process_cmd(cmd_struct cual)
{

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Process Cmd(%d) %s towho %d Fromwho %d Node %d Me %d Free1 %d Free2 %d\n",cual.cmd,tcmds[cual.cmd],
							cual.towho,cual.fromwho,cual.nodeId,sysConfig.whoami,cual.free1,cual.free2);
#endif

	if(((cual.towho==EVERYBODY) || (cual.towho==sysConfig.whoami)) && cual.nodeId==sysConfig.nodeid)
	{
		show_leds(cual.cmd);
		entran++;

	   switch (cual.cmd)
	   {
		   case ACK:
			   cmd_ack(cual);
			   break;
		   case NAK:
			   cmd_nak(cual);
			   break;
		   case DONE:
			   cmd_done(cual);
			   break;
		   case PING:
			   cmd_ping(cual);
			   break;
		   case PONG:
			   cmd_pong(cual);
			   break;
		   case QUIET:
			   cmd_quiet(cual);
			   break;
		   case NEWID:
			   cmd_newid(cual);
			   break;
		   case RESET:
			   cmd_reset(cual);
			   break;
		   case KILL:
			   cmd_kill(cual);
			   break;
		   case RUN:
			   cmd_runlight(cual);
			   break;
		   case OFF:
			   cmd_off(cual);
				break;
		   case ON:
			   cmd_on(cual);
				break;
		   case RUALIVE:
			   cmd_are_you_alive(cual);
				break;
		   case IMALIVE:
			   cmd_i_am_alive(cual);
			   break;
		   case BLINK:
			   cmd_blink(cual);
				break;
		   case LEDS:
			   cmd_leds(cual);
			   break;
		   case FWARE:
			   cmd_firmware(cual);
			   break;
		   case WALK:
			   cmd_walk(cual);
			   break;
		   case EXECW:
			   cmd_execute_walk(cual);
			   break;
		   case CLONE:
			   cmd_clone(cual);
			   break;
		   case SENDCLONE:
			   cmd_send_clone(cual);
			   break;
		   case LOGIN:
			   cmd_login(cual);
			   	   break;
		   case ALARM:
			   cmd_alarm(cual);
			   break;
		   case ACKL:
			   cmd_acklogin(cual);
			   break;
		   default:
			   printf("Invalid incoming Command %d\n",cual.cmd);
			   break;
	   }
	}
}

void reportLight(u32 expected, u32 readleds)
{
	u32 xored,bt;
	int fue=0;
	char textl[80];
	xored=(expected&readleds)^expected;
	for (int a=0;a<32;a++)
	{
		bt=1ul<<a;
		if((xored & bt))
		{
			if(!(sysLights.failed & (1ul<<a)))
			{
				fue=-1;
				for (int b=0;b<6;b++)
					if(sysLights.inPorts[b]==a)
					{
						fue=b;
						break;
					}
				sysLights.failed = sysLights.failed|(1ul<<a);
		//		write_to_flash_lights();
//				if(sysConfig.mode==SERVER)
//				{
//					sprintf(textl,"Street %s/%s Light %s failed",sysConfig.groupName,sysConfig.lightName, sysLights.theNames[fue]);
//					sendAlert(string(textl),strlen(textl));
//				}
//				else
//				{
					sprintf(textl,"%s station %s light failed",sysConfig.stationName, bulbColors[sysLights.theNames[fue]]);
					sendMsg(ALARM,EVERYBODY,fue,sysConfig.stationid,textl,strlen(textl));
//				}
			}
		}
	}
}

void runLight(void * pArg)
{
	int 					demora=0,son=0;
	int 					restar=0;
	u32 					ledStatus,tempread;
	bool 					qfue;

	cuantoDura=(int)pArg*FACTOR2;

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
				printf("[WEBD]noLast Light %d bits %x delay %d type %d opt %d\n",a,sysLights.lasLuces[a].ioports,demora,sysLights.lasLuces[a].typ,
						sysLights.lasLuces[a].opt);
#endif
			int copyOptions=sysLights.lasLuces[a].opt;
			if (copyOptions>1) //for walk and walk-blink
			{
				if (globalWalk)
					copyOptions-=2;
				else
					copyOptions=99; //Skip everything down. WALK button not pressed
			}

			if(copyOptions==0) //Just turn ON
			{
				REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
				REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[a].ioports);//clear all set bits
				u32 ledStatus=REG_READ(GPIO_IN_REG );//read bits
				delay(100);
				ledStatus=REG_READ(GPIO_IN_REG );//read bits
				if((sysLights.lasLuces[a].inports & ledStatus)!=sysLights.lasLuces[a].inports )
					reportLight(sysLights.lasLuces[a].inports, ledStatus);
#ifdef DEBUGSYS
				if(sysConfig.traceflag & (1<<WEBD))
				{
					tempread=sysLights.lasLuces[a].inports & ledStatus;
					if(tempread==sysLights.lasLuces[a].inports)
						qfue=true;
					else
						qfue=false;
					printf("[WEBD]Read GPIO %x expected %x and %x result %s\n",ledStatus,sysLights.lasLuces[a].inports,tempread,qfue?"True":"False");
				}
#endif
				delay(demora-100);
			}

			if(copyOptions==1) //Blink
			{
				son=demora/400;
				for (int b=0;b<son;b++)
				{
					REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);           //clear bits
					delay(200);
					REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[a].ioports);   //set bits
					delay(200);
					ledStatus=REG_READ(GPIO_IN_REG );//read bits
					if((sysLights.lasLuces[a].inports & ledStatus)!=sysLights.lasLuces[a].inports )
						reportLight(sysLights.lasLuces[a].inports, ledStatus);

#ifdef DEBUGSYS
					if(sysConfig.traceflag & (1<<WEBD))
					{
						tempread=sysLights.lasLuces[a].inports & ledStatus;
						if(tempread==sysLights.lasLuces[a].inports)
							qfue=true;
						else
							qfue=false;
						printf("[WEBD]BRead GPIO %x expected %x and %x result %s\n",ledStatus,sysLights.lasLuces[a].inports,tempread,qfue?"True":"False");
					}
#endif
				}
			}
		}
		else
		{
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<WEBD))
				printf("[WEBD]Last Light %d bits %x delay %d\n",a,sysLights.lasLuces[a].ioports,demora);
#endif
			REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
			REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[a].ioports);//clear all set bits
			delay(100);
			ledStatus=REG_READ(GPIO_IN_REG );//read bits
			if((sysLights.lasLuces[a].inports & ledStatus)!=sysLights.lasLuces[a].inports )
				reportLight(sysLights.lasLuces[a].inports, ledStatus);
#ifdef DEBUGSYS
			if(sysConfig.traceflag & (1<<WEBD))
			{
				tempread=sysLights.lasLuces[a].inports & ledStatus;
				if(tempread==sysLights.lasLuces[a].inports)
					qfue=true;
				else
					qfue=false;
				printf("[WEBD]LRead GPIO %x expected %x and %x result %s\n",ledStatus,sysLights.lasLuces[a].inports,tempread,qfue?"True":"False");
			}
#endif
		}
	}
	if(!sysConfig.clone) // If not a clone send DONE for the whole CLONE Group
		sendMsg(DONE,EVERYBODY,0,0,NULL,0);
	globalWalk=false;
	runHandle=NULL;
	vTaskDelete(NULL);
}


static int socket_add_ipv4_multicast_group(int sock,int ap)
{
    struct ip_mreq 				imreq ;
    struct in_addr 				iaddr,localInterface;
    int 						err = 0;
    tcpip_adapter_ip_info_t 	ip_info ;


    memset(&imreq,0,sizeof(imreq));
    memset(&iaddr,0,sizeof(iaddr));
    memset(&ip_info,0,sizeof(ip_info));

	imreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IPV4_ADDR);

	ESP_LOGI(TAG, "Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
	err=setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&imreq, sizeof(struct ip_mreq));
	if (err < 0)
	{
		ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
		return err;
	 }

    if(ap==CLIENT || ap==SERVER) //Client and Server only in one direction
    {
        if(ap==CLIENT)  //Client always to STA
        	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
        else //Server to AP
        	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
		localInterface.s_addr =(in_addr_t) ip_info.ip.addr;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
		{
			printf("Setting local interface %d %s\n",errno,strerror(errno));
			close(sock);
			return err;
		  }
    }
    else
    {
    	//Repeater in both directions
    	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
		localInterface.s_addr =(in_addr_t) ip_info.ip.addr;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
		{
			printf("Setting local interface %d %s\n",errno,strerror(errno));
			close(sock);
			return err;
		  }
    	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
		localInterface.s_addr =(in_addr_t) ip_info.ip.addr;
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
		{
			printf("Setting local interface %d %s\n",errno,strerror(errno));
			close(sock);
			return err;
		  }
    }
	return ESP_OK;
}

int create_multicast_ipv4_socket(int port,int ap)
{
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));

    printf("Create MultiSocket Port %d AP %d\n",port,ap);
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        printf("Failed to create socket. Error %d %s\n", errno,strerror(errno));
        return -1;
    }
 //  printf("Socket Address %d.%d.%d.%d\n",IP2STR(&ip_info.ip));

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    {
        printf("setsockopt(SO_REUSEADDR) failed\n");
        return ESP_FAIL;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        printf("Failed to bind socket. Error %d %s\n", errno,strerror(errno));
        goto err;
    }
printf("Binded\n");
    err = socket_add_ipv4_multicast_group(sock,ap);
    if (err < 0) {
    	printf("Error group %d %s\n", errno,strerror(errno));
        goto err;
    }
printf("Done create socket\n");
    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}

void sendMsg(int cmd,int aquien,int f1,int f2,char * que,int len)
{
	int 						sock,err;
    struct in_addr       		localInterface;
    struct sockaddr_in   		groupSock;
    tcpip_adapter_ip_info_t 	ip_info ;


	answer.centinel=THECENTINEL;
	answer.cmd=cmd;
	answer.nodeId=sysConfig.nodeid;
	answer.towho=aquien;
	answer.fromwho=sysConfig.whoami;
	answer.lapse=millis();
	answer.free1=f1;
	answer.free2=f2;
	answer.seqnum++;
	if(sysConfig.mode==SERVER)
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP,&answer.ipstuff);
	else
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA,&answer.ipstuff);
//	answer.myip=localIp;
	time(&answer.theTime);

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]%s(%d) SendMsg(%s) towho %d\n",sysConfig.stationName,sysConfig.stationid,tcmds[cmd],aquien);
#endif
	memset(&answer.buff,0,sizeof(answer.buff));
	if(que && (len>0))
		memcpy(&answer.buff,que,len);

	//tcpip_adapter_get_ip_info(sysConfig.mode?TCPIP_ADAPTER_IF_AP:TCPIP_ADAPTER_IF_STA, &answer.ipstuff);

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
//	tcpip_adapter_ip_info_t if_info;
	if(sysConfig.mode==CLIENT || sysConfig.mode==REPEATER)
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);//repeater and client send to the STA
	else
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info); //server sends to the AP

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("SendMsg to Ip %d:%d:%d:%d\n",IP2STR(&ip_info.ip));
#endif

	localInterface.s_addr =(in_addr_t) ip_info.ip.addr;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
	{
		ESP_LOGE(TAG,"Setting local interface %d %s\n",errno,strerror(errno));
		close(sock);
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

void rxMessage(void *pArg)
{
	cmd_struct 	comando;
    char 		raddr_name[32];
    struct 		sockaddr_in6 raddr; // Large enough for both IPv4 or IPv6
    u32 		lastts=0;
    int 		theSock;

    rxmessagef=true;
	theSock = create_multicast_ipv4_socket(UDP_PORT,sysConfig.mode);
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
			vTaskDelete(NULL);
			//exit(1);
		}

		entrats=millis()-lastts;
		lastts=millis();


#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
	{
		// Get the sender's address as a string
		memset(raddr_name,0,sizeof(raddr_name));
		if (raddr.sin6_family == PF_INET)
			inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr.s_addr,raddr_name, sizeof(raddr_name)-1);
		printf("[CMDD]In from %s seq %d msg %s\n",raddr_name,comando.seqnum, comando.buff);
	}
#endif

		if (comando.centinel!=THECENTINEL)
			printf("Invalid centinel\n");
		else
			process_cmd(comando);
		}
  }

void streamTask(void *pArg) //for Repeater configuration
{
	cmd_struct 				comand;
	int 					sock;
    struct sockaddr_in   	groupSock;
    struct in_addr        	localInterface;
	tcpip_adapter_ip_info_t if_info;
    int 					err;
    bool					ap=(int)pArg;

    while(true)
    {
    	//create socket
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

		//set the Interface we want to use.
		tcpip_adapter_get_ip_info(ap?TCPIP_ADAPTER_IF_STA:TCPIP_ADAPTER_IF_AP, &if_info);//for repeater AP and STA should be interchangeable
		localInterface.s_addr =(in_addr_t) if_info.ip.addr;
		ESP_LOGI(TAG,"StreamSendMsg %s to Ip %d:%d:%d:%d",ap?"UP":"DOWN",IP2STR(&if_info.ip));
		if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,(char *)&localInterface,sizeof(localInterface)) < 0)
		{
			ESP_LOGE(TAG,"Setting local interface %d %s\n",errno,strerror(errno));
			close(sock);
			return;
		  }

		while(true)
		{

			if( xQueueReceive( ap?upQ:downQ, &comand, portMAX_DELAY ))
			{
#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<TRAFFICD))
				printf("[TRAFFICD]Sending %s CMD(%d) %s Ip %d:%d:%d:%d\n",ap?"Up":"Down",comand.cmd,tcmds[comand.cmd],IP2STR(&if_info.ip));
#endif
				err=sendto(sock, &comand, sizeof(cmd_struct), 0,(struct sockaddr*)&groupSock,sizeof(groupSock));
				if (err < 0)
				{
					ESP_LOGE(TAG, "IPV4 sendto failed. errno: %d %s", errno, strerror(errno));
					close(sock); //we are going to die anyway. Should do something usefull like blink all lights
					break; //reopen socket, etc
				}
			}
		}
    }
}

void repeater(void *pArg) //connected to the STA
{
	cmd_struct 	comando;
    char 		raddr_name[32];
    struct 		sockaddr_in6 raddr; // Large enough for both IPv4 or IPv6
    u32 		lastts=0;
    int 		theSock;
    u32			me;
    tcpip_adapter_ip_info_t 	ip_info ;

    printf("Repeater started\n");
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
	me=ip_info.ip.addr;
	u8 monton[4];
	memcpy(&monton,(void*)&ip_info.ip.addr,4);
	monton[3]=0;
	memcpy(&downstream,&monton,4);
	downstream=downstream<<8;

	// very important for the repeater to decide if its up or downstream. Down if for the AP section
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
	memcpy(&monton,(void*)&ip_info.ip.addr,4);
	monton[3]=0;
	memcpy(&upstream,&monton,4);
	upstream=upstream<<8;

	theSock = create_multicast_ipv4_socket(UDP_PORT,sysConfig.mode);
	if (theSock < 0)
	{
		printf("RX Failed to create IPv4 multicast socket\n");
		vTaskDelete(NULL);
	}

	xTaskCreate(&streamTask,"upstream",4096,(void*)1, 5, NULL);
	xTaskCreate(&streamTask,"downstream",4096,(void*)0, 5, NULL);
	repeaterConf=true;
	while(1)
	{
		socklen_t socklen = sizeof(raddr);
		int len = recvfrom(theSock,(void*)&comando, sizeof(comando), MSG_WAITALL,(struct sockaddr *)&raddr, &socklen);

		if (len < 0)
		{
			printf("multicast recvfrom failed: errno %d\n", errno);
			exit(1);
		}

		if (comando.centinel==THECENTINEL)
		{
		entrats=millis()-lastts;
		lastts=millis();

#ifdef DEBUGSYS
		if(sysConfig.traceflag & (1<<TRAFFICD))
		{
			memset(raddr_name,0,sizeof(raddr_name));
			if (raddr.sin6_family == PF_INET)
				inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr.s_addr,raddr_name, sizeof(raddr_name)-1);
			printf("[TRAFFICD]Repeater In from %s seq %d msg %s towho %d\n",raddr_name,comando.seqnum, comando.buff,comando.towho);
		}
#endif


		u32 este=((struct sockaddr_in *)&raddr)->sin_addr.s_addr;
		if(me != este)
		{
			u8 monton[4];
			u32 mask=0x00ffffff;
			u32 res=(este&mask)<<8;
			memcpy(monton,&res,4);
	#ifdef DEBUGSYS
			if(sysConfig.traceflag&(1<<TRAFFICD))
				printf("[TRAFFIC]Repeater RxMess for STA %d %d %d %d r=%x d=%x u=%x\n",monton[0],monton[1],monton[2],monton[3],res,downstream,upstream);
	#endif
			if(res==upstream)
			{
				xQueueSend( upQ, &comando,( TickType_t ) 0 ); //use a high number to signal Timeout
	#ifdef DEBUGSYS
			if(sysConfig.traceflag&(1<<TRAFFICD))
				printf("[TRAFFICD]Send Upstream Cmd %s\n",tcmds[comando.cmd]);
	#endif
			}
			if(res==downstream)
			{
				//also send because you there can be clones....
				xQueueSend( downQ, &comando,( TickType_t ) 0 );//start up-down repeating process

	#ifdef DEBUGSYS
			if(sysConfig.traceflag&(1<<TRAFFICD))
					printf("[TRAFFICD]Send Downstream %d Cmd %s\n",comando.towho,tcmds[comando.cmd]);
	#endif

				if(comando.towho==sysConfig.stationid || comando.towho==EVERYBODY ) //its for us, you are also a standard light
					process_cmd(comando);
			}
		}
	}
		else
			printf("Invalid Centinel\n");
	}
}

bool findNetwork(char *cualnet)
{
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
	    .show_hidden = true
	};

	int err=esp_wifi_scan_start(&scan_config, true);
	if (err!=ESP_OK){
		printf("Error scan %x %x\n",err,ESP_ERR_WIFI_NOT_INIT);
		return ESP_FAIL;
	}
	uint16_t ap_num =10;
	wifi_ap_record_t ap_records[10];
	esp_wifi_scan_get_ap_records(&ap_num, ap_records);
	for(int i = 0; i < ap_num; i++)
	{
		if(strcmp((char*)ap_records[i].ssid,cualnet)==0)
			return ESP_OK;
	}

	return ESP_FAIL;

}
void connectInternet(void *pArg)
{
	wifi_config_t 		configap;

	while(true)
	{
		if(string(sysConfig.ssid[1])!="") //1 is in Controller=MQTT server and in Repeater=Controller
		{
			bool quefue=findNetwork(sysConfig.ssid[1]);
			if(!quefue)
			{
#ifdef DEBUGSYS
				if(sysConfig.traceflag & (1<<WIFID))
					printf("[WIFID]Found %s\n",sysConfig.ssid[1]);
#endif
				memset(&configap,0,sizeof(configap));
				strcpy((char *)configap.sta.ssid , sysConfig.ssid[1]);
				strcpy((char *)configap.sta.password, sysConfig.pass[1]);
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &configap));
				ESP_ERROR_CHECK(esp_wifi_connect());

				vTaskDelete(NULL);
			}
			delay(10000);
		}
	}
}

void initWiFi()
{
	wifi_init_config_t 	cfg=WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t 		configap;
	u8 					mac[6];
	char 				textl[20];

	printf("Starting Server handler\n");
	tcpip_adapter_init();

	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler_Server, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	esp_wifi_set_ps(WIFI_PS_NONE); //otherwise multicast does not work well or at all

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	esp_wifi_get_mac(ESP_IF_WIFI_STA, (uint8_t*)&mac);
	memset(&configap,0,sizeof(configap));
//AP section done
	if(string(sysConfig.ssid[0])!="") //in Controller and Repeater is SSID name
	{
		strcpy((char *)configap.ap.ssid,sysConfig.ssid[0]);
		strcpy((char *)configap.ap.password,sysConfig.pass[0]);
	//	printf("AP Loading [%s][%s]\n",configap.ap.ssid,configap.ap.password);
	}
	else
	{
		sprintf(textl,"LightIoT%02x%02x",mac[4],mac[5]);
		strcpy((char*)configap.ap.ssid,textl);
		strcpy((char*)configap.ap.password,textl);
	}
	configap.ap.ssid_len=strlen((char *)configap.ap.ssid);
	configap.ap.authmode=WIFI_AUTH_WPA_PSK;
	configap.ap.ssid_hidden=false;
	configap.ap.max_connection=14;
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &configap));
	ESP_ERROR_CHECK(esp_wifi_start());
	//STA section below. Due to problems with autoconnect, must do it manually
	xTaskCreate(&connectInternet,"internet",4096,NULL, 5, NULL);									// If we are a Controller

}

void initWiFiRepeater()
{
	wifi_init_config_t 	cfg=WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t 		configap;
	u8 					mac[6];

	printf("Staring Repeater Handler\n");
	// Start by Connecting to Controller. When established, start the AP via the event_handler
	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler_Repeater, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	esp_wifi_set_ps(WIFI_PS_NONE); //otherwise multicast does not work well or at all

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	esp_wifi_get_mac(ESP_IF_WIFI_STA, (uint8_t*)&mac);
	memset(&configap,0,sizeof(configap));
	strcpy((char *)configap.sta.ssid , sysConfig.ssid[1]);
	strcpy((char *)configap.sta.password, sysConfig.pass[1]);
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &configap));
	ESP_ERROR_CHECK(esp_wifi_start());
}


void initWiFiSta()
{
	printf("Starting Client Light\n");
	wifi_init_config_t 	cfg=WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t sta_config;

	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler_client, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	esp_wifi_set_ps(WIFI_PS_NONE); //otherwise multicast does not work well or at all
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	memset(&sta_config,0,sizeof(sta_config));

	if (string(sysConfig.ssid[0])!="")
	{
		strcpy((char*)sta_config.sta.ssid,sysConfig.ssid[0]);
		strcpy((char*)sta_config.sta.password,sysConfig.pass[0]);
		printf("WifiSta ssid %s pass %s\n",sta_config.sta.ssid,sta_config.sta.password);
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
		ESP_ERROR_CHECK(esp_wifi_start());
	}
	else
		printf("Station has no Controller/Repeater configured\n");

	// WiFi led
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
	char textl[40];

	//We do it this way so we can have a single global.h file with EXTERN variables(when not main app)
	// and be able to compile routines in an independent file

	cola = xQueueCreate( 10, sizeof( u16 ) ); //DONE queue
	upQ = xQueueCreate( 20, sizeof( cmd_struct ) ); //Upstream queue
	downQ = xQueueCreate( 20, sizeof( cmd_struct ) ); //Downstream queue

	loginSemaphore= xSemaphoreCreateBinary();

	sonUid=0;

	//blinking stuff
	interval=100;
	howmuch=1000;

	FACTOR=sysConfig.reserved;
	if(FACTOR==0)
		FACTOR=1000;
	FACTOR2=sysConfig.reserved2;
	if(FACTOR2==0)
		FACTOR2=1000;

	// Task handles to NULL
	runHandle		=NULL;
	cycleHandle		=NULL;
	blinkHandle		=NULL;
	mongoHandle		=NULL;
	mdnsHandle		=NULL;
	mqttHandle		=NULL;
	rxHandle		=NULL;
	loginHandle		=NULL;

	rxmessagef=false;
	kalive=true;
	globalWalk=false;
	numLogins=0;
	semaphoresOff=false;
	sntpf=false;
	rtcf=false;
	rebootf=false;
	loginf=false;
	repeaterConf=false;

	//clear activity stats
	for (int a=0;a<MAXNODES;a++){
		activeNodes.lastTime[a]=0;
		activeNodes.nodesReported[a]=-1;
	}
	activeNodes.reported=0;
	memset(&burnt,0,sizeof(burnt));

	uint16_t a=esp_random();
	sprintf(textl,"Trf%04x",a);
	idd=string(textl);
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<BOOTD))
		printf("[BOOTD]Id %s\n",textl);
#endif

	if(sysConfig.mode==SERVER)
	{
		settings.host=sysConfig.mqtt;
		settings.port = sysConfig.mqttport;
		settings.client_id=strdup(textl);
		settings.username=sysConfig.mqttUser;
		settings.password=sysConfig.mqttPass;
		settings.event_handle = mqtt_event_handler;
		settings.user_context =sysConfig.mqtt; //name of server
		settings.transport=0?MQTT_TRANSPORT_OVER_SSL:MQTT_TRANSPORT_OVER_TCP;
		settings.buffer_size=2048;
		settings.disable_clean_session=false;
	}

	strcpy(bulbColors[0],"RED");
	strcpy(bulbColors[1],"YLW");
	strcpy(bulbColors[2],"GRN");
	strcpy(bulbColors[3],"LFT");
	strcpy(bulbColors[4],"RGT");
	strcpy(bulbColors[5],"DWK");
	strcpy(bulbColors[6],"WLK");

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

	for (int i=NKEYS/2;i<NKEYS;i++) //Do the - version of trace
	{
		strcpy(lookuptable[i].key,"-");
		strcat(lookuptable[i].key,lookuptable[i-NKEYS/2].key);
		lookuptable[i].val=i*-1;
		lookuptable[i-NKEYS/2].val=i-NKEYS/2;
	}

	// Commands available
	strcpy(tcmds[0],"ACK");
	strcpy(tcmds[1],"NAK");
	strcpy(tcmds[2],"DONE");
	strcpy(tcmds[3],"PING");
	strcpy(tcmds[4],"PONG");
	strcpy(tcmds[5],"QUIET");
	strcpy(tcmds[6],"RESET");
	strcpy(tcmds[7],"NEWID");
	strcpy(tcmds[8],"RUN");
	strcpy(tcmds[9],"OFF");
	strcpy(tcmds[10],"ON");
	strcpy(tcmds[11],"RUALIVE");
	strcpy(tcmds[12],"IMALIVE");
	strcpy(tcmds[13],"KILL");
	strcpy(tcmds[14],"BLINK");
	strcpy(tcmds[15],"LEDS");
	strcpy(tcmds[16],"FIRMW");
	strcpy(tcmds[17],"WALK");
	strcpy(tcmds[18],"EXECW");
	strcpy(tcmds[19],"SENDCLONE");
	strcpy(tcmds[20],"CLONE");
	strcpy(tcmds[21],"LOGIN");
	strcpy(tcmds[22],"ALARM");
	strcpy(tcmds[23],"LOGINACK");

	//kbd Comands text
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
	strcpy(kbdTable[11],"Quiet");
	strcpy(kbdTable[12],"Trace");
	strcpy(kbdTable[13],"Temperature");
	strcpy(kbdTable[14],"Status");
	strcpy(kbdTable[15],"MqttId");
	strcpy(kbdTable[16],"AccessPoint");
	strcpy(kbdTable[17],"Mode");
	strcpy(kbdTable[18],"Ping");
	strcpy(kbdTable[19],"Reset");
	strcpy(kbdTable[20],"NewId");
	strcpy(kbdTable[21],"Statistics");
	strcpy(kbdTable[22],"Zero");
	strcpy(kbdTable[23],"Display");
	strcpy(kbdTable[24],"Settings");
	strcpy(kbdTable[25],"Beat");
	strcpy(kbdTable[26],"StopCycle");
	strcpy(kbdTable[27],"Alive");
	strcpy(kbdTable[28],"Streets");
	strcpy(kbdTable[29],"Help");
	strcpy(kbdTable[30],"Silent");
	strcpy(kbdTable[31],"BulbTest");
	strcpy(kbdTable[32],"Date");
	strcpy(kbdTable[33],"Dumpcore");
	strcpy(kbdTable[34],"Loglevel");

	//Set up Mqtt Variables
	spublishTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/MSG";
	cmdTopic=string(APP)+"/"+string(sysConfig.groupName)+"/"+string(sysConfig.lightName)+"/CMD";

	strcpy(MQTTSERVER,"m15.cloudmqtt.com");

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

	// Section to set Command names and associated code to execute for Webcmds and Mqtt Commands

	strcpy(cmdName[0],"/tf_test");
	theCode[0]=set_test;
	strcpy(cmdName[1],"/tf_onoff");
	theCode[1]=set_OnOff;
	strcpy(cmdName[2],"/tf_blink");
	theCode[2]=set_blink;
	strcpy(cmdName[3],"/tf_reset");
	theCode[3]=set_reset;
	strcpy(cmdName[4],"/tf_run");
	theCode[4]=set_run;
	strcpy(cmdName[5],"/tf_leds");
	theCode[5]=set_leds;
	strcpy(cmdName[6],"/tf_walk");
	theCode[6]=set_walk;
	//Load the Http uri structure and our cmds structures with commands
	for (int a=0;a<MAXCMDS;a++)
	{
		loscmds[a].method=HTTP_GET;
		loscmds[a].user_ctx=(void*)theCode[a];
		loscmds[a].handler=http_get_handler;
		loscmds[a].uri=strdup(cmdName[a]);
		strcpy(cmds[a].comando,cmdName[a]);
		cmds[a].code=theCode[a];
	}

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
	rtcf=true;
	TODAY=1<<algo.dayOfWeek();

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
		printf("[BOOTD]RTC->UNIX yDay %d yDate %s\n",timeinfo.tm_yday,asctime(&timeinfo));
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

int read_blob(nvs_handle theHandle,string name,u8 *donde, int len, u8 *md5start)
{
	unsigned char 	lkey[16];
	size_t 			largo=len;
	int 			que;

	esp_err_t q=nvs_get_blob(theHandle,name.c_str(),donde,&largo);

	if (q !=ESP_OK)
	{
		printf("Read %s error %d\n",name.c_str(),q);
		return ESP_FAIL; //try to recover
	}

	int diff =md5start-donde;

	makeMd5(donde,diff,(void*)lkey);
	que=memcmp(md5start,&lkey,16);

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<BOOTD))
		printf("[BOOTD]%s MD5 %s\n",name.c_str(),que?"Invalid":"Valid");
#endif
	return que;
}

int read_flash()
{
		esp_err_t q ;
		q=read_blob(nvshandle, "config",(u8*)&sysConfig,sizeof(sysConfig),(u8*)&sysConfig.md5);
		return q;
}

int read_flash_seq()
{
		esp_err_t q ;
		q=read_blob(seqhandle, "seq",(u8*)&sysSequence,sizeof(sysSequence),(u8*)&sysSequence.md5);
		return q;
}

int read_flash_cycles()
{
		esp_err_t q ;
		q=read_blob(seqhandle, "cycles",(u8*)&allCycles,sizeof(allCycles),(u8*)&allCycles.md5);
		return q;
}

int read_flash_lights()
{
		esp_err_t q ;
		q=read_blob(lighthandle, "lights",(u8*)&sysLights,sizeof(sysLights),(u8*)&sysLights.md5);
		return q;
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

bool loadScheduler()
{
	nextSchedule=0;
	u32 faltan=86400; //secs in a day. Should be Zero when finished
	time_t now;
	time(&now);
	printf("Today %d\n",TODAY);
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
	u16 soyYo=100; //timeout signal
	printf("Timer timeout\n");
	if(xTimer==doneTimer)
		xQueueSend( cola, &soyYo,( TickType_t ) 0 ); //use a high number to signal Timeout
}

void cycleManager(void * pArg)
{
	char theNodes[50];
	int voy=0,cual,esteCycle;
	node_struct intersections;
	char textl[20];
	time_t now;
	u16 soyYo; //Street Number like 0,1,2. Should not skip one, ex, 0,2,3
	u32 st,ulCount,fueron;

	cual=(int)pArg; //cycle to use
	esteCycle=sysSequence.sequences[cual].cycleId;

	while(!rxtxf)
	{
		cycleHandle=NULL;
		vTaskDelete(NULL);
	}

#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<TRAFFICD))
		printf("[TRAFFICD]Schedule %d Cycle %d\n",cual,esteCycle);
#endif

	strcpy(theNodes,allCycles.nodeSeq[esteCycle]);
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

		if(displayf)
		{
			drawBars();
			if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				gCycleTime=intersections.timeval[voy];
				sprintf(textl,"   %3ds   ",gCycleTime);
				eraseMainScreen();
				drawString(64, 20, string(sysConfig.calles[intersections.nodeid[voy]]),24, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
				drawString(90, 0, textl, 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				display.drawLine(0,18,127,18);
				display.drawLine(0,50,127,50);
				xSemaphoreGive(I2CSem);
			}
		}
		globalNode=intersections.nodeid[voy];
		globalDuration=intersections.timeval[voy]*FACTOR;

		time(&now);
		internal_stats.schedule_changes++;
		internal_stats.started[esteCycle][intersections.nodeid[voy]]++;
		sendMsg(RUN,intersections.nodeid[voy],intersections.timeval[voy],0,(char*)&now,sizeof(now)); //Send date/time as server
		if(intersections.timeval[voy]<5)
			printf("TIMER Fault %d\n",intersections.timeval[voy]);

		vTimerSetTimerID( doneTimer, ( void * ) 0 ); //clear time out signal from timer
		xTimerGenericCommand(doneTimer,tmrCOMMAND_CHANGE_PERIOD,(intersections.timeval[voy]+2)*FACTOR,0,0);//MUST wait for done so 2 secs more and no timeout

		st=millis();

		while(true)
		{
			if( xQueueReceive( cola, &soyYo, portMAX_DELAY )) //two reasons, a Done CMd or a TimeOut
			{
				if(soyYo==intersections.nodeid[voy]) //Its supposed to be for the current Street of the Cycle
				{
					gCycleTime=-1;

//						ulCount = ( uint32_t ) pvTimerGetTimerID( doneTimer );
//						if(ulCount)
//						{
//	#ifdef DEBUGSYS
//							if(sysConfig.traceflag & (1<<TRAFFICD))
//								printf("[TRAFFICD]DONE timeout %d\n",ulCount);
//	#endif
//							sendMsg(KILL,intersections.nodeid[voy],0,0,NULL,0); //if necessary
//							//Log timeout, send warning if x times,etc
//						}
				//	}
						internal_stats.confirmed[esteCycle][soyYo]++;

					fueron=millis()-st;
#ifdef DEBUGSYS
					if(sysConfig.traceflag & (1<<TRAFFICD))
						printf("[TRAFFICD]DONE received %d\n",fueron);
#endif
					break; //next in cycle
				}
				else
				{
					if(soyYo>30){
						internal_stats.timeout[esteCycle][intersections.nodeid[voy]]++;
						sendMsg(KILL,intersections.nodeid[voy],0,0,NULL,0); //if necessary
						printf("Timeout for %d. Assumed its done\n",intersections.nodeid[voy]);
						break;
					}
					else
						printf("Talking out of turn %d. Possible configuration problem\n",soyYo);
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

	while(!(timef || rtcf) || !rxtxf) //wait for all Logins and the Timer Flag
		delay(1000);

	if(!loadScheduler())
	{
		printf("Scheduler not enabled.\n");
		vTaskDelete(NULL);
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

	vTaskDelete(NULL);//done
}

void heartBeat(void *pArg)
{
	while(true)
	{
		if(kalive )
			sendMsg(RUALIVE,EVERYBODY,0,0,NULL,0);
		if(sysConfig.keepAlive<10000)
			sysConfig.keepAlive=10000;
		delay(sysConfig.keepAlive);
	}
}

void blink_lights(int lon)
{
	while(1)
	{
		gpio_set_level((gpio_num_t)WIFILED, 1);
		gpio_set_level((gpio_num_t)SENDLED, 1);
		gpio_set_level((gpio_num_t)RXLED, 1);
		gpio_set_level((gpio_num_t)MQTTLED, 1);
			delay(lon);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		gpio_set_level((gpio_num_t)SENDLED, 0);
		gpio_set_level((gpio_num_t)RXLED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 0);
			delay(lon);
	}
}

int recover(string cual, void * donde, int len, int diff)
{
	printf("Recovering %s len %d\n",cual.c_str(),len);
	esp_err_t q ;
	size_t largo=len;

	q=nvs_get_blob(backhandle,cual.c_str(),donde,&largo);

	if (q !=ESP_OK)
	{
		printf("Error read recover %d\n",q);
		return -1;
	}

	unsigned char lkey[16];
	makeMd5(donde,diff,(void*)lkey);
	int err=memcmp(lkey,(donde+diff),16);
	return err;
}

void open_recovery()
{
	backupf=false;
	int err = nvs_open("backup", NVS_READWRITE, &backhandle);
		if(err!=ESP_OK)
		{
			printf("Error opening Backup File\n");
			return;
		}
	backupf=true;
}

void load_config()
{
	int err = nvs_open("config", NVS_READWRITE, &nvshandle);
	if(err!=ESP_OK)
	{
		printf("Error opening NVS File\n");
		blink_lights(100);
	}
	else
	{
		err=read_flash();
		if(err!=0)
		{
			int diff=(u8*)&sysConfig.md5-(u8*)&sysConfig;
			err=recover("config",(void*)&sysConfig,sizeof(sysConfig),diff);
			printf("Recovering Configuration %d\n",err);
			if(err==0)
				write_to_flash(false);
		}
	}
}

void load_others()
{

	int err = nvs_open("seq", NVS_READWRITE, &seqhandle);
	if(err!=ESP_OK)
	{
		printf("Error opening Seq File\n");
		blink_lights(500);
	}
	else
	{
		err=read_flash_seq();
		if(err!=0)
		{
			int diff=(u8*)&sysSequence.md5-(u8*)&sysSequence;
			err=recover("seq",(void*)&sysSequence,sizeof(sysSequence),diff);
			printf("Recovering Sequence %d\n",err);
			if(err==0)
				write_to_flash_seq(false);
		}
		err=read_flash_cycles();
		if(err!=0)
		{
			int diff=(u8*)&allCycles.md5-(u8*)&allCycles;
			err=recover("seq",(void*)&allCycles,sizeof(allCycles),diff);
			printf("Recovering Cycles %d\n",err);
			if(err==0)
				write_to_flash_cycles(false);
		}
	}
}

void load_lights()
{
	int err = nvs_open("lights", NVS_READWRITE, &lighthandle);
	if(err!=ESP_OK)
	{
		printf("Error opening Lights File\n");
		blink_lights(1000);
	}
	else
	{
		err=read_flash_lights();
		if(err!=0)
		{
			int diff=(u8*)&sysLights.md5-(u8*)&sysLights;
			err=recover("lights",(void*)&sysLights,sizeof(sysLights),diff);
			printf("Recovering Lights %d\n",err);
			if(err==0)
				write_to_flash_lights(false);
		}
	}
}


//main
void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    //md5 stuff
    mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
    memset(&md5,0,sizeof(md5));
    mbedtls_md_init(&md5);
    mbedtls_md_setup(&md5, mbedtls_md_info_from_type(md_type), 0);

    open_recovery();
    load_config();
	esp_log_level_set("*", (esp_log_level_t)sysConfig.free); //shut up

	gpio_set_direction((gpio_num_t)0, GPIO_MODE_INPUT);
	printf("3 Secs to erase\n");
	delay(3000);
	reboot= rtc_get_reset_reason(1); //Reset cause for CPU 1

	load_lights();

	if(sysConfig.mode==SERVER)  //Scheduler only in Controller Mode
		load_others();


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

	if(sysConfig.mode==SERVER)
	{
		initI2C();  			// for Screen
		initScreen();			// Screen
		init_temp();		// Temperature sensors
		initRtc();			//RTC CLock
	}

	init_log();				// Log file management
	initPorts();			//Output and Input ports

	gpio_set_level((gpio_num_t)sysLights.defaultLight, 1);

	REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.outbitsPorts);//clear all set bits
	REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.lasLuces[sysLights.numLuces-1].ioports);//set last light state

	keepAlive=sysConfig.keepAlive;//minute
	//Save new boot count and reset code
	sysConfig.bootcount++;
	sysConfig.lastResetCode=reboot;
	sysLights.failed=0;
	write_to_flash_lights(false);
	write_to_flash(true);

	rxtxf=false; //default stop
	memset(&answer,0,sizeof(answer));
	// Start Main Tasks
	if(sysConfig.mode==SERVER)
		xTaskCreate(&timerManager,"dispMgr",10240,NULL, MGOS_TASK_PRIORITY, NULL);				//Manages all display to LCD
	xTaskCreate(&kbd,"kbd",8192,NULL, MGOS_TASK_PRIORITY, NULL);								// User interface while in development. Erased in RELEASE
	xTaskCreate(&logManager,"log",6144,NULL, MGOS_TASK_PRIORITY, NULL);						// Log Manager

	if(sysConfig.mode==CLIENT)
			initWiFiSta();
	if(sysConfig.mode==SERVER)
			initWiFi();
	if(sysConfig.mode==REPEATER)
	    initWiFiRepeater();

	if(sysConfig.mode==SERVER){
		xTaskCreate(&controller,"controller",10240,NULL, 5, NULL);									// If we are a Controller
	}
}
