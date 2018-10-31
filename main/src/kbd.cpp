/*
 * kbd.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */
#include "kbd.h"

extern void show_config(u8 meter, bool full);
extern void load_from_fram(u8 meter);
extern void write_to_flash();
void write_to_fram(u8 meter,bool adding);
extern string makeDateString(time_t t);
extern esp_mqtt_client_config_t settings;
extern uart_port_t uart_num;
//typedef struct { char key[10]; int val; } t_symstruct;
extern t_symstruct 	lookuptable[NKEYS];
void relay(stateType estado);

int keyfromstring(char *key)
{

    int i;
    for (i=0; i < NKEYS; i++) {
        if (strcmp(lookuptable[i].key, key) == 0)
        {
            return lookuptable[i].val;
        }
    }
    return 100;
}

string get_string(uart_port_t uart_num,u8 cual)
{
	uint8_t ch;
	char dijo[20];
	int son=0,len;
	memset(&dijo,0,20);
	while(1)
	{
		len = uart_read_bytes(uart_num, (uint8_t*)&ch, 1,4);
		if(len>0)
		{
			if(ch==cual)
				return string(dijo);

			else
				dijo[son++]=ch;
			if (son>sizeof(dijo)-1)
				son=sizeof(dijo)-1;
		}

		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

void kbd(void *arg) {
	int len;
	uart_port_t uart_num = UART_NUM_0 ;
	uint32_t add;
	char lastcmd=10;
	char data[50];
	string s1,s2;
	time_t t;
	uint16_t errorcode,code1;
	int cualf,whom;
	int *p=0;
	char textl[30];
	char sermod[]="Only in Server Mode\n";
	wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t tcpip_adapter_sta_list;

	uart_config_t uart_config = {
			.baud_rate = 115200,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
	};
	uart_param_config(uart_num, &uart_config);
	uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	esp_err_t err= uart_driver_install(uart_num, UART_FIFO_LEN+10 , 2048, 0, NULL, 0);
	if(err!=ESP_OK)
		printf("Error UART Install %d\n",err);

	while(1)
	{
		len = uart_read_bytes((uart_port_t)uart_num, (uint8_t*)data,2,20/ portTICK_RATE_MS);
		if(len>0)
		{
			if(data[0]==10)
				data[0]=lastcmd;
			lastcmd=data[0];
			switch(data[0])
			{
			case 'C':
			case 'c':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				esp_wifi_ap_get_sta_list(&wifi_sta_list);
				tcpip_adapter_get_sta_list(&wifi_sta_list, &tcpip_adapter_sta_list);
				for (int i=0; i<wifi_sta_list.num; i++)
				    printf("Connected Nodes[%d]->MAC["MACSTR"]-IP{"IPSTR"}\n",i,MAC2STR(wifi_sta_list.sta[i].mac),IP2STR(&tcpip_adapter_sta_list.sta[i].ip));
					break;
			case 'w':
			case 'W':
				printf("Whoami(%d)",aqui.whoami);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1=="")
					break;
				aqui.whoami=atoi(s1.c_str());
				write_to_flash();
				break;

			case 'h':
			case 'H':
        		printf("Kbd Heap %d\n",xPortGetFreeHeapSize());
        		break;
			case 'x':
			case 'X':
				xTaskCreate(&set_FirmUpdateCmd,"dispMgr",10240,NULL, MGOS_TASK_PRIORITY, NULL);
				break;
			case 'L':
					fclose(bitacora);
					bitacora = fopen("/spiflash/log.txt", "w");//truncate to 0 len
					fclose(bitacora);
					break;
			case 'l':
				printf("Log:\n");
				fseek(bitacora,0,SEEK_SET);
				while(1)
				{
					//read date
					add=fread(&t,1,4,bitacora);
					if(add==0)
						break;
					//read code
					add=fread(&errorcode,1,2,bitacora);
					if(add==0)
						break;
					add=fread(&code1,1,2,bitacora);
					if(add==0)
						break;
					printf("Date %s|Code %d|%s|Code1 %d\n",makeDateString(t).c_str(),errorcode,logText[errorcode].c_str(),code1);
				}
				break;
			case 'q':
			case 'Q':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Quiet To Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1=="")
					break;
				whom=atoi(s1.c_str());
				printf("Quiet(%d):",quiet);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
				{
					quiet=atoi(s1.c_str());
					sendMsg(QUIET,whom,quiet,0,NULL,0);
					displayf=quiet;
				}
				break;
			case 'v':
			case 'V':{
				printf("Trace Flags ");
				for (int a=0;a<NKEYS/2;a++)
					if (aqui.traceflag & (1<<a))
						{
							if(a<(NKEYS/2)-1)
								printf("%s-",lookuptable[a].key);
							else
								printf("%s",lookuptable[a].key);
						}
				printf("\nEnter TRACE FLAG:");
				fflush(stdout);

				s1=get_string((uart_port_t)uart_num,10);
				memset(textl,0,10);
				memcpy(textl,s1.c_str(),s1.length());
				for (int a=0;a<s1.length();a++)
					textl[a]=toupper(textl[a]);
				printf("Debug %s\n",textl);
				s1=string(textl);
				if(strcmp(s1.c_str(),"NONE")==0)
				{
					aqui.traceflag=0;
					write_to_flash();
					break;
				}
				if(strcmp(s1.c_str(),"ALL")==0)
				{
					aqui.traceflag=0xFFFF;
					write_to_flash();
					break;
				}
				cualf=keyfromstring((char*)s1.c_str());
				if(cualf==100)
				{
					printf("Invalid Debug Option\n");
					break;
				}
				if(cualf>=0 )
				{
					printf("Debug Key Pos %d %s added\n",cualf,s1.c_str());
					aqui.traceflag |= 1<<cualf;
					write_to_flash();
					break;
				}
				else
				{
					cualf=cualf*-1;
					printf("Debug Key Pos %d %s removed\n",cualf-(NKEYS/2),s1.c_str());
					aqui.traceflag ^= 1<<(cualf-(NKEYS/2));
					write_to_flash();
					break;
				}

				}

			case 'T':{
				printf("Temp Sensor #:");
				fflush(stdout);
				do{
					len = uart_read_bytes((uart_port_t)uart_num, (uint8_t*)data, sizeof(data),20);
				} while(len==0);
				add=atoi(data);
				if (add>=numsensors)
				{
					printf("Out of range[0-%d]\n",numsensors-1);
					break;
				}
				printf("%d\nId=",add);
				for (int a=0;a<8;a++)
					printf("%02x",sensors[add][a]);
				printf(" Temp: %0.1f\n",DS_get_temp(&sensors[add][0]));
				break;}
			case 'f':{
				show_config(0, true) ;
				break;}
			case 'g':{
				aqui.guardOn=!aqui.guardOn;
				printf("Guard is %s\n",aqui.guardOn?"On":"Off");
				gGuard=aqui.guardOn;
				write_to_flash();
				break;}
			case 'G':{
				if(aqui.guardOn)
					gpio_set_level(LASER, 1);
				else
					gpio_set_level(LASER, 0);
				printf("Laser %s\n",aqui.guardOn?"On":"Off");
				break;}
			case 's':{
				aqui.sendMqtt=!aqui.sendMqtt;
				printf("SendMqtt is %s\n",aqui.sendMqtt?"On":"Off");
				write_to_flash();
				break;}
			case 'k':
				printf("Relay Off\n");
				relay(0);
			//	gpio_set_level((gpio_num_t)RELAY, 0);
				break;
			case 'K':
				printf("Relay On\n");
				gpio_set_level((gpio_num_t)RELAY, 1);
				break;
			case 'S':
				printf("Pos:\n");
				s1=get_string((uart_port_t)uart_num,10);
				len=atoi(s1.c_str());
				printf("%d\n",len);
				printf("SSID:\n");
				s1=get_string((uart_port_t)uart_num,10);
				memset((void*)&aqui.ssid[len][0],0,sizeof(aqui.ssid[len]));
				memcpy((void*)&aqui.ssid[len][0],(void*)s1.c_str(),s1.length());//without the newline char
				printf("SSID[%d][%s]\n",len,aqui.ssid[len]);
				printf("Password:\n");
				s1=get_string((uart_port_t)uart_num,10);
				memset((void*)&aqui.pass[len][0],0,sizeof(aqui.pass[len]));
				memcpy((void*)&aqui.pass[len][0],(void*)s1.c_str(),s1.length());//without the newline char
				printf("[%s]\n",aqui.pass[len]);

				curSSID=aqui.lastSSID=0;
				write_to_flash();
				break;
			case 'd':
			case 'D':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Delay To Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1=="")
					break;
				whom=atoi(s1.c_str());
				printf("Delay(%d):",howmuch);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					howmuch=atol(s1.c_str());
				sendMsg(QUIET,whom,howmuch,0,NULL,0);
				break;
			case 'i':
			case 'I':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Interval To Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1=="")
					break;
				whom=atoi(s1.c_str());
				printf("Interval(%d):",interval);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					interval=atol(s1.c_str());
				sendMsg(QUIET,whom,interval,0,NULL,0);
				break;
			case 'm':
			case 'M':
				printf("Mode Server=1 Client=0(%d)",aqui.mode);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					aqui.mode=atol(s1.c_str());
					write_to_flash();
				}
				break;
							case '0': //send a Start Message. ONLY Controller can send this command
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Start To Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sendMsg(START,atoi(s1.c_str()),0,0,NULL,0);
				}
				break;
			case '1':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Stop To Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sendMsg(STOP,atoi(s1.c_str()),0,0,NULL,0);
				}
				break;
			case '2':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Ping To Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					sendMsg(PING,atoi(s1.c_str()),0,0,NULL,0);
				break;

			case '3':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Counter from Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sendMsg(COUNTERS,atoi(s1.c_str()),0,0,NULL,0);
				}
				break;

			case '4':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Reset Counters for Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sendMsg(RESETC,atoi(s1.c_str()),0,0,NULL,0);
				}
				break;

			case '5':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("Reset Unit for Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sendMsg(RESET,atoi(s1.c_str()),0,0,NULL,0);
				}
				break;

			case '6':
				if(!aqui.mode){
					printf(sermod);
					break;//Only server mode
				}
				printf("New Id for Whom:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					printf("Id(can duplicate):");
					fflush(stdout);
					s2=get_string((uart_port_t)uart_num,10);
					if (s2!=""){
						if(atoi(s1.c_str())==255)
						{
							printf("Can not do for Everybody\n");
							break;
						}
					}
					sendMsg(NEWID,atoi(s1.c_str()),atoi(s2.c_str()),0,NULL,0);
				}
				break;
			case '7':
				printf("Tx %d Rx %d\n",salen,entran);
				break;
			case '8':
				salen=entran=0;
				printf("Zero Counters\n");
				break;
			case '9':
				displayf=!displayf;
				printf("Display %s\n",displayf?"On":"Off");
				break;
			case 'z':
				printf("Settings. Host %s Port %d Client %s User %s Pass %s\n",settings.host,settings.port,settings.client_id,settings.username,settings .password );
				break;
			default:
				printf("No cmd\n");
				break;



			}

		}
		vTaskDelay(BLINKT / portTICK_PERIOD_MS);
	}
}


