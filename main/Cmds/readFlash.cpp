/*
 * readFlash.cpp
 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */

#include "readFlash.h"
extern const char *byte_to_binary(uint32_t x);
extern string byte_to_binarytxt(uint32_t x);
extern string byte_to_binary_porttxt(uint32_t x);

string makeDateString(time_t t)
{
	char local[40];
	struct tm timeinfo;
	if (t==0)
		time(&t);
	localtime_r(&t, &timeinfo);
	sprintf(local,"%02d/%02d/%02d %02d:%02d:%02d",timeinfo.tm_mday,timeinfo.tm_mon+1,1900+timeinfo.tm_year,timeinfo.tm_hour,
			timeinfo.tm_min,timeinfo.tm_sec);
	return string(local);
}

void print_date_time(string que,time_t t)
{
	struct tm timeinfo;

	localtime_r(&t, &timeinfo);
	printf("[%s] %02d/%02d/%02d %02d:%02d:%02d\n",que.c_str(),timeinfo.tm_mday,timeinfo.tm_mon,1900+timeinfo.tm_year,timeinfo.tm_hour,
			timeinfo.tm_min,timeinfo.tm_sec);
}

string parseCycle(string cual)
{
	string s;
	int cualn;

	s="";

	  char * pch;
	//  printf ("Splitting string \"%s\" into tokens:\n",cual.c_str());
	  pch = strtok ((char*)cual.c_str(),"-");
	  while (pch != NULL)
	  {
		  cualn=atoi(pch);
		  s=s+string(sysConfig.calles[cualn])+" ";
		  pch = strtok (NULL,",");
		  s+=string(pch)+ " ";
		  pch = strtok (NULL, "-");
	  }
	  return s;
	}

string theTime(time_t eltime)
{
	char strftime_buf[60];
	struct tm timeinfo;

	localtime_r(&eltime, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	return(string(strftime_buf));

}

void print_hardware_section(time_t now)
{
	tcpip_adapter_ip_info_t ip_info ;

	printf("========== Hardware ===========\n");
	printf ("Last Compile %s-%s\n",__DATE__,__TIME__);

	if(sysConfig.mode==SERVER && numsensors>0)
		printf("Temp %.02f\n",DS_get_temp(&sensors[0][0]));
	u32 diffd=now-sysConfig.lastTime;
	u16 horas=diffd/3600;
	u16 min=(diffd-(horas*3600))/60;
	u16 secs=diffd-(horas*3600)-(min*60);
	printf("[Last Boot: %s] [Elapsed %02d:%02d:%02d] [Previous Boot %s] [Count:%d ResetCode:0x%02x]\n",makeDateString(sysConfig.lastTime).c_str(),horas,min,secs,
			makeDateString(sysConfig.preLastTime).c_str(),sysConfig.bootcount,sysConfig.lastResetCode);
	for(int a=0;a<2;a++)
		if(sysConfig.ssid[a][0]!=0)
			printf("[SSID[%d]:[%s]-[%s] %s\n",a,sysConfig.ssid[a],sysConfig.pass[a],curSSID==a ?"*":" ");
	if(sysConfig.mode==CLIENT){
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
		printf( "[Client IP:" IPSTR "] ", IP2STR(&ip_info.ip));
	}
	else
	{
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
		printf( "%s->[%s IP:" IPSTR "] ",sysConfig.mode==SERVER?"Server":"Repeater",sysConfig.mode==SERVER?"Internet":"Controller", IP2STR(&ip_info.ip));
		tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
		printf( "[%s IP:" IPSTR "]\n",sysConfig.mode==SERVER?"Client":"ClientRep", IP2STR(&ip_info.ip));
	}

	u8 mac[6];
	esp_wifi_get_mac(WIFI_IF_STA, mac);
	printf("[MAC %2x%2x] ",mac[4],mac[5]);
	printf("[AP Name:%s]\n",AP_NameString.c_str());
	printf("TController Name:%s Working:%s\n",sysConfig.lightName,sysConfig.working?"On":"Off");
	if(sysConfig.mode==SERVER)
	{
		printf("MQTT Server:[%s:%d] Connected:%s User:[%s] Passw:[%s]\n",sysConfig.mqtt,sysConfig.mqttport,mqttf?"Yes":"No",sysConfig.mqttUser,sysConfig.mqttPass);
		printf("Cmd Queue:%s\n",cmdTopic.c_str());
		printf("Answer Queue:%s\n",spublishTopic.c_str());
//	printf("Alert Queue:%s\n",alertTopic.c_str());
		printf("Update Server:%s\n",sysConfig.domain);
		nameStr=string(APP)+".bin";
	}
	printf("[Version OTA-Updater %s] ",sysConfig.actualVersion);
	printf("[Firmware %s @ %s]\n",nameStr.c_str(),makeDateString(sysConfig.lastUpload).c_str());
	if(sysConfig.traceflag>0)
		{
		printf("Trace Flags ");

					for (int a=0;a<NKEYS;a++)
						if (sysConfig.traceflag & (1<<a))
						{
							if(a<NKEYS-1)
								printf("%s-",lookuptable[a].key);
							else
								printf("%s",lookuptable[a].key);
						}
					printf("\n");
		}
	else
		printf("No trace flags\n");
}

void print_id_section()
{
	printf("\n========== Identification ===========\n");

	printf("Station Id:%d NodeId:%d Clone:%s Name %s\n",sysConfig.whoami,sysConfig.nodeid,sysConfig.clone?"Yes":"No",sysConfig.stationName);
	printf("[DispMgrTimer %d] Factor %d Leds %d HeartBeat %d\n",sysConfig.DISPTIME,FACTOR,sysConfig.showLeds,kalive);

}

void print_connection_section(u8 full)
{
	if(sysConfig.mode==SERVER)
	//Connected Users
		if(full==0 || full==3)
		{
			printf("\n========== Connection ===========\n");

			if(sonUid>0)
			{
				printf("Connected Users %d\n",sonUid);
				for (int a=0;a<sonUid;a++){
					printf("Uid %s ",montonUid[a].c_str());
					print_date_time(string("LogIn"),uidLogin[a] );
				}
			}
		}
}

void print_station_section(u8 full)
{
	string algo;
	char textl[100];
	struct tm  ts;

	//Station Stuff
	if(full==0 || full==4 ||full==1)
	{
		printf("\n========== Station ===========\n");

		algo="Ports:";

		for(int a=0;a<sysLights.numLuces;a++)
		{
			if (sysLights.outPorts[a]>=0)
			{
				if(a<sysLights.numLuces-1)
					sprintf(textl,"%d/%d(%s)-",sysLights.outPorts[a],sysLights.inPorts[a],bulbColors[sysLights.theNames[a]]);
				else
					sprintf(textl,"%d/%d(%s)",sysLights.outPorts[a],sysLights.inPorts[a],bulbColors[sysLights.theNames[a]]);

				algo+=string(textl);
			}
		}
		printf("%s\n",algo.c_str());

			printf("Lights %d Default %d Blink %d Walk %s Failed %x\n",sysLights.numLuces,sysLights.defaultLight,sysLights.blinkLight,globalWalk?"Y":"N",sysLights.failed);


// Lights sequence
			for(int a=0;a<sysLights.numLuces;a++)
			{
				printf("LightSeq #%d Ports %s (%d-%s) Time %d secs\n",a,byte_to_binary_porttxt(sysLights.lasLuces[a].ioports).c_str(),
					//	sysLights.lasLuces[a].opt?"Blk":"On ",sysLights.lasLuces[a].typ?"%":"F",sysLights.lasLuces[a].valor);
				sysLights.lasLuces[a].opt,sysLights.lasLuces[a].typ?"%":"F",sysLights.lasLuces[a].valor);
			}

		if(sysConfig.mode==SERVER)
		{
			//Cycles
			printf("Total TLights %d\n",sysConfig.totalLights);

			printf("Num Cycles %4d\n",allCycles.numcycles);
			for (int a=0;a<allCycles.numcycles;a++){
				if(allCycles.totalTime[a]>3)
					sprintf(textl,"%5d",allCycles.totalTime[a]);
				if(allCycles.totalTime[a]==3)
					strcpy(textl,"Blink");
				if(allCycles.totalTime[a]==0)
					strcpy(textl,"  Off");

				printf("Cycle %d Total Time %s ->%s\n",a,textl,parseCycle(allCycles.nodeSeq[a]).c_str());
			}

			//Nodes sequences
			printf("Num Schedules %d\n",sysSequence.numSequences);
			for (int a=0;a<sysSequence.numSequences;a++)
			{
				   ts = *localtime((const time_t*)&sysSequence.sequences[a].startSeq);
					strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
					char diass[8]="-------";
							char diaSemana[8]="SMTWTFS";
							//printf("Dias %x\n",lticket->days);
							for (int b=0;b<7;b++)
							{
								if(sysSequence.sequences[a].weekDay & (1<<b))
									diass[b]=diaSemana[b] ;
							}
							diass[7]=0;
							string nada=string(diass);
					printf("Schedule[%d] (%d)->[%s] %s [%s ",a,sysSequence.sequences[a].cycleId,parseCycle(allCycles.nodeSeq[sysSequence.sequences[a].cycleId]).c_str(),diass,textl);
				   ts = *localtime((const time_t*)&sysSequence.sequences[a].stopSeq);
					strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
					printf("%s]=>%dms\n",textl,sysSequence.sequences[a].stopSeq-sysSequence.sequences[a].startSeq);

			}
		}
	}
}

void print_operation_section(u8 full)
{
	time_t local;
	struct tm  ts;


	if(sysConfig.mode==SERVER)
		if(full==0 || full==3)
		{
			printf("\n========== Operation ===========\n");

			printf("Active Nodes\n");
			for (int a=0;a<20;a++)
			{
				if(activeNodes.nodesReported[a]!=-1)
				{
					local=activeNodes.lastTime[a];
					localtime_r(&local, &ts);
					printf("Node[%d] reported %s at %s",a,activeNodes.dead[a]?"dead":"alive",asctime(&ts));
				}
			}

			tcpip_adapter_sta_list_t tcpip_adapter_sta_list;
			wifi_sta_list_t wifi_sta_list;
			esp_wifi_ap_get_sta_list(&wifi_sta_list);
			tcpip_adapter_get_sta_list(&wifi_sta_list, &tcpip_adapter_sta_list);
			if(wifi_sta_list.num==0)
				printf("NO Stations connected\n");
			else
				printf("%d Connected Devices\n",wifi_sta_list.num);
			for (int i=0; i<wifi_sta_list.num; i++)
				printf("TLight[%d]->MAC[" MACSTR "]-IP{" IPSTR "}\n",i,MAC2STR(wifi_sta_list.sta[i].mac),IP2STR(&tcpip_adapter_sta_list.sta[i].ip));

			printf("Logins %d\n",numLogins);
			printf("Controller Street %d Station %d %s\n",sysConfig.whoami,sysConfig.stationid,sysConfig.stationName);
			for (int a=0;a<numLogins;a++){
				localtime_r((time_t*)&logins[a].timestamp, &ts);
				printf("Street %d Station %d %s @%s",logins[a].nodel,logins[a].stationl,logins[a].namel,asctime(&ts));
			}
		}
}

void print_general_section(u8 full)
{
	string algo;
	char textl[100];
	struct tm  ts;

	if(full==0 || full==5)
	{
		printf("\n========== General ===========\n");

		int este=scheduler.seqNum[scheduler.voy];
		ts = *localtime((const time_t*)&sysSequence.sequences[este].startSeq);
		strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
		int cyc=sysSequence.sequences[scheduler.seqNum[scheduler.voy]].cycleId;
		if(sysConfig.mode==SERVER)
			printf("RxTxf %d Timef %d Connected %d Semaphores are %s in Cycle %s of Schedule %s-",rxtxf,timef,totalConnected,semaphoresOff?"Off":"On",parseCycle(allCycles.nodeSeq[cyc]).c_str(),textl);
		ts = *localtime((const time_t*)&sysSequence.sequences[este].stopSeq);
		strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
		printf("%s\n",textl);

		for (int a=0;a<6;a++)
			if(sysConfig.calles[a][0]!=0)
				printf("Street[%d] is %s\n",a,sysConfig.calles[a]);

		printf("========== Lights =============\n");
		for(int a=0;a<MAXNODES;a++)
		{
			algo="";
			if(burnt[a])  // lights burnt
			{
				int fue=-1;
				for (int c=0;c<numLogins;c++)
				{
					if(logins[c].stationl==a)
					{
						fue=a;
						break;
					}
				}
					printf("Station(%d)%s Burnt Bulbs:",a,logins[fue].namel);
					for (int b=0;b<MAXLIGHTS;b++)
					{ //which ones
						int c=1<<b;
						if (burnt[a] & c)
							printf("%s ",bulbColors[b]);
					}
					printf("\n");
			}
		}
	}
}

void print_stats_section(u8 full)
{
	time_t local;
	struct tm  ts;
	char textl[100];


	if(full==0 || full==2)
	{
		printf("\n========== Statistics ===========\n");
		local=internal_stats.session_start;
		localtime_r(&local, &ts);
		printf("System with %d schedules completed started at %s",internal_stats.schedule_changes,asctime(&ts));

		for (int a=0;a<MAXCYCLES;a++)
		{
			if(internal_stats.started[a][0]>0)
			{
				ts = *localtime((const time_t*)&sysSequence.sequences[a].startSeq);
				strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
				printf("Schedule(%d) From %s to ",a,textl);
				ts = *localtime((const time_t*)&sysSequence.sequences[a].stopSeq);
				strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
				printf("%s\n",textl);

				for (int b=0;b<MAXNODES;b++)
					if(internal_stats.started[a][b]>0)
						printf("Node %d Started %d Confirmed %d Timeout %d\n",b,internal_stats.started[a][b],internal_stats.confirmed[a][b],internal_stats.timeout[a][b]);
			}
		}
	}
}

void print_cycle_section(u8 full)
{
	char textl[100];

	if(full==0 || full==4)
	{
		printf("\n========== Cycles ===========\n");

		sprintf(textl,"with total duration %d in Light %d %d ms\n",cuantoDura,globalLuz,globalLuzDuration);
		printf("Traffic light is %srunning %s",runHandle?"":"not ",runHandle?textl:"\n");
		if(sysConfig.mode==SERVER)
			printf("Controller is in Street %s duration %d ms\n",sysConfig.calles[globalNode],globalDuration);
	}
}

void show_config( u8 full) // read flash and if HOW display Status message for terminal
{
	char textl[50];
	time_t now = 0;
	string algo;

	switch(full)
	{
		case 0:
			strcpy(textl,"Show Full");
			break;
		case 1:
			strcpy(textl,"Show Configuration");
			break;
		case 2:
			strcpy(textl,"Show Statistics");
			break;
		case 3:
			strcpy(textl,"Show Network");
			break;
		case 4:
			strcpy(textl,"Show LightStuff");
			break;
		case 5:
			strcpy(textl,"Show General");
			break;
		default:
			strcpy(textl,"Unknown");
	}

		time(&now);
		print_date_time(string(textl),now );
		if(full==0 || full==2 )
			print_hardware_section(now);

		print_id_section();

//Trace Flags

		print_connection_section(full);
		print_station_section(full);
		print_operation_section(full);
		print_general_section(full);
		print_stats_section(full);
		print_cycle_section(full);

}




