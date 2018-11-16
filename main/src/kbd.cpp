/*
 * kbd.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */
#include "kbd.h"
using namespace std;

extern void show_config(u8 meter, bool full);
extern void write_to_flash();
extern void write_to_flash_seq();
extern void write_to_flash_lights();
extern void write_to_flash_cycles();
extern string makeDateString(time_t t);
extern void cycleManager(void *pArg);

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

int cmdfromstring(string key)
{

    int i;
    for (i=0; i <KCMDS; i++)
    {
    	string s1=string(kbdTable[i]);
		for (auto & c: s1) c = toupper(c);
    	if(strstr(s1.c_str(),key.c_str())!=NULL){
    //		printf("Found key %s in pos %d = %s\n",key.c_str(),i,s1.c_str());
            return i;
    	}
    }
    return -1;
}


string get_string(uart_port_t uart_num,u8 cual)
{
	uint8_t ch;
	char dijo[50];
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
	uint32_t res=0;

	  char * pch;
	//  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
	  pch = strtok ((char*)cual.c_str(),",");
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
	  pch = strtok ((char*)cual.c_str(),"-");
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
	  pch = strtok ((char*)cual.c_str(),",");
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

string byte_to_binarytxt(uint32_t x)
{

    uint32_t c;
    string resp="";
    int van=0;

    for (int a=0;a<sysLights.numLuces;a++)
    {
    	c=(1<<sysLights.thePorts[a]);
    	if (x&c)
    	{
    		if(van>0)
    			resp+="-";
    		van++;
    		resp+=sysLights.theNames[a];
    	}
    }

    return resp;
}

string byte_to_binary_porttxt(uint32_t x)
{

    uint32_t c;
    string resp="";
    int van=0;
    char textl[10];

    for (int a=0;a<sysLights.numLuces;a++)
    {
    	c=(1<<sysLights.thePorts[a]);
    	if (x&c)
    	{
    		if(van>0)
    			resp+="+";
    		van++;
    		sprintf(textl,"%d(",sysLights.thePorts[a]);
    		resp+=string(textl)+sysLights.theNames[a];
    		resp+=")";
    	}
    }

    return resp;
}

void kbd_blink(uart_port_t uart_num)
{
	for (int b=0;b<10;b++)
		{
			REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
			vTaskDelay(200/portTICK_PERIOD_MS);
			REG_WRITE(GPIO_OUT_W1TS_REG, sysLights.allbitsPort);//clear all set bits
			vTaskDelay(200/portTICK_PERIOD_MS);
		}
		REG_WRITE(GPIO_OUT_W1TC_REG, sysLights.allbitsPort);//clear all set bits
}

void kbd_ports(uart_port_t uart_num)
{
	string s1;
	s1="";
	for (int a=0;a<sysLights.numLuces;a++)
		if(a<sysLights.numLuces-1)
			s1+=string(sysLights.theNames[a])+"-";
		else
			s1+=string(sysLights.theNames[a]);

	printf("Ports(%s)[%s]:",byte_to_binary(sysLights.allbitsPort),s1.c_str());
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10); //format is port#-name,port#-name,etc
	if (s1!=""){
		sysLights.numLuces=0;
		for(int a=0;a<6;a++){
			sysLights.thePorts[a]=-1;
			sysLights.theNames[a][0]=0;
		}
		string s2="";
		int van=0;
		char * pch;
		//  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
		  pch = strtok ((char*)s1.c_str(),"-");
		  while (pch != NULL)
		  {
			//  printf("Port %s-",pch);
			  s2=s2+string(pch)+",";
			  pch = strtok (NULL,",");
			  strcpy(sysLights.theNames[van],pch);
			//  printf("Name %s\n",pch);
			  pch = strtok (NULL, "-");
			  van++;
		  }
		sysLights.numLuces=van;
		sysLights.allbitsPort=strbits(s2,true);
		write_to_flash_lights();
	}
}
void kbd_factor(uart_port_t uart_num)
{
	string s1;
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
	printf("Show Leds(%s):",sysConfig.showLeds?"Y":"N");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	for (auto & c: s1) c = toupper(c);
	if (s1=="Y")
	{
		sendMsg(LEDS,EVERYBODY,1,0,NULL,0);
	}
	write_to_flash();
}
void kbd_light_sequence(uart_port_t uart_num)
{
	string s1,s2,algo;
	char textl[20];
	int pos,add;

	algo="";
	for (int a=0;a<sysLights.numLuces;a++)
		if(sysLights.thePorts[a]>=0)
		{
			if(a<sysLights.numLuces-1)
				sprintf(textl,"%d/%s,",sysLights.thePorts[a],sysLights.theNames[a]);
			else
				sprintf(textl,"%d/%s",sysLights.thePorts[a],sysLights.theNames[a]);
			algo+=string(textl);
		}

	printf("Default Light(%d)(%s):",sysLights.defaultLight,algo.c_str());
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		sysLights.defaultLight=atoi(s1.c_str());

	printf("Blink Light(%d)(%s):",sysLights.blinkLight,algo.c_str());
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		sysLights.blinkLight=atoi(s1.c_str());

	printf("Component#:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
	{
		if(atoi(s1.c_str())<sysLights.numLuces)
		{
			pos=atoi(s1.c_str());
			printf("IOPorts(%s)(%s):",byte_to_binarytxt(sysLights.lasLuces[pos].ioports).c_str(),algo.c_str());
			fflush(stdout);
			s1=get_string((uart_port_t)uart_num,10);
			if(s1!=""){
				add=strbitsConfirm(s1);
				if(add>0)
					sysLights.lasLuces[pos].ioports=add;
				else
				{
					printf("Invalid Port...exiting\n");
					return;
				}
			}
			printf("Options(%d):",sysLights.lasLuces[pos].opt);
			fflush(stdout);
			s1=get_string((uart_port_t)uart_num,10);
			if(s1!="")
				sysLights.lasLuces[pos].opt=atoi(s1.c_str());
			printf("Type(%s)[0=FIX,1=%%]:",sysLights.lasLuces[pos].typ?"%":"F");
			fflush(stdout);
			s1=get_string((uart_port_t)uart_num,10);
			if(s1!="")
				sysLights.lasLuces[pos].typ=atoi(s1.c_str());
			printf("Value(%d):",sysLights.lasLuces[pos].valor);
			fflush(stdout);
			s1=get_string((uart_port_t)uart_num,10);
			if(s1!="")
				sysLights.lasLuces[pos].valor=atol(s1.c_str());
		}
		}
	write_to_flash_lights();

}

void kbd_cycle(uart_port_t uart_num)
{
	string s1;
	int pos;

	printf("Total TLights(%d):",sysConfig.totalLights);
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if(s1!=""){
		sysConfig.totalLights=atoi(s1.c_str());
		write_to_flash();
	}
	printf("Cycle#:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if(s1!="")
	{
		pos=atoi(s1.c_str());
		if (pos>39)
		{
			printf("Out of range\n");
			return;
		}
		if(pos+1>allCycles.numcycles)
			allCycles.numcycles=pos+1;

		printf("Cycle(Node-Time)(%d):%s->",allCycles.totalTime[pos],allCycles.nodeSeq[pos]);
		fflush(stdout);
		s1=get_string((uart_port_t)uart_num,10);
		if(s1!="")
		{
			allCycles.totalTime[pos]=getNodeTime(s1);
			printf("s1 cycle %s\n",s1.c_str());
			strcpy(allCycles.nodeSeq[pos],s1.c_str());
			printf("Cycle %s\n",allCycles.nodeSeq[pos]);
			allCycles.nodeSeq[pos][s1.length()]=0;
			write_to_flash_cycles();
		}
	};
}

void kbd_schedule(uart_port_t uart_num)
{
	string s1,algo;
	int pos;
	int nodeseq=0,weekd=0;
	struct tm  ts;
	char textl[50];
	struct tm tm;
	char * resul;
	time_t epoch, epoch1;

			printf("Controller Sequence#:");
			fflush(stdout);
			s1=get_string((uart_port_t)uart_num,10);
			if(s1!="")
			{
				pos=atoi(s1.c_str());
				if (pos<29)
				{
					printf("Cycle[max %d]:",allCycles.numcycles);
					fflush(stdout);
					s1=get_string((uart_port_t)uart_num,10);
					if(s1!="")
						nodeseq=atoi(s1.c_str());

					printf("WeekDay#(%d):",sysSequence.sequences[pos].weekDay);
					fflush(stdout);
					s1=get_string((uart_port_t)uart_num,10);
					if(s1!="")
						weekd=atoi(s1.c_str());
					else
						weekd=sysSequence.sequences[pos].weekDay;

					ts = *localtime((const time_t*)&sysSequence.sequences[pos].startSeq);
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
							return;
						}
						epoch = mktime(&tm);
					}
					else
						epoch=sysSequence.sequences[pos].startSeq;

					memset(&tm, 0, sizeof(struct tm));
					ts = *localtime((const time_t*)&sysSequence.sequences[pos].stopSeq);
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
							return;
						}
						epoch1 = mktime(&tm);
						if (epoch>epoch1)
						{
							printf("Stop Time is less than Start Time\n");
							return;
						}
					}
					else
						epoch1=sysSequence.sequences[pos].stopSeq;

					printf("Seq[%d] Cycle %s Week %d Start %d Stop %d\n",pos,allCycles.nodeSeq[nodeseq],weekd,(int)epoch,(int)epoch1);
					sysSequence.sequences[pos].cycleId=nodeseq;
					sysSequence.sequences[pos].weekDay=weekd;
					sysSequence.sequences[pos].startSeq=epoch;
					sysSequence.sequences[pos].stopSeq=epoch1;
					if(pos+1>sysSequence.numSequences)
						sysSequence.numSequences=pos+1;
					write_to_flash_seq();
				}
			}

}

void kbd_connected(uart_port_t uart_num)
{
	wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t tcpip_adapter_sta_list;


	esp_wifi_ap_get_sta_list(&wifi_sta_list);
	tcpip_adapter_get_sta_list(&wifi_sta_list, &tcpip_adapter_sta_list);
	if(wifi_sta_list.num==0)
		printf("NO Stations connected\n");
	for (int i=0; i<wifi_sta_list.num; i++)
	    printf("Connected Nodes[%d]->MAC[" MACSTR "]-IP{" IPSTR "}\n",i,MAC2STR(wifi_sta_list.sta[i].mac),IP2STR(&tcpip_adapter_sta_list.sta[i].ip));

}

void kbd_id(uart_port_t uart_num)
{
	string s1;
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
	if(s1!="")
	{
		for (auto & c: s1) c = toupper(c);
		if(s1=="Y")
			sysConfig.clone=1;
		else
			sysConfig.clone=0;
	}
	write_to_flash();

}

void kbd_firmware(uart_port_t uart_num)
{
	string s1;
	firmware_type elfw;
	char textl[50],	temp[20];

	printf("Launch Firmware update:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	for (auto & c: s1) c = toupper(c);
	if (s1=="Y")
	{
		strcpy(elfw.ap,sysConfig.ssid[0]);
		strcpy(elfw.pass,sysConfig.pass[0]);
		memcpy(textl,sysConfig.ssid[1],strlen(sysConfig.ssid[1])+1);
		memcpy(temp,sysConfig.pass[1],strlen(sysConfig.pass[1])+1);
		memcpy(&textl[strlen(sysConfig.ssid[1])+1],temp,strlen(temp)+1);
		int pos=strlen(sysConfig.ssid[1])+strlen(sysConfig.pass[1])+2;
		sendMsg(FWARE,EVERYBODY,strlen(sysConfig.ssid[0])+strlen(sysConfig.pass[0])+2,0,textl,pos);
	}

}

void kbd_clearlog(uart_port_t uart_num)
{
	string s1;

	printf("Clear Log File?");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	for (auto & c: s1) c = toupper(c);
	if (s1=="Y")
	{
		fclose(bitacora);
		bitacora = fopen("/spiflash/log.txt", "w");//truncate to 0 len
		fclose(bitacora);
		printf("Log File cleared\n");
	}
}

void kbd_showlog(uart_port_t uart_num)
{
	string s1;
	int add;
	time_t t;
	u16 errorcode,code1;

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

}


void kbd_quiet(uart_port_t uart_num)
{
	string s1;
	printf("Quiet To Whom:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1=="")
		return;
	int whom=atoi(s1.c_str());
	printf("Quiet(%d):",quiet);
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
	{
		quiet=atoi(s1.c_str());
		sendMsg(QUIET,whom,quiet,0,NULL,0);
		displayf=quiet;
	}

}


void kbd_trace(uart_port_t uart_num)
{
	string s1;

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
	if(s1=="")
		return;
	for (auto & c: s1) c = toupper(c);

//				memset(textl,0,10);
//				memcpy(textl,s1.c_str(),s1.length());
//				for (int a=0;a<s1.length();a++)
//					textl[a]=toupper(textl[a]);
//				printf("Debug %s\n",textl);
//				s1=string(textl);

	if(strcmp(s1.c_str(),"NONE")==0)
	{
		sysConfig.traceflag=0;
		write_to_flash();
		return;
	}
	if(strcmp(s1.c_str(),"ALL")==0)
	{
		sysConfig.traceflag=0xFFFF;
		write_to_flash();
		return;
	}
	int cualf=keyfromstring((char*)s1.c_str());
	if(cualf==100)
	{
		printf("Invalid Debug Option\n");
		return;
	}
	if(cualf>=0 )
	{
		printf("Debug Key Pos %d %s added\n",cualf,s1.c_str());
		sysConfig.traceflag |= 1<<cualf;
		write_to_flash();
		return;
	}
	else
	{
		cualf=cualf*-1;
		printf("Debug Key Pos %d %s removed\n",cualf-(NKEYS/2),s1.c_str());
		sysConfig.traceflag ^= 1<<(cualf-(NKEYS/2));
		write_to_flash();
		return;
	}
}



void kbd_accessPoint(uart_port_t uart_num)
{
	string s1;
	int len;

	printf("Which AP:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if(s1!="")
	{
		len=atoi(s1.c_str());
		printf("SSID(%s):",sysConfig.ssid[len]);
		fflush(stdout);
		s1=get_string((uart_port_t)uart_num,10);
		if(s1!="")
		{
			memset((void*)&sysConfig.ssid[len][0],0,sizeof(sysConfig.ssid[len]));
			memcpy((void*)&sysConfig.ssid[len][0],(void*)s1.c_str(),s1.length());//without the newline char
		}
		printf("Password(%s):",sysConfig.pass[len]);
		fflush(stdout);
		s1=get_string((uart_port_t)uart_num,10);
		if(s1!="")
		{
			memset((void*)&sysConfig.pass[len][0],0,sizeof(sysConfig.pass[len]));
			memcpy((void*)&sysConfig.pass[len][0],(void*)s1.c_str(),s1.length());//without the newline char
		}
		curSSID=sysConfig.lastSSID=0;
		write_to_flash();
	}

}


void kbd_delay(uart_port_t uart_num)
{
	string s1;
	printf("Delay To Whom:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1=="")
		return;
	int whom=atoi(s1.c_str());
	printf("Delay(%d):",howmuch);
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		howmuch=atol(s1.c_str());
	sendMsg(DELAY,whom,howmuch,0,NULL,0);
}

void kbd_interval(uart_port_t uart_num)
{
	string s1;
	printf("Interval To Whom:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1=="")
		return;
	int whom=atoi(s1.c_str());
	printf("Interval(%d):",interval);
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		interval=atol(s1.c_str());
	sendMsg(INTERVAL,whom,interval,0,NULL,0);
}


void kbd_mode(uart_port_t uart_num)
{
	string s1;
	printf("Mode Server=S Client=C(%s)",sysConfig.mode?"S":"C");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	for (auto & c: s1) c = toupper(c);
	if (s1=="S")
	{
		sysConfig.mode=1;
		printf("Light Name(%s)",sysConfig.lightName);
		fflush(stdout);
		s1=get_string((uart_port_t)uart_num,10);
		if (s1!="")
			strcpy(sysConfig.lightName,s1.c_str());
		printf("Group Name(%s)",sysConfig.groupName);
		fflush(stdout);
		s1=get_string((uart_port_t)uart_num,10);
		if (s1!="")
			strcpy(sysConfig.groupName,s1.c_str());
		if(string(sysConfig.groupName)=="")
			strcpy(sysConfig.groupName,sysConfig.lightName);
	}
	else
		if (s1=="C")
			sysConfig.mode=0;
		else
			printf("Invalid Option\n");
	write_to_flash();
}

void kbd_start(uart_port_t uart_num)
{
	string s1;
	printf("Start Who:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!=""){
		sendMsg(START,atoi(s1.c_str()),0,0,NULL,0);
	}
}

void kbd_stop(uart_port_t uart_num)
{
	string s1;
	printf("%s:",rxtxf?"Running-->Stop?":"Stopped-->Run?");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	for (auto & c: s1) c = toupper(c);
	if (s1!="Y")
		return;
	rxtxf=!rxtxf;
	if(rxtxf)
	{
		if(!cycleHandle)
			xTaskCreate(&cycleManager,"cycle",4096,(void*)0, MGOS_TASK_PRIORITY, &cycleHandle);
	}
	else
	{
		vTaskDelete(cycleHandle);
		cycleHandle=NULL;
	}

}

void kbd_newid(uart_port_t uart_num)
{
	string s1,s2;
	printf("New Id for Who:");
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
				return;
			}
		}
		sendMsg(NEWID,atoi(s1.c_str()),atoi(s2.c_str()),0,NULL,0);
	}
}


void kbd_street(uart_port_t uart_num)
{
	string s1;
	printf("Which Street:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1=="")
		return;
	int len=atoi(s1.c_str());
	printf("Name(%s):",sysConfig.calles[len]);
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if(s1=="")
		return;
	strcpy(sysConfig.calles[len],s1.c_str());
	write_to_flash();
}

void kbd_kalive(uart_port_t uart_num)
{
	string s1;

	printf("%s Heartbeat:",kalive?"Stop":"Resume");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	for (auto & c: s1) c = toupper(c);
	if(s1=="Y")
		kalive=!kalive;
}


void kbd_alive(uart_port_t uart_num)
{
	string s1;
	printf("Alive Time(%d):",sysConfig.keepAlive);
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!=""){
		sysConfig.keepAlive=atol(s1.c_str());
		write_to_flash();
	}
}


void kbd_rualive(uart_port_t uart_num)
{
	sendMsg(RUALIVE,EVERYBODY,0,0,NULL,0);
	printf("RUALIVe sent\n");

}
void kbd_reset(uart_port_t uart_num)
{
	string s1;
	printf("Reset Unit:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		sendMsg(RESET,atoi(s1.c_str()),0,0,NULL,0);
}

void kbd_resetcounter(uart_port_t uart_num)
{
	string s1;
	printf("Reset Counters:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		sendMsg(RESETC,atoi(s1.c_str()),0,0,NULL,0);
}


void kbd_counter(uart_port_t uart_num)
{
	string s1;

	printf("Counter from Who:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		sendMsg(COUNTERS,atoi(s1.c_str()),0,0,NULL,0);
}

void kbd_ping(uart_port_t uart_num)
{
	string s1;
	printf("Ping Who:");
	fflush(stdout);
	s1=get_string((uart_port_t)uart_num,10);
	if (s1!="")
		sendMsg(PING,atoi(s1.c_str()),0,0,NULL,0);
}


void kbd(void *arg) {
	uart_port_t uart_num = UART_NUM_0 ;
	string algo;
	int lastcmd=-1;
	string s1,s2,cmds;
	char sermod[]="Only in Server Mode\n";
	char temp[20];
	char local[KCMDS][20];
	int pos=0;


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

	memcpy(local,kbdTable, sizeof(local));

	  for(int i=0;i<=KCMDS;i++)
	    for(int j=i+1;j<=KCMDS;j++){
	      if(strcmp(local[i],local[j])>0){
	        strcpy(temp,local[i]);
	        strcpy(local[i],local[j]);
	        strcpy(local[j],temp);
	     }
	  }

	while(1)
	{
		cmds=get_string((uart_port_t)uart_num,10);
		for (auto & c: cmds) c = toupper(c);
		if(cmds!="")
			lastcmd=cmdfromstring(cmds);
		if(lastcmd>=0)
		{
			switch(lastcmd)
			{
			case BLINKc:
				kbd_blink(uart_num);
				break;

			case FACTORc:
				kbd_factor(uart_num);
				break;

			case PORTSc:
				kbd_ports(uart_num);
				break;

			case LIGHTSc:
				kbd_light_sequence(uart_num);
				break;

			case CYCLEc:
				if(!sysConfig.mode)
				{
				printf(sermod);
					break;//Only server mode
				}
				kbd_cycle(uart_num);
				break;

			case SCHEDULEc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_schedule(uart_num);
				break;

			case CONNECTEDc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_connected(uart_num);
				break;

			case IDc:
				kbd_id(uart_num);
				break;

			case FIRMWAREc:
				if(!sysConfig.mode)
				{
					printf(sermod);
					break;//Only server mode
				}
				kbd_firmware(uart_num);
				break;

			case LOGCLEARc:
				kbd_clearlog(uart_num);
				break;

			case LOGc:
				kbd_showlog(uart_num);
				break;

			case QUIETc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_quiet(uart_num);
				break;

			case TRACEc:
				kbd_trace(uart_num);
				break;

			case TEMPc:
				printf("Current Temp: %0.1fC\n",DS_get_temp(&sensors[0][0]));
				break;

			case STATUSc:
				show_config(0, true) ;
				break;

			case MQTTIDc:
				sysConfig.sendMqtt=!sysConfig.sendMqtt;
				printf("SendMqtt is %s\n",sysConfig.sendMqtt?"On":"Off");
				write_to_flash();
				break;

			case APc:
				kbd_accessPoint(uart_num);
				break;

			case DELAYc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_delay(uart_num);
				break;

			case INTERVALc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_interval(uart_num);
				break;

			case MODEc:
				kbd_mode(uart_num);
				break;

			case STARTc: //send a Start Message. ONLY Controller can send this command
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_start(uart_num);
				break;

			case STOPc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_stop(uart_num);
				break;
				printf("Stop Who:");
				fflush(stdout);
				s1=get_string((uart_port_t)uart_num,10);
				if (s1!=""){
					sendMsg(STOP,atoi(s1.c_str()),0,0,NULL,0);
				}
				break;

			case PINGc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_ping(uart_num);
				break;

			case COUNTERc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_counter(uart_num);
				break;

			case RESETCOUNTc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_resetcounter(uart_num);
				break;

			case RESETc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_reset(uart_num);
				break;

			case NEWIDc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_newid(uart_num);
				break;

			case STATSc:
				printf("Tx %d Rx %d\n",salen,entran);
				break;

			case ZEROc:
				salen=entran=0;
				printf("Zero Counters\n");
				break;

			case DISPLAYc:
				displayf=!displayf;
				printf("Display %s\n",displayf?"On":"Off");
				break;

			case SETTINGSc:
				printf("Host %s Port %d Client %s User %s Pass %s\n",settings.host,settings.port,settings.client_id,settings.username,settings .password );
				break;

			case RUALIVEc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_rualive(uart_num);
				break;

			case STOPCYCLEc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				rxtxf=!rxtxf;
				printf("Stop %s\n",rxtxf?"Y":"N");
				break;

			case ALIVEc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_alive(uart_num);
				break;

			case STREETc:
				if(!sysConfig.mode){
					printf(sermod);
					break;//Only server mode
				}
				kbd_street(uart_num);
				break;

			case HELPc:
				printf("Available commands:");
				for (int a=0;a<KCMDS;a++)
				{
					if(local[a][0]!=pos)
					{
						printf("\n");
						pos=local[a][0];
					}
					else
						printf("-");
					if(a<KCMDS-1)
						printf("%s",local[a]);
					else
						printf("%s",local[a]);
				}
				printf("\n");
				break;

			case KALIVEc:
				kbd_alive(uart_num);
				break;

			default:
				printf("No cmd\n");
				break;
			}
		}
		vTaskDelay(BLINKT / portTICK_PERIOD_MS);
	}
}

