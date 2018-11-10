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
extern void write_to_flash_seq();
extern void write_to_flash_lights();
extern void write_to_flash_cycles();
void write_to_fram(u8 meter,bool adding);
extern string makeDateString(time_t t);
extern esp_mqtt_client_config_t settings;
extern uart_port_t uart_num;
//typedef struct { char key[10]; int val; } t_symstruct;
extern t_symstruct 	lookuptable[NKEYS];
//void relay(stateType estado);

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

bool confirmPort(int cual)
{
	for (int a=0;a<6;a++)
		if(cual==sysLights.thePorts[a])
			return true;
	return false;
}

uint32_t strbitsConfirm(string cual)
{
	string s;
	u8 llevo=0;

	uint32_t res=0;

	  char * pch;
	//  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
	  pch = strtok (cual.c_str(),",");
	  while (pch != NULL)
	  {
		  if(confirmPort(atoi(pch)))
		  {
		  res = res | (1<<(atoi(pch)));
	//    printf ("%s res %d\n",pch,res);
	    pch = strtok (NULL, ",");
		  }
		  else return 0;
	  }
	  return res;
	}


uint16_t getNodeTime(string cual)
{
	string s;
	uint16_t res=0;

	  char * pch;
	//  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
	  pch = strtok (cual.c_str(),"-");
	  while (pch != NULL)
	  {
		 // printf("Node %s-",pch);
		  pch = strtok (NULL,",");
		  res+=atoi(pch);
		//  printf("Time %s\n",pch);
		  pch = strtok (NULL, "-");
	  }
	  return res;
	}

uint32_t strbits(string cual,bool savep)
{
	string s;
	u8 llevo=0;

	uint32_t res=0;

	  char * pch;
	//  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
	  pch = strtok (cual.c_str(),",");
	  while (pch != NULL)
	  {
		  if(atoi(pch)>31)
			  return 0;
		  res = res | (1<<(atoi(pch)));
		  if(savep)
		  {
			  sysLights.thePorts[llevo++]=atoi(pch);
			  sysLights.numLuces=llevo;
		  }
	//    printf ("%s res %d\n",pch,res);
	    pch = strtok (NULL, ",");
	  }
	  return res;
	}

const char *byte_to_binary(uint32_t x)
{
    static char b[33];
    b[0] = '\0';

    uint32_t z=0x80000000;
    u8 c=0;
    for (int a= 31; a >= 0; a --)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
        z=z>>1;
        c++;
		if (c>3 && a>1)
		{
			strcat(b,"-");
			c=0;
		}

    }

    return b;
}

void kbd(void *arg) {
	int len;
	uart_port_t uart_num = UART_NUM_0 ;
	uint32_t add,bits,epoch=0,epoch1=0;
	u8 pos,eltipo,lasOps,nodeid,weekd,nodeseq;
	string algo;
	char lastcmd=10;
	char data[50];
	string s1,s2;
	time_t t;
	uint16_t errorcode,code1;
	int cualf,whom;
	char textl[30];
	char sermod[]="Only in Server Mode\n";
	wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t tcpip_adapter_sta_list;
	struct tm tm;
	char * resul;
	struct tm  ts;

	uart_config_t uart_config = {
			.baud_rate = 115200,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
			.use_ref_tick=false
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
			case 'e':
			case 'E':
				for (int b=0;b<10;b++)
				{
					REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
					vTaskDelay(200/portTICK_PERIOD_MS);
					REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.allbitsPort);//clear all set bits
					vTaskDelay(200/portTICK_PERIOD_MS);
				}
				REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
				break;
			case 'F':
				printf("Factorschedule(%d):",FACTOR);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					FACTOR=atoi(s1.c_str());
				sysConfig.reserved=FACTOR;
				printf("FactorLight(%d):",FACTOR2);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					FACTOR2=atoi(s1.c_str());
				sysConfig.reserved2=FACTOR2;
				write_to_flash();
				break;
			case 'b':
			case 'B':
				printf("Ports(6max)(%s):",byte_to_binary(sysLights.allbitsPort));
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sysLights.numLuces=0;
					for(int a=0;a<6;a++)
						sysLights.thePorts[a]=-1;
					sysLights.allbitsPort=strbits(s1,true);
					write_to_flash_lights();
				}
				break;
			case 'p':
			case 'P':
				algo="";
				for (int a=0;a<sysLights.numLuces;a++)
					if(sysLights.thePorts[a]>=0)
					{
						if(a<sysLights.numLuces-1)
							sprintf(textl,"%d-",sysLights.thePorts[a]);
						else
							sprintf(textl,"%d",sysLights.thePorts[a]);
						algo+=string(textl);
					}

				printf("Default Light(%d)(%s):",sysLights.defaultLight,algo.c_str());
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					sysLights.defaultLight=atoi(s1.c_str());

				printf("Component#:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
				{
					if(atoi(s1.c_str())<sysLights.numLuces)
					{


						pos=atoi(s1.c_str());
						printf("IOPorts(%s)(%s):",byte_to_binary(sysLights.lasLuces[pos].ioports),algo.c_str());
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!=""){
							add=strbitsConfirm(s1);
							if(add>0)
								sysLights.lasLuces[pos].ioports=add;
							else
							{
								printf("Invalid Port...exiting\n");
								break;
							}
						}
						printf("Options(%d):",sysLights.lasLuces[pos].opt);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
							sysLights.lasLuces[pos].opt=atoi(s1.c_str());
						printf("Type(%d):",sysLights.lasLuces[pos].typ);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
							sysLights.lasLuces[pos].typ=atoi(s1.c_str());
						printf("Value(%d):",sysLights.lasLuces[pos].valor);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
							sysLights.lasLuces[pos].valor=atol(s1.c_str());
						write_to_flash_lights();
					}
					}
				break;
			case 'n':
				printf("Cycle#:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if(s1!="")
				{
					pos=atoi(s1.c_str());
					if (pos>39)
					{
						printf("Out of range\n");
						break;
					}
					if(pos+1>allCycles.numcycles)
						allCycles.numcycles=pos+1;

					printf("Cycle(Node-Time)(%d):%s->",allCycles.totalTime[pos],allCycles.nodeSeq[pos]);
					fflush(stdout);
					s1=get_string((uart_port_t)uart_num,10);
					if(s1!="")
					{
						allCycles.totalTime[pos]=getNodeTime(s1);
						strcpy(allCycles.nodeSeq[pos],s1.c_str());
						allCycles.nodeSeq[pos][s1.length()]=0;
						write_to_flash_cycles();
					}
				};
				break;
			case 'o':
				nodeseq=weekd=0;
				printf("Controller Sequence#:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if(s1!="")
				{
					pos=atoi(s1.c_str());
					if (pos<29)
					{
						printf("Cycle[max%d]:",allCycles.numcycles);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
							nodeseq=atoi(s1.c_str());

						printf("WeekDay#(%d):",sysSequence.sequences[pos].weekDay);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
							weekd=atoi(s1.c_str());

						ts = *localtime(&sysSequence.sequences[pos].startSeq);
						strftime(textl, sizeof(textl), "%H:%M:%S", &ts);

						printf("Start Time(HH:MM:SS)(%s):",textl);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
						{
							memset(&tm, 0, sizeof(struct tm));
							algo="2000-1-1 "+s1;
							resul=strptime(algo.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
							if (resul==NULL)
							{
								printf("Failed to convert %s\n",s1.c_str());
								break;
							}
							epoch = mktime(&tm);
						}
						memset(&tm, 0, sizeof(struct tm));
						ts = *localtime(&sysSequence.sequences[pos].stopSeq);
						strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
						printf("Stop Time(HH:MM:SS)(%s):",textl);
						fflush(stdout);
						s1=get_string((uart_port_t)uart_num,10);
						if(s1!="")
						{
							algo="2000-1-1 "+s1;
							resul=strptime(algo.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
							if (resul==NULL)
							{
								printf("Failed to convert %s\n",s1.c_str());
								break;
							}
							epoch1 = mktime(&tm);
							if (epoch>epoch1)
							{
								printf("Stop Time is less than Start Time\n");
								break;
							}
						}

						printf("Seq[%d] Cycle %s Week %d Start %d Stop %d\n",pos,allCycles.nodeSeq[nodeseq],weekd,epoch,epoch1);
						sysSequence.sequences[pos].cycleId=nodeseq;
						sysSequence.sequences[pos].weekDay=weekd;
						sysSequence.sequences[pos].startSeq=epoch;
						sysSequence.sequences[pos].stopSeq=epoch1;
						if(pos+1>sysSequence.numSequences)
							sysSequence.numSequences=pos+1;
						write_to_flash_seq();
					}
				}
			break;
			case 'C':
			case 'c':
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				esp_wifi_ap_get_sta_list(&wifi_sta_list);
				tcpip_adapter_get_sta_list(&wifi_sta_list, &tcpip_adapter_sta_list);
				if(wifi_sta_list.num==0)
					printf("NO Stations connected\n");
				for (int i=0; i<wifi_sta_list.num; i++)
				    printf("Connected Nodes[%d]->MAC["MACSTR"]-IP{"IPSTR"}\n",i,MAC2STR(wifi_sta_list.sta[i].mac),IP2STR(&tcpip_adapter_sta_list.sta[i].ip));
					break;
			case 'w':
			case 'W':

				printf("Station Whoami(%d)",sysConfig.whoami);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					sysConfig.whoami=atoi(s1.c_str());
				printf("Node Id(%d):",sysConfig.nodeid);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					sysConfig.nodeid=atoi(s1.c_str());
				printf("Clone Id(%s):",sysConfig.clone?"Y":"N");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!="")
					sysConfig.clone=atoi(s1.c_str());
				write_to_flash();
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
				if(!sysConfig.mode){
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
					if (sysConfig.traceflag & (1<<a))
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
					sysConfig.traceflag=0;
					write_to_flash();
					break;
				}
				if(strcmp(s1.c_str(),"ALL")==0)
				{
					sysConfig.traceflag=0xFFFF;
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
					sysConfig.traceflag |= 1<<cualf;
					write_to_flash();
					break;
				}
				else
				{
					cualf=cualf*-1;
					printf("Debug Key Pos %d %s removed\n",cualf-(NKEYS/2),s1.c_str());
					sysConfig.traceflag ^= 1<<(cualf-(NKEYS/2));
					write_to_flash();
					break;
				}

				}

			case 'T':{
				printf("Temp: %0.1fC\n",DS_get_temp(&sensors[0][0]));
				break;}
			case 'f':{
				show_config(0, true) ;
				break;}
			case 's':{
				sysConfig.sendMqtt=!sysConfig.sendMqtt;
				printf("SendMqtt is %s\n",sysConfig.sendMqtt?"On":"Off");
				write_to_flash();
				break;}
			case 'S':
				printf("Pos:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				len=atoi(s1.c_str());
				printf("SSID:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				memset((void*)&sysConfig.ssid[len][0],0,sizeof(sysConfig.ssid[len]));
				memcpy((void*)&sysConfig.ssid[len][0],(void*)s1.c_str(),s1.length());//without the newline char
				printf("Password:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				memset((void*)&sysConfig.pass[len][0],0,sizeof(sysConfig.pass[len]));
				memcpy((void*)&sysConfig.pass[len][0],(void*)s1.c_str(),s1.length());//without the newline char

				curSSID=sysConfig.lastSSID=0;
				write_to_flash();
				break;
			case 'd':
			case 'D':
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
				printf("Mode Server=1 Client=0(%d)",sysConfig.mode);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sysConfig.mode=atol(s1.c_str());
					write_to_flash();
				}
				break;
				case '0': //send a Start Message. ONLY Controller can send this command
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
				if(!sysConfig.mode){
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
			case 'Z':
				sendMsg(RUALIVE,EVERYBODY,0,0,NULL,0);
				printf("RUALIVe sent\n");
				break;
			case 'Q':
				rxtxf=!rxtxf;
				printf("Stop %s\n",rxtxf?"Y":"N");
				break;
			case 'M':
				printf("Alive Time(%d):",sysConfig.keepAlive);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sysConfig.keepAlive=atoi(s1.c_str());
					write_to_flash();
				}
				break;
			case 'N':
				printf("Calles Pos:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1=="")
					break;
				len=atoi(s1.c_str());
				printf("Nombre(%s):",sysConfig.calles[len]);
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if(s1=="")
					break;
				strcpy(sysConfig.calles[len],s1.c_str());
				write_to_flash();
				break;
			default:
				printf("No cmd\n");
				break;
			}

		}
		vTaskDelay(BLINKT / portTICK_PERIOD_MS);
	}
}


