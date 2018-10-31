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
//#include "user_config.h"
const char 						*TAG = "TFF";
extern  string makeDateString(time_t t);
extern void postLog(int code,int code1);

void processCmds(void * nc,cJSON * comands);
void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
void displayTimeSequence(int cuantos);

using namespace std;

uart_port_t uart_num = UART_NUM_0 ;
uart_config_t uart_config = {
			.baud_rate = 115200,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
	};

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
		como +=gpio_get_level(cual)?1:-1;
		delay(6);
	}
	if (como<0)
		como=0;
	if(como>1)
		como=1;
	return como;
}

void hardwareError()
{
	stateVM=UNKNOWN;
	printf("Hardware error. Stopping\n");
	if(dispTimer)
		xTimerStop(dispTimer,0);
	while(FOREVER)
	{
		gpio_set_level((gpio_num_t)WIFILED, 1);
		gpio_set_level((gpio_num_t)MQTTLED, 0);

		delay(100);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		gpio_set_level((gpio_num_t)MQTTLED, 1);
		delay(100);
	}
}

void timerError()
{
	int closeb,openb;

	stateVM=TIMERSTATE;
	//	printf("Timer error. Stopping\n");
	if(dispTimer)
		xTimerStop(dispTimer,0);
	while(FOREVER)
	{
		startLed=60;
		delay(200);
		closeb=gpio_get_level(CLOSESW);
		openb=gpio_get_level(OPENSW);
		if(!closeb)
		{
			stateVM=CLOSED;
			startLed=0;
			return;
		}
		if(!openb)
		{
			startLed=0;
			stateVM=OPENED;
			return;
		}
	}
}


void timerCallback( TimerHandle_t xTimer )
{

	if(xTimer==openTimer)
	{
		gpio_isr_handler_remove(LASERSW);
		gpio_set_level(LASER, LASEROFF);
		if(aqui.traceflag & (1<<GEND))
			printf("[GEND]Timer OPEN Expired\n");
		timerError();
	}
	if(xTimer==closeTimer)
	{
		args[2].timinter=1; // Activate LaserManager with option 1 (do not wait for break)
		xSemaphoreGive(lasert.mimutex);
		if(aqui.traceflag & (1<<GEND))
			printf("[GEND]Timer WBREAK Expired\n");
	}
	if(xTimer==dispTimer)
	{
		globalTotalDisp=millis()-globalDisp;
		display.displayOff();
		displayf=false;
		if(aqui.traceflag & (1<<GEND))
			printf("[GEND]Timer DISPLAY Expired\n");
	}

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
			if(aqui.traceflag & (1<<MQTTD))
				printf("[MQTTD]Msg:%s\n",mensa.mensaje);

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

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q=nvs_set_blob(nvshandle,"config",(void*)&aqui,sizeof(aqui));

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

	if (aqui.ssid[0]==0)
		esp_wifi_get_mac(WIFI_IF_AP, mac);
	else
		esp_wifi_get_mac(WIFI_IF_STA, mac);

	sprintf(local,"%02x%02x",mac[4],mac[5]);//last tow bytes of MAC to identify the connection
	string macID=string(local);
	for(int i = 0; i < macID.length(); i++)
		macID[i] = toupper(macID[i]);

	appn=string(aqui.meterName);//meter name will be used as SSID if available else [APP]IoT-XXXX

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
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]Find %s of %d cmds\n",cual.c_str(),MAXCMDS);
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
		if(aqui.traceflag & (1<<CMDD))
			printf("[CMDD]Webcmdrsn %d %s\n",cualf,uri);
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
				else
					if(aqui.traceflag & (1<<CMDD))
						printf("[CMDD]Cmd Not found\n");
			}
		}
	}
}

void sendResponse(void * comm,int msgTipo,string que,int len,int code,bool withHeaders, bool retain)
{
	struct mg_connection *nc=(struct mg_connection*)comm;
	char textl[10];

	if(aqui.traceflag & (1<<PUBSUBD))
		PRINT_MSG("[PUBSUBD]Type %d Sending response %s len=%d\n",msgTipo,que.c_str(),que.length());
	if(msgTipo==1)
	{ // MQTT Response
		esp_mqtt_client_handle_t mcomm=( esp_mqtt_client_handle_t)comm;
		string final;
//		sprintf(textl,"%02d!",code);
		final=que;

		if (!mqttf)
		{
			if(aqui.traceflag & (1<<MQTTD))
				printf("[MQTTD]No mqtt\n");
			return;
		}
		if(withHeaders)
		{
			for (int a=0;a<sonUid;a++)
			{
				spublishTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/"+montonUid[a]+"/MSG";
				if(aqui.traceflag & (1<<PUBSUBD))
					PRINT_MSG("[PUBSUBD]Publish %s Msg %s\n",spublishTopic.c_str(),final.c_str());
				esp_mqtt_client_publish(mcomm, (char*)spublishTopic.c_str(), (char*)final.c_str(),final.length(), 0, 0);
				spublishTopic="";
				delay(200);
			}
		}
		else
		{
				spublishTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/"+uidStr+"/MSG";
				if(aqui.traceflag & (1<<PUBSUBD))
					printf("[PUBSUBD]DirectPublish %s Msg %s\n",spublishTopic.c_str(),final.c_str());
				esp_mqtt_client_publish(mcomm, (char*)spublishTopic.c_str(), (char*)final.c_str(),final.length(), 0, 0);

		}
	}
	else
	{ //Web Response
		if(aqui.traceflag & (1<<WEBD))
			printf("[WEBD]Web response\n");
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


void relay(stateType estado)
{
	if(aqui.traceflag & (1<<GEND))
		printf("[GEND]Door relay %d\n",estado);
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		drawString(RELAYX,RELAYY, "R", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
		gpio_set_level((gpio_num_t)RELAY, 1);
		delay(aqui.relay);
		gpio_set_level((gpio_num_t)RELAY, 0);
		drawString(RELAYX,RELAYY,"  ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
		xSemaphoreGive(I2CSem);
	}
}


void initSensors()
{
	closeQueueSet = xQueueCreateSet( 2 );
	openQueueSet = xQueueCreateSet( 2 );
	if(closeQueueSet==NULL)
		printf("QClosed failed\n");
	if(openQueueSet==NULL)
		printf("QOpen failed\n");
	args[0].pin=OPENSW;
	args[0].mimutex=xSemaphoreCreateBinary();
	args[1].pin=CLOSESW;
	args[1].mimutex=xSemaphoreCreateBinary();
	args[2].pin=LASERSW;
	args[2].mimutex=xSemaphoreCreateBinary();
	lasert.pin=LASERSW;
	lasert.mimutex=args[2].mimutex;

	openTimerSema=xSemaphoreCreateBinary();
	closeTimerSema=xSemaphoreCreateBinary();
	doorLedSema=xSemaphoreCreateBinary();

	cuentaRelay=0;


	gpio_config_t io_conf;
	uint64_t mask=1;  //If we are going to use Pins >=32 needs to shift left more than 32 bits which the compilers associates with a const 1<<x. Stupid
		io_conf.intr_type = GPIO_INTR_DISABLE;
		io_conf.pin_bit_mask = (mask<<CLOSESW|mask<<OPENSW|mask<<DSPIN);
		io_conf.mode = GPIO_MODE_INPUT;
		io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
		io_conf.pull_up_en =GPIO_PULLUP_ENABLE;
		gpio_config(&io_conf);

		io_conf.intr_type = GPIO_INTR_NEGEDGE;
		io_conf.pin_bit_mask = (mask<<LASERSW);
		gpio_config(&io_conf);

		io_conf.intr_type = GPIO_INTR_DISABLE;
		io_conf.pin_bit_mask = (mask<<LASER|mask<<RELAY|mask<<MQTTLED);
		io_conf.mode = GPIO_MODE_OUTPUT;
		io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
		io_conf.pull_up_en =GPIO_PULLUP_DISABLE;
		gpio_config(&io_conf);

		io_conf.pin_bit_mask = (mask<<DOORLED);
		io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
		gpio_config(&io_conf);

		gpio_set_level((gpio_num_t)RELAY, 0);
		gpio_set_level((gpio_num_t)LASER, LASEROFF);
		gpio_set_level((gpio_num_t)DOORLED, 0);

		adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
		esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
	    adc1_config_width(ADC_WIDTH_BIT_12);
		adcchannel=(adc1_channel_t)ADCCHAN;
		adc1_config_channel_atten(adcchannel, ADC_ATTEN_DB_11);

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
	if(aqui.traceflag==(1<<BOOTD))
		printf("Started mongoose\n");
	while (FOREVER)
		mg_mgr_poll(&mgr, 10);

} // mongooseTask

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
			if(aqui.traceflag&(1<<CMDD))
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

			if(aqui.traceflag&(1<<CMDD))
				printf("[CMDD]MDNS time %s\n",textl);
			string s=string(textl);
			ret=mdns_service_add( AP_NameString.c_str(),"_doorIoT", "_tcp", 80,serviceTxtData, 4 );
			if(ret && (aqui.traceflag&(1<<CMDD)))
							printf("Failed add service  %d\n",ret);
			ret=mdns_service_txt_item_set("_doorIoT", "_tcp", "Boot", s.c_str());
			if(ret && (aqui.traceflag&(1<<CMDD)))
							printf("Failed add txt %d\n",ret);
			s="";
		}

	vTaskDelete(NULL);
}

void initialize_sntp(void *args)
{
	 struct timeval tvStart;
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, (char*)"pool.ntp.org");
	sntp_init();
	time_t now = 0;


	int retry = 0;
	const int retry_count = 10;
	setenv("TZ", "EST5", 1); //UTC is 5 hours ahead for Quito
	tzset();

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
	gettimeofday(&tvStart, NULL);
	sntpf=true;
	if(aqui.traceflag&(1<<BOOTD))
		printf("[BOOTD]Internet Time %04d/%02d/%02d %02d:%02d:%02d YDays %d DoW:%d\n",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,
			timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_yday,timeinfo.tm_wday);

	aqui.preLastTime=aqui.lastTime;
	time(&aqui.lastTime);
	write_to_flash();
	timef=1;
	postLog(0,aqui.bootcount);

	if(!mdnsf)
		xTaskCreate(&mdnstask, "mdns", 4096, NULL, 5, NULL); //Ota Interface Controller
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

	temp=string(aqui.ssid[cual]);
	len=temp.length();

	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(64,42,"               ",10,TEXT_ALIGN_CENTER,DISPLAYIT,REPLACE);
			drawString(64,42,string(aqui.ssid[curSSID]),10,TEXT_ALIGN_CENTER,DISPLAYIT,REPLACE);
			xSemaphoreGive(I2CSem);
		}

	if(aqui.traceflag & (1<<WIFID))
			printf("[WIFID]Try SSID =%s= %d %d\n",temp.c_str(),cual,len);
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	temp=string(aqui.ssid[cual]);
	len=temp.length();
	memcpy((void*)sta_config.sta.ssid,temp.c_str(),len);
	sta_config.sta.ssid[len]=0;
	temp=string(aqui.pass[cual]);
	len=temp.length();
	memcpy((void*)sta_config.sta.password,temp.c_str(),len);
	sta_config.sta.bssid_set=0;
	sta_config.sta.password[len]=0;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	esp_wifi_start(); //if error try again indefinitly

	vTaskDelete(NULL);

}

esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
	system_event_ap_staconnected_t *conap;
	system_event_sta_got_ip_t *ipgave;
	conap=(system_event_ap_staconnected_t*)&event->event_info;
	ipgave=(system_event_sta_got_ip_t*)&event->event_info;
	esp_err_t err;
	string local="Closed",temp;
	system_event_ap_staconnected_t *staconnected;
	if(aqui.traceflag & (1<<WIFID))
		printf("[WIFID]Wifi Handler %d\n",event->event_id);
    mdns_handle_system_event(ctx, event);

	//delay(100);
	switch(event->event_id)
	{
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      	ESP_LOGI(TAG,"Station disconnected %02x:%02x:%02x:%02x:%02x:%02x",conap->mac[0],conap->mac[1],
      			conap->mac[2],conap->mac[3],conap->mac[4],conap->mac[5]);
      	break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
    	ESP_LOGI(TAG,"Assigned AP Ip %d:%d:%d:%d",IP2STR(&ipgave->ip_info.ip));


    	break;

	case SYSTEM_EVENT_STA_GOT_IP:
		gpio_set_level((gpio_num_t)WIFILED, 1);
		connf=true;
		localIp=event->event_info.got_ip.ip_info.ip;
		get_traffic_name();
		if(aqui.traceflag&(1<<BOOTD))
			printf( "[BOOTD]Got IP: %d.%d.%d.%d \n", IP2STR(&event->event_info.got_ip.ip_info.ip));

		if(!mqttf)
		{
			if(aqui.traceflag&(1<<CMDD))
				printf("[CMDD]Connect to mqtt\n");
			xTaskCreate(&mqttmanager,"mgr",10240,NULL,  5, NULL);		// User interface while in development. Erased in RELEASE

			clientCloud = esp_mqtt_client_init(&settings);
			 if(clientCloud)
			    esp_mqtt_client_start(clientCloud);
			 else
				 printf("Fail mqtt initCloud\n");
		}

		if(!mongf)
		{
			setLogo("DoorIoT");
			xTaskCreate(&mongooseTask, "mongooseTask", 10240, NULL, 5, NULL); //  web commands Interface controller
			xTaskCreate(&initialize_sntp, "sntp", 2048, NULL, 3, NULL); //will get date
		}
		break;
	case SYSTEM_EVENT_AP_START:  // Handle the AP start event
	//	tcpip_adapter_ip_info_t ip_info;
	//	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
	//	printf("System not Configured. Use local AP and IP:" IPSTR "\n", IP2STR(&ip_info.ip));
        err=esp_wifi_connect();
        if(err)
        	printf("Error Connect AP %d\n",err);
        else
		if(!mongf)
		{
			xTaskCreate(&mongooseTask, "mongooseTask", 10240, NULL, 5, NULL);
			xTaskCreate(&initialize_sntp, "sntp", 2048, NULL, 3, NULL);
			xTaskCreate(&ConfigSystem, "cfg", 1024, (void*)100, 3, NULL);
		}
		break;

	case SYSTEM_EVENT_STA_START:
		if(aqui.traceflag & (1<<WIFID))
			printf("[WIFID]Connect\n");
		esp_wifi_connect();
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
		staconnected = &event->event_info.sta_connected;
		printf("AP Sta connect MAC %02x:%02x:%02x:%02x:%02x:%02x\n", staconnected->mac[0],staconnected->mac[1],staconnected->mac[2],staconnected->mac[3],
				staconnected->mac[4],staconnected->mac[5]);
		break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
	case SYSTEM_EVENT_ETH_DISCONNECTED:
		connf=false;
		gpio_set_level((gpio_num_t)WIFILED, 0);

		if(aqui.traceflag & (1<<WIFID))
			printf("[WIFID]Reconnect %d\n",curSSID);
		curSSID++;
		if(curSSID>4)
			curSSID=0;

		temp=string(aqui.ssid[curSSID]);

		if(aqui.traceflag & (1<<WIFID))
			printf("[WIFID]Temp[%d]==%s=\n",curSSID,temp.c_str());
		if(temp=="")
			curSSID=0;

		xTaskCreate(&newSSID,"newssid",2048,(void*)curSSID, MGOS_TASK_PRIORITY, NULL);

		break;

	case SYSTEM_EVENT_STA_CONNECTED:
		if(aqui.traceflag & (1<<WIFID))
			printf("[WIFID]Connected SSID[%d]=%s\n",curSSID,aqui.ssid[curSSID]);
		aqui.lastSSID=curSSID;
		write_to_flash();
		gpio_set_level((gpio_num_t)WIFILED, 1);
		break;

	default:
		if(aqui.traceflag & (1<<WIFID))
			printf("[WIFID]default WiFi %d\n",event->event_id);
		break;
	}
	return ESP_OK;
} // wifi_event_handler


void initI2C()
{
	i2cp.sdaport=(gpio_num_t)SDAW;
	i2cp.sclport=(gpio_num_t)SCLW;
	i2cp.i2cport=I2C_NUM_0;
	miI2C.init(i2cp.i2cport,i2cp.sdaport,i2cp.sclport,400000,&I2CSem);//Will reserve a Semaphore for Control
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
        		if(aqui.traceflag & (1<<MQTTD))
        			printf("[MQTTD]Connected %s(%d)\n",(char*)event->user_context,(u32)client);
            break;
        case MQTT_EVENT_DISCONNECTED:
        		if(aqui.traceflag & (1<<MQTTD))
        			printf( "[MQTTD]MQTT_EVENT_DISCONNECTED %s(%d)\n",(char*)event->user_context,(u32)client);
        		gpio_set_level((gpio_num_t)MQTTLED, 0);
        		mqttf=false;
        		mqttCon=0;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            	if(aqui.traceflag & (1<<MQTTD))
            		printf("[MQTTD]Subscribed Cloud\n");
            	mqttf=true;
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
        	if(aqui.traceflag & (1<<MQTTD))
        		printf( "[MQTTD]MQTT_EVENT_UNSUBSCRIBED %s(%d)\n",(char*)event->user_context,(u32)client);
        		  esp_mqtt_client_subscribe(client, cmdTopic.c_str(), 0);
            break;
        case MQTT_EVENT_PUBLISHED:
        	if(aqui.traceflag & (1<<MQTTD))
        		printf( "[MQTTD]MQTT_EVENT_PUBLISHED %s(%d)\n",(char*)event->user_context,(u32)client);
            break;
        case MQTT_EVENT_DATA:
        	if(aqui.traceflag & (1<<MQTTD))
        	{
        		printf("[MQTTD]MSG for %s(%d)\n",(char*)event->user_context,(u32)client);
        		printf("[MQTTD]TOPIC=%.*s\r\n", event->topic_len, event->topic);
        		printf("[MQTTD]DATA=%.*s\r\n", event->data_len, event->data);
        	}
            	datacallback(client,event);
            break;
        case MQTT_EVENT_ERROR:
        	if(aqui.traceflag & (1<<MQTTD))
        		printf("[MQTTD]MQTT_EVENT_ERROR %s(%d)\n",(char*)event->user_context,(u32)client);
        	err=esp_mqtt_client_start(client);
            if(err)
                printf("Error Start Disconnect %s. None functional now!\n",(char*)event->user_context);
            break;
    }
    return ESP_OK;
}


void process_cmd(cmd_struct cual)
{
	if((cual.towho==255) || (cual.towho==aqui.whoami))
	{
		blink(RXLED);
		//rxMsg[cual.cmd]++;
		entran++;
	   switch (cual.cmd)
	   {
		   case START:
			   rxtxf =true;
			   printf("[%d-%d]Start\n",cual.towho,aqui.whoami);
			   xTaskCreate(&mcast_example_task, "mcast_task", 4096, NULL, 5, NULL);
			   break;
		   case STOP:
			   rxtxf =false; //He will kill himself so close and mem be freed
			   printf("[%d-%d]Stop\n",cual.towho,aqui.whoami);
			   break;
		   case ACK:
			   printf("[%d-%d]ACK received from %d->" IPSTR "\n",cual.towho,aqui.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
			   break;
		   case NAK:
			   printf("[%d-%d]NAKC received from %d->" IPSTR "\n",cual.towho,aqui.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
			   break;
		   case DONE:
			   printf("[%d-%d]DONE received from %d->" IPSTR "\n",cual.towho,aqui.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
			   break;
		   case PING:
			   printf("[%d-%d]Ping received from %d->" IPSTR "\n",cual.towho,aqui.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
				sendMsg(PONG,cual.fromwho,0,0,NULL,0);
			   break;
		   case PONG:
			   printf("[%d-%d]Pong from %d->" IPSTR "\n",cual.towho,aqui.whoami,cual.fromwho,IP2STR(&cual.ipstuff.ip));
			   break;
		   case COUNTERS:
			   printf("[%d-%d]Send Counters received\n",cual.towho,aqui.whoami);
				sendMsg(SENDC,cual.fromwho,salen,entran,NULL,0);
			   break;
		   case SENDC:
			   printf("[%d-%d]Counters in from %d Out %d In %d\n",cual.towho,aqui.whoami,cual.fromwho,cual.free1,cual.free2);
			   break;
		   case TEST:
			   printf("[%d-%d]Incoming from %d SeqNum %d Lapse %d\n",cual.towho,aqui.whoami,cual.fromwho,cual.seqnum,cual.lapse);
			   break;
		   case DELAY:
			   printf("[%d-%d]Delay from %d to %d \n",cual.towho,aqui.whoami,cual.fromwho,cual.free1);
			   howmuch=cual.free1;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case QUIET:
			   printf("[%d-%d]Quiet from %d to  %d \n",cual.towho,aqui.whoami,cual.fromwho,cual.free1);
			   displayf=cual.free1;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case INTERVAL:
			   printf("[%d-%d]Interval from %d to %d \n",cual.towho,aqui.whoami,cual.fromwho,cual.free2);
			   interval=cual.free1;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case NEWID:
			   printf("[%d-%d]NewId from %d to %d \n",cual.towho,aqui.whoami,aqui.whoami,cual.free1);
			   aqui.whoami=cual.free1;
			   write_to_flash();
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case RESETC:
			   printf("[%d-%d]ResetCounters from %d \n",cual.towho,aqui.whoami,aqui.whoami);
			   entran=salen=0;
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   break;
		   case RESET:
			   printf("Reset unit after 5 seconds\n");
				sendMsg(ACK,cual.fromwho,0,0,NULL,0);
			   vTaskDelay(5000 / portTICK_PERIOD_MS);
			   esp_restart();
			   break;

		   default:
			   break;
	   }
	}
}

static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq ;
    struct in_addr iaddr;
    memset(&imreq,0,sizeof(imreq));
    memset(&iaddr,0,sizeof(iaddr));
    int err = 0;
    // Configure source interface

    tcpip_adapter_ip_info_t ip_info = { 0 };
    if(aqui.mode!=1)
    	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    else
    	err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP address info. Error 0x%x", err);
        goto err;
    }
    ESP_LOGI(TAG,"MULTIAssigned AP Ip %d:%d:%d:%d",IP2STR(&ip_info.ip));
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

    if (assign_source_if) {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                         sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d %s", errno,strerror(errno));
            goto err;
        }
    }

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
	answer.towho=aquien;
	answer.fromwho=aqui.whoami;
	answer.lapse=millis();
	answer.free1=f1;
	answer.free2=f2;
	answer.seqnum++;
	if(que && (len>0))
		memcpy(&answer.buff,que,len);
	tcpip_adapter_get_ip_info(aqui.mode?TCPIP_ADAPTER_IF_AP:TCPIP_ADAPTER_IF_STA, &answer.ipstuff);
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
		blink(SENDLED);
		close(sock);
}

void rxMultiCast(void *pArg)
{
	int theSock;
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

//		// Get the sender's address as a string
//
//		if (raddr.sin6_family == PF_INET) {
//			inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr.s_addr,
//						raddr_name, sizeof(raddr_name)-1);
//		}
//
//		string algo=string(raddr_name);
//		printf("RX from %s\n",raddr_name);

		if (comando.centinel!=THECENTINEL)
			printf("Invalid centinel\n");
		else
			process_cmd(comando);
		}
  }


static void mcast_example_task(void *pvParameters)
{
    int sock,err;
    static int send_count;
    struct addrinfo hints;
    struct addrinfo *res;
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
          if (aqui.mode!=1)
        	  comando.towho=0;
          else
        	  comando.towho=255;
          comando.fromwho=aqui.whoami;

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

void initWiFi()
{
	uint8_t mac[8];
	char textl[20];
	wifi_init_config_t 				cfg=WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t 					configap;

	tcpip_adapter_init();
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	esp_wifi_set_ps(WIFI_PS_NONE); //otherwise multicast does not work well or at all

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
//	esp_wifi_get_mac(ESP_IF_WIFI_AP, (uint8_t*)&mac);
	memset(&configap,0,sizeof(configap));
	strcpy(configap.sta.ssid , "taller");
	strcpy(configap.sta.password, "csttpstt");
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &configap));
	strcpy(configap.ap.ssid,aqui.ssid[0]);
	strcpy(configap.ap.password,aqui.pass[0]);
	configap.ap.ssid_len=strlen(configap.ap.ssid);
	configap.ap.authmode=WIFI_AUTH_WPA_PSK;
	configap.ap.ssid_hidden=false;
	configap.ap.max_connection=10;
	configap.ap.beacon_interval=100;
	ESP_LOGI(TAG,"AP %s",aqui.ssid[0]);
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &configap));
	ESP_ERROR_CHECK(esp_wifi_start());

}

void initWiFiSta()
{
	wifi_config_t sta_config;
	ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	cfg.event_handler = &esp_event_send;
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	if (string(aqui.ssid[curSSID])!="")
	{
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

		int len;
		string temp=string(aqui.ssid[curSSID]);
		len=temp.length();
		memcpy((void*)sta_config.sta.ssid,temp.c_str(),len);
		sta_config.sta.ssid[len]=0;
		temp=string(aqui.pass[0]);
		len=temp.length();
		memcpy((void*)sta_config.sta.password,temp.c_str(),len);
		sta_config.sta.bssid_set=0;
		sta_config.sta.password[len]=0;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
		temp="";
	}

	ESP_ERROR_CHECK(esp_wifi_start());

	// WiFi led
	gpio_set_direction((gpio_num_t)WIFILED, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)WIFILED, 0);
	vTaskDelete(NULL);


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
		drawString(64,40,string(aqui.ssid[curSSID]),10,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		xSemaphoreGive(I2CSem);
	}
	else
		printf("Failed to InitScreen\n");
}


void initVars()
{
	char textl[30];

	//We do it this way so we can have a single global.h file with EXTERN variables(when not main app)
	// and be able to compile routines in an independent file

	sonUid=0;

	uint16_t a=esp_random();
	sprintf(textl,"Traffic%04d",a);
	idd=string(textl);
	if(aqui.traceflag & (1<<BOOTD))
		printf("[BOOTD]Id %s\n",textl);

	settings.host=aqui.mqtt;
	settings.port = aqui.mqttport;
	settings.client_id=strdup(idd.c_str());
	settings.username=aqui.mqttUser;
	settings.password=aqui.mqttPass;
	settings.event_handle = mqtt_event_handler;
	settings.user_context =aqui.mqtt; //name of server
	settings.transport=0?MQTT_TRANSPORT_OVER_SSL:MQTT_TRANSPORT_OVER_TCP;
	settings.buffer_size=2048;
	settings.disable_clean_session=true;

	strcpy(APP,"TrafficIoT\0");

	strcpy(lookuptable[0].key,"BOOTD");
	strcpy(lookuptable[1].key,"WIFID");
	strcpy(lookuptable[2].key,"MQTTD");
	strcpy(lookuptable[3].key,"PUBSUBD");
	strcpy(lookuptable[4].key,"MONGOOSED");
	strcpy(lookuptable[5].key,"CMDD");
	strcpy(lookuptable[6].key,"WEBD");
	strcpy(lookuptable[7].key,"GEND");
	strcpy(lookuptable[8].key,"LASERD");
	strcpy(lookuptable[9].key,"DOORD");
	strcpy(lookuptable[10].key,"MQTTT");
	strcpy(lookuptable[11].key,"HEAPD");

	strcpy(lookuptable[12].key,"-BOOTD");
	strcpy(lookuptable[13].key,"-WIFID");
	strcpy(lookuptable[14].key,"-MQTTD");
	strcpy(lookuptable[15].key,"-PUBSUBD");
	strcpy(lookuptable[16].key,"-MONGOOSED");
	strcpy(lookuptable[17].key,"-CMDD");
	strcpy(lookuptable[18].key,"-WEBD");
	strcpy(lookuptable[19].key,"-GEND");
	strcpy(lookuptable[20].key,"-LASERD");
	strcpy(lookuptable[21].key,"-DOORD");
	strcpy(lookuptable[22].key,"-MQTTT");
	strcpy(lookuptable[23].key,"-HEAPD");

	for (int a=0;a<NKEYS;a++)
		if(a<(NKEYS/2))
			lookuptable[a].val=a;
		else
			lookuptable[a].val=a*-1;

	//Set up Mqtt Variables
	spublishTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/MSG";
	cmdTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/CMD";

	strcpy(WHOAMI,"rsimpsonbusa@gmail.com");
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

	strcpy((char*)&cmds[0].comando,"/ga_firmware");			cmds[0].code=set_FirmUpdateCmd;			//done...needs testing in a good esp32


	barX[0]=0;
	barX[1]=6;
	barX[2]=12;

	barH[0]=5;
	barH[1]=10;
	barH[2]=15;

	eport = 2525;

	strcpy((char*)WIFIME,"DoorIoT0");
	strcpy((char*)eserver , "mail.smtp2go.com");;
	strcpy(TAGG , "gar");

	displayf=true;
	mongf=false;
	//compile_date[] = __DATE__ " " __TIME__;
	usertime=lastGuard=millis();


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
	guardCount=gMotorCount=0;
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
			if(aqui.traceflag&(1<<BOOTD))
			printf("[BOOTD]Could not open file\n");
			return -1;
		}
		else
			if(aqui.traceflag&(1<<BOOTD))
				printf("[BOOTD]Opened file append\n");
	}
	llogf=true;

	return ESP_OK;
}

void read_flash()
{
		esp_err_t q ;
		int largo=sizeof(aqui);
			q=nvs_get_blob(nvshandle,"config",(void*)&aqui,&largo);

		if (q !=ESP_OK)
			printf("Error read %d\n",q);
}

void init_temp()
{
	//Temp sensors
	numsensors=DS_init(DSPIN,bit12,&sensors[0][0]);
	if(numsensors==0)
		numsensors=DS_init(DSPIN,bit12,&sensors[0][0]); //try again

	if(aqui.traceflag & (1<<BOOTD))
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
}

void virtualMachine(void* arg)
{
	// switches. ON is NO CONTACT and OFF is contact
 while(true)
		 delay(1000);
}


void heapWD(void *pArg)
{
	while(FOREVER)
	{
		delay(60000); //every minute
		if(xPortGetFreeHeapSize()<MINHEAP)
		{
			postLog(17,xPortGetFreeHeapSize());
			esp_restart();
		}
	}
}


void app_main(void)
{
//	esp_log_level_set("*", ESP_LOG_ERROR); //shut up
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
		printf("NVS Config open\n");

	tcpip_adapter_init();
	gpio_set_direction((gpio_num_t)0, GPIO_MODE_INPUT);
	delay(3000);
	reboot= rtc_get_reset_reason(1); //Reset cause for CPU 1
	read_flash();
	gGuard=aqui.guardOn;

	traceflag=(debugflags)aqui.traceflag;

	if (aqui.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
		//	if (aqui.centinel!=CENTINEL )
	{
		printf("Read centinel %x",aqui.centinel);
		erase_config();
	}

	curSSID=aqui.lastSSID;
	initVars(); 			// used like this instead of var init to be able to have independent file per routine(s)
	printf("Vars\n");
//	initI2C();  			// for Screen
//	printf("Init2c\n");
//	initScreen();			// Screen
//	printf("Screen\n");
    init_temp();			// Temperature sensors
	printf("VersionEsp32-3.0.3\n");
	init_log();				// Log file management
	printf("log\n");
	stateVM=CLOSED;
	initSensors();
	//Save new boot count and reset code
	aqui.bootcount++;
	aqui.lastResetCode=reboot;
	write_to_flash();

	memset(&answer,0,sizeof(answer));
	// Start Main Tasks

	//xTaskCreate(&displayManager,"dispMgr",10240,NULL, MGOS_TASK_PRIORITY, NULL);				//Manages all display to LCD
	xTaskCreate(&kbd,"kbd",8192,NULL, MGOS_TASK_PRIORITY, NULL);								// User interface while in development. Erased in RELEASE
	xTaskCreate(&logManager,"log",6144,NULL, MGOS_TASK_PRIORITY, NULL);						// Log Manager
	xTaskCreate(&virtualMachine,"vm",10240,NULL, 5, NULL);									// Log Manager
	xTaskCreate(&heapWD,"heapWD",1024,NULL, MGOS_TASK_PRIORITY, NULL);

	ESP_LOGI(TAG, "Node Mode %s", aqui.mode?"Server":"Client");

	if(aqui.mode==0)
	    initWiFiSta();
	else
	    initWiFi();
	xTaskCreate(&rxMultiCast, "rxMulti", 4096, NULL, 4, NULL);

}
