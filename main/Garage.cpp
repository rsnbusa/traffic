#include <stdio.h>
#include <stdint.h>
#include "defines.h"
#include "forward.h"
#include "includes.h"
#include "projTypes.h"
#include "string.h"
#include "globals.h"
#include "cmds.h"
#include "framSPI.h"
#include "framDef.h"
#include "freertos/timers.h"
#include "driver/adc.h"
//#include "user_config.h"

extern  string makeDateString(time_t t);
extern void postLog(int code,int code1);

void processCmds(void * nc,cJSON * comands);
void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
void displayTimeSequence(int cuantos);

//wbreak w/o timeout

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

void get_garage_name()
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
		sprintf(textl,"%02d!",code);
		final=string(textl)+que;

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

static void IRAM_ATTR gpio_isr_handler(interrupt_type* arg)
{

	BaseType_t tasker;

	if(!aqui.working)
		return; //Nothing happening

	if(arg->mimutex && arg->pin==LASERSW)
	{
		args[2].timinter=0; //Wake LaserManager with Option 0 (standard laser break)
		xSemaphoreGiveFromISR(arg->mimutex, &tasker );
		if (tasker)
			portYIELD_FROM_ISR();
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

void gOpeningBreak(argumento *args)
{
	guardfopen=false;
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(GUARDX,GUARDY, "    ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			xSemaphoreGive(I2CSem);
		}

	while(FOREVER)
		{
			if(args->mimutex)
			{
				if(xSemaphoreTake(args->mimutex, portMAX_DELAY)) //portMAX_DELAY
				{
					guardfopen=true;
					if(aqui.traceflag & (1<<DOORD))
						printf("[DOORD]Opening Break\n");
					if(xSemaphoreTake(I2CSem, portMAX_DELAY))
						{
							drawString(GUARDX,GUARDY, "G", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
							xSemaphoreGive(I2CSem);
						}
					oBHandle=NULL;
					vTaskDelete(NULL); // ONE signal is enough . Now just die
				}
			}
				delay(100);
		}
}
//test master
void gOpenedBreak(argumento *args)
{

	while(FOREVER)
		{
			if(args->mimutex)
			{
				if(xSemaphoreTake(args->mimutex, portMAX_DELAY)) //portMAX_DELAY
					//count and activate relay
				{
					if(aqui.traceflag & (1<<DOORD))
						printf("[DOORD]Opened Bread\n");
					displayTimeSequence(aqui.wait/2);
					relay(stateVM);
				}
			}
				delay(100);
		}
}

void gClosingBreak(argumento *args)
{

	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(GUARDX,GUARDY, "    ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			xSemaphoreGive(I2CSem);
		}

	while(FOREVER)
		{
			if(args->mimutex)
			{
				if(xSemaphoreTake(args->mimutex, portMAX_DELAY)) //wait for interrupt from laser guard
					//change state and activate relay
				{
					if(xSemaphoreTake(I2CSem, portMAX_DELAY))
						{
							drawString(GUARDX,GUARDY, "g", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
							xSemaphoreGive(I2CSem);
						}
					if(aqui.traceflag & (1<<DOORD))
						printf("[DOORD]ClosingBreak\n");
					guardCount++;
					if(guardCount>2)
					{
						//surely hw error with the guard sensor.
						printf("Guard error\n");
						stateVM=GFAULT;
						gGuard=false;
						cBHandle=NULL;
						vTaskDelete(NULL);
					}
					relay(stateVM);
					guardfopen=false; //Reset flag and wait for break again and or timeout
					vTaskDelete(NULL);
				}
			}
				delay(100);
		}
}

void gCloseBreak(argumento *args)
{ // wait for BREAK or Timeout Break

	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(64, 20,  "                ", 24, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
			drawString(64, 20,  "WBREAK", 24, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
			xSemaphoreGive(I2CSem);
		}
	if(!breakf) //If not sent the WBREAK command, start timer else wait forever for a break
				printf("Failed to start WBREAK\n");
	breakf=false; //just once
	while(FOREVER)
		{
			if(args->mimutex)
			{
				if(xSemaphoreTake(args->mimutex, portMAX_DELAY)) //portMAX_DELAY
					//change state and activate relay
				{
					if(aqui.traceflag & (1<<DOORD))
						printf("[DOORD]Close break activated\n");
					if(!args->timinter) //Not activated by the timer so display the time, else close it
						displayTimeSequence(aqui.wait);
					if(!gpio_get_level(OPENSW)) //Not already closing
						relay(stateVM);
					guardfopen=false; //Reset flag and wait for break again and or timeout
					CBHandle=NULL;
					vTaskDelete(NULL);
				}
			}
			delay(100);
		}
}

 void doorLedTask(void *args)
 {
	 startLed=0;
 	while(FOREVER)
 	{

 	 			while(startLed>0)
 			{
 				gpio_set_level((gpio_num_t)DOORLED, 1);
 				delay(startLed);
 				gpio_set_level((gpio_num_t)DOORLED, 0);
 				delay(startLed);
 			}
 	 	 		delay(1000);
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

	gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

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
	string local="Closed",temp;

	if(aqui.traceflag & (1<<WIFID))
		printf("[WIFID]Wifi Handler %d\n",event->event_id);
    mdns_handle_system_event(ctx, event);

	//delay(100);
	switch(event->event_id)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
		gpio_set_level((gpio_num_t)WIFILED, 1);
		connf=true;
		localIp=event->event_info.got_ip.ip_info.ip;
		get_garage_name();
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
		tcpip_adapter_ip_info_t ip_info;
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
		printf("System not Configured. Use local AP and IP:" IPSTR "\n", IP2STR(&ip_info.ip));

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

	case SYSTEM_EVENT_STA_DISCONNECTED:
	case SYSTEM_EVENT_AP_STADISCONNECTED:
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


void initWiFi(void *pArg)
{
	wifi_config_t sta_config,configap;
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

	else
	{
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
		memcpy(configap.ap.ssid,"DoorIoT\0",10);
		memcpy(configap.ap.password,"csttpstt\0",9);
		configap.ap.ssid[11]=0;
		configap.ap.password[8]=0;
		configap.ap.ssid_len=0;
		configap.ap.authmode=WIFI_AUTH_WPA_PSK;
		configap.ap.ssid_hidden=false;
		configap.ap.max_connection=4;
		configap.ap.beacon_interval=100;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &configap));
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
	sprintf(textl,"garage%04d",a);
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

	strcpy(APP,"DoorIoT\0");

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
	strcpy(MQTTSERVER,"m11.cloudmqtt.com");

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
	strcpy((char*)&cmds[1].comando,"/ga_erase");				cmds[1].code=set_eraseConfig;			//done
	strcpy((char*)cmds[2].comando,"/ga_httpstatus");			cmds[2].code=set_HttpStatus;			//done
	strcpy((char*)cmds[3].comando,"/ga_status");				cmds[3].code=set_statusSend;			//done
	strcpy((char*)cmds[4].comando,"/ga_reset");				cmds[4].code=set_reset;					//done
	strcpy((char*)cmds[5].comando,"/ga_settings");			cmds[5].code=set_settings;				//done
	strcpy((char*)cmds[6].comando,"/ga_resetstats");			cmds[6].code=set_resetstats;				//done
	strcpy((char*)cmds[7].comando,"/ga_internal");			cmds[7].code=set_internal;				//done
	strcpy((char*)cmds[8].comando,"/ga_openDoor");			cmds[8].code=set_opendoor;				//done
	strcpy((char*)cmds[9].comando,"/ga_generalap");			cmds[9].code=set_generalap;				//done
	strcpy((char*)cmds[10].comando,"/ga_scan");				cmds[10].code=set_scanCmd;				//done
	strcpy((char*)cmds[11].comando,"/ga_clearlog");			cmds[11].code=set_clearLog;				//done
	strcpy((char*)cmds[12].comando,"/ga_readlog");			cmds[12].code=set_readlog;				//done
	strcpy((char*)cmds[13].comando,"/ga_sleep");				cmds[13].code=set_sleepmode;				//done
	strcpy((char*)cmds[14].comando,"/ga_break");				cmds[14].code=set_waitbreak;				//done
	strcpy((char*)cmds[15].comando,"/ga_session");			cmds[15].code=set_session;				//done

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

	TickType_t tim;
	tim=aqui.openTimeout;
	openTimer=xTimerCreate("OpenTimer",tim /portTICK_PERIOD_MS,pdFALSE,( void * ) 0,&timerCallback);
	if(openTimer==NULL)
		printf("Failed to create Open timer\n");
	tim=aqui.closeTimeout;
	closeTimer=xTimerCreate("CloseTimer",tim /portTICK_PERIOD_MS,pdFALSE,( void * ) 0,&timerCallback);
	if(closeTimer==NULL)
		printf("Failed to create Close timer\n");
	if(aqui.sleepTime>0)
	{
		gsleepTime=tim=aqui.sleepTime;
		dispTimer=xTimerCreate("dispTimer",tim/portTICK_PERIOD_MS,pdFALSE,( void * ) 0,&timerCallback);
	}
	else
		dispTimer=xTimerCreate("dispTimer",0xffffffff /   portTICK_PERIOD_MS,pdFALSE,( void * ) 0,&timerCallback);
	if(dispTimer==NULL)
		printf("Failed to create Display timer\n");
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

void sendState()
{
	char textl[100];
	string webString;
	if(aqui.sendMqtt)
	{
		sprintf(textl,"%d!%d!%d!%d!%d!%s!%d!%d",aqui.opens,aqui.aborted,aqui.stucks,aqui.guards,aqui.countCycles>0?aqui.totalCycles/aqui.countCycles:0,
				makeDateString(aqui.lastOpen).c_str(),aqui.working,stateVM);
		webString=string(textl);
		sendResponse(mqttCon,1,webString,webString.length(),MSTATUS,1,0);
	}
}

void closeToOpening()
{
	if(!displayf){
		display.displayOn();
		displayf=true;
	}

	guardCount=0; //will use guardCount to count how many times it has reached Opened. More than X is probable guard hw error.Stop
	startCycle=millis();

	if( xTimerIsTimerActive( openTimer ) != pdFALSE )
		xTimerStop(openTimer,0);
	if( xTimerStart( openTimer,0) != pdPASS )
		printf("Failed to start timer\n");
	if( xTimerIsTimerActive( dispTimer ) != pdFALSE )
		xTimerStop(dispTimer,0); //stop display timer

	gwait=aqui.wait;

	if(gGuard)
	{
		aqui.waitBreak=1;
		guardfopen=guardf=false;
		lastGRelay=0;
		gpio_set_level(LASER, LASERON);
		delay(200);
		gpio_isr_handler_add(LASERSW, (gpio_isr_t)gpio_isr_handler, &lasert);
		xSemaphoreTake(args[2].mimutex,100); // used to clear a prevoius Action. Sort of debouncing
		xTaskCreate(&gOpeningBreak,"obreak",4096,&args[2],  MGOS_TASK_PRIORITY, &oBHandle);
	}
	stateVM=OPENING;
	sendState();
}

void closingToClose()
{

	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(GUARDX,GUARDY, "    ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			xSemaphoreGive(I2CSem);
		}

	if( xTimerIsTimerActive( closeTimer ) != pdFALSE )
			xTimerStop(closeTimer,0);

	gMotorMonitor=false;
	if(cBHandle){
		vTaskDelete(cBHandle);
		cBHandle=NULL;
	}
	gpio_isr_handler_remove(LASERSW);
	gpio_set_level(LASER, LASEROFF);
	if(aqui.traceflag & (1<<DOORD))
		printf("[DOORD]ClosingToClose\n");
	guardf=false;
	time(&aqui.lastOpen); //record last Cycle
	aqui.opens++;
	aqui.elapsedCycle=millis()-startCycle;
	postLog(OPENCLOSE,aqui.wait);
	aqui.waitBreak=0;
	cuentaRelay=0;
	write_to_flash();
	if( xTimerIsTimerActive( openTimer ) != pdFALSE )
		xTimerStop(openTimer,0);
	if( xTimerIsTimerActive( dispTimer ) != pdFALSE )
		xTimerStop(dispTimer,0);
	if( xTimerStart( dispTimer,0) != pdPASS )
		printf("Failed to start timer disp\n");
	stateVM=CLOSED;
	guardfopen=false;
	sendState();
}

void openingToClosed()
{
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(GUARDX,GUARDY, "    ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			xSemaphoreGive(I2CSem);
		}

	gMotorMonitor=false;
	uint32_t son=millis()-startCycle;
	if(son>500)
	{
		if( xTimerIsTimerActive( openTimer ) != pdFALSE )
			xTimerStop(openTimer,0);
		if( xTimerIsTimerActive( dispTimer ) != pdFALSE ){
			xTimerStop(dispTimer,0);
		if( xTimerStart( dispTimer,0) != pdPASS )
			printf("Failed to start timer disp\n");
		}
		else{
			globalDisp=millis();
			if( xTimerStart( dispTimer,0) != pdPASS )
						printf("Failed to start timer2 disp\n");
		}
		aqui.aborted++;

		if(gGuard)
		{
			gpio_isr_handler_remove(LASERSW);
			gpio_set_level(LASER, LASEROFF);
			guardfopen=guardf=false;
		}
		if(oBHandle){
			vTaskDelete(oBHandle);
			oBHandle=NULL;
		}
		stateVM=CLOSED;
		sendState();
	}
	else
		if(aqui.traceflag & (1<<DOORD))
			printf("[DOORD]False Abort %d\n",son);

}

void displayTimeSequence(int cuantos)
{
	char textl[30];
	int son=cuantos/1000;
	if(son<5)
		son=5;
	for(int a=0;a<son;a++)
	{
		if(xSemaphoreTake(I2CSem, 300))
		{
			if((gpio_get_level(OPENSW) ) && a>2)
			{
				eraseMainScreen();
				xSemaphoreGive(I2CSem);
				return;
			}
			eraseMainScreen();
			sprintf(textl,"%d",son-a);
			drawString(64,20, string(textl), 24, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
			xSemaphoreGive(I2CSem);
		}
		delay(1000);
	}
	if(xSemaphoreTake(I2CSem, 300))
	{
		eraseMainScreen();
		xSemaphoreGive(I2CSem);
	}
}

void openingToOpened()
{
	if( xTimerIsTimerActive( openTimer ) != pdFALSE )
		xTimerStop(openTimer,0);
	//	if( xTimerIsTimerActive( dispTimer ) != pdFALSE )
	//		xTimerStop(dispTimer,0);
	//	if( xTimerStart( dispTimer,0) != pdPASS )
	//		printf("Failed to start timer disp\n");

	stateVM=OPENED;
	sendState();
	if(oBHandle)
	{
		vTaskDelete(oBHandle);
		oBHandle=NULL;
	}

//	if(aqui.waitBreak && gGuard)
		if(gGuard)
	{
		if(aqui.traceflag & (1<<DOORD))
			printf("[DOORD]Set opened and break\n");
			if( xTimerIsTimerActive( openTimer ) != pdFALSE )
				xTimerStop(openTimer,0);
		if(guardfopen)  //Break during the opening phase, do not wait for the wbreak
			{
				guardfopen=false;
				displayTimeSequence(aqui.wait/2);
				if(!gpio_get_level(OPENSW)) //Not already closing
					relay(stateVM);
			}
		else //Wait for the wbreak
			xTaskCreate(&gCloseBreak,"cbreak",4096,&args[2],MGOS_TASK_PRIORITY, &CBHandle); //Close Break manager
	}
	else //NO guard, count without wbreak
		{
			if(aqui.traceflag & (1<<DOORD))
				printf("[DOORD]Close w/o guard activated\n");
			displayTimeSequence(aqui.wait);
			if(!gpio_get_level(OPENSW)) //Not already closing
				relay(stateVM);
		}

}

void openedToClosing()
{
	gMotorCount=0;
	gMotorMonitor=true;
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		drawString(GUARDX,GUARDY, "   ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
		xSemaphoreGive(I2CSem);
	}

	if( xTimerIsTimerActive( closeTimer ) != pdFALSE )
			xTimerStop(closeTimer,0);

	startClosing=millis();
	//start open-close watchdog timer. If it fires, HARDWARE ERROR 1 version
	if( xTimerIsTimerActive( openTimer ) != pdFALSE )
		xTimerStop(openTimer,0);
	if( xTimerStart( openTimer,0) != pdPASS )
		printf("Failed to start timer\n");
	stateVM=CLOSING;
	sendState();
	if(gGuard)
	{
	//	guardCount=0;
		if(aqui.traceflag & (1<<DOORD))
			printf("[DOORD]Opened to Closing\n");
		gpio_set_level(LASER, LASERON);
		lastGRelay=0;
		xSemaphoreTake(args[2].mimutex,100); // used to clear a prevoius Action. Sort of debouncing
		xTaskCreate(&gClosingBreak,"cbreak",4096,&args[2],MGOS_TASK_PRIORITY, &cBHandle); //Close Break manager
	}
}

void closingToOpened()
{
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		drawString(GUARDX,GUARDY, "   ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
		xSemaphoreGive(I2CSem);
	}

	gMotorMonitor=false;
	if(millis()-startClosing>2000)
	{
		if( xTimerIsTimerActive( openTimer ) != pdFALSE )
			xTimerStop(openTimer,0);
printf("ClosingOpened done\n");
		stateVM=OPENED;
		sendState();
		displayTimeSequence(aqui.wait/1000);
		if(!gpio_get_level(args->pin)) //Not already closing
		{
			printf("closingto opened\n");
			relay(stateVM);
		}
	}

}


void virtualMachine(void* arg)
{
	// switches. ON is NO CONTACT and OFF is contact

	if(aqui.traceflag & (1<<DOORD))
		printf("[DOORD]Starting controller. State %d\n",stateVM);

	//get initial state of switches
	int closeb=gpio_get_level(CLOSESW);
	int openb=gpio_get_level(OPENSW);

	if(closeb==false && openb==false)
		hardwareError(); //both ON is a hardware error and will not be able to function. STOP

	if(!closeb)
		stateVM=CLOSED;
	if(!openb)
		stateVM=OPENED;

	errorf=guardfopen=false;
	aqui.waitBreak=false;
	gpio_set_level(LASER,LASEROFF);

	while(FOREVER) // forever
	{
		//get switches state. getGPIO is in itself 50 ms delay so no need to delay()
		closeb=getGPIO(CLOSESW);
		openb=getGPIO(OPENSW);

		switch(stateVM) //we are driven by a state machine
		{
		case CLOSED:
			//only possible state change is OPENING
			if(closeb)
			{
				//Opening
				if(aqui.traceflag & (1<<DOORD))
					printf("[DOORD]Close Opening\n");
				startLed=300;
				guardfopen=false;
				closeToOpening();
			}
			break;
		case OPENING:
			//Possible states are OPENED (normal cycle) and CLOSED (abort cycle)

			if(!openb)
			{
				if(aqui.traceflag & (1<<DOORD))
					printf("[DOORD]Opened\n");
				startLed=0;
				openingToOpened();
			}
			if(!closeb)
			{
				if(aqui.traceflag & (1<<DOORD))
					printf("[DOORD]Closed\n");
				startLed=0;
				openingToClosed();
			}
			break;
		case OPENED:
			//possible state is Closing only

			if(openb) // Closing
			{
				if(aqui.traceflag & (1<<DOORD))
					printf("[DOORD]Open Closing\n");
				openedToClosing();
				startLed=500;

			}
			break;
		case CLOSING:
			//possible states are CLOSED and OPENED
			if(!closeb)
			{
				if(aqui.traceflag & (1<<DOORD))
					printf("[DOORD]Closed\n");
				if(stateVM==CLOSING) //was closing now CLosed
					startLed=0;
					closingToClose();
			}
			if(!openb) //guard activated and now Opened
			{
				closingToOpened();
				startLed=5;
			}
			break;
		case TIMERSTATE:
			if(aqui.traceflag & (1<<DOORD))
				printf("[DOORD]Timer Guard\n");
			break;
		case GFAULT:
		case VOLTS:
			break;
		default:
			if(!errorf)
			{
				printf("BIG ERROR. UNKNOWN State\n");
				errorf=true;
			}
		}
	}
}

void wakeUp(void *pArg)
{
	while(FOREVER)
	{
		delay(1000);
		if(!displayf)
		{
			if(!gpio_get_level((gpio_num_t)0) )
			{
				if( xTimerIsTimerActive( dispTimer ))
					xTimerStop(dispTimer,0);
					display.displayOn();
					xTimerStart(dispTimer,0);
					displayf=true;
			}
		}
		else
		{
			if(!gpio_get_level((gpio_num_t)0) )
			{
				gsleepTime+=10000;
				if(gsleepTime>200000)
					gsleepTime=30000;
				xTimerDelete(dispTimer,0);
				dispTimer=xTimerCreate("dispTimer",gsleepTime/portTICK_PERIOD_MS,pdFALSE,( void * ) 0,&timerCallback);
				if(dispTimer)
					xTimerStart(dispTimer,0);
			}
		}
	}
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
	esp_log_level_set("*", ESP_LOG_ERROR); //shut up
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
	initI2C();  			// for Screen
	initScreen();			// Screen
    init_temp();			// Temperature sensors
	printf("VersionEsp32-3.0.3\n");
	init_log();				// Log file management
	stateVM=CLOSED;
	initSensors();
	//Save new boot count and reset code
	aqui.bootcount++;
	aqui.lastResetCode=reboot;
	write_to_flash();

	// Start Main Tasks

	xTaskCreate(&displayManager,"dispMgr",10240,NULL, MGOS_TASK_PRIORITY, NULL);				//Manages all display to LCD
	xTaskCreate(&kbd,"kbd",8192,NULL, MGOS_TASK_PRIORITY, NULL);								// User interface while in development. Erased in RELEASE
	xTaskCreate(&logManager,"log",6144,NULL, MGOS_TASK_PRIORITY, NULL);						// Log Manager
	xTaskCreate(&virtualMachine,"vm",10240,NULL, 5, NULL);									// Log Manager
	xTaskCreate(&initWiFi,"main",20000,NULL, MGOS_TASK_PRIORITY, NULL);						// Log Manager
	xTaskCreate(&doorLedTask,"ledmgr",1024,NULL, MGOS_TASK_PRIORITY, NULL);
	xTaskCreate(&wakeUp,"waker",1024,NULL, MGOS_TASK_PRIORITY, NULL);
	xTaskCreate(&heapWD,"heapWD",1024,NULL, MGOS_TASK_PRIORITY, NULL);

	xTimerStart(dispTimer,0);
}
