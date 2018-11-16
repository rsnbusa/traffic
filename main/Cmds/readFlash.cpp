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


void show_config( u8 meter, bool full) // read flash and if HOW display Status message for terminal
{
	char textl[100];
	time_t now = 0;
	string algo;
	struct tm  ts;


		time(&now);
		print_date_time(string("Flash Read Garage"),now );
		if(full)
		{
			printf ("Last Compile %s-%s\n",__DATE__,__TIME__);

			if(sysConfig.mode)
				printf("Temp %.02f\n",DS_get_temp(&sensors[0][0]));
			u32 diffd=now-sysConfig.lastTime;
			u16 horas=diffd/3600;
			u16 min=(diffd-(horas*3600))/60;
			u16 secs=diffd-(horas*3600)-(min*60);
			printf("[Last Boot: %s] [Elapsed %02d:%02d:%02d] [Previous Boot %s] [Count:%d ResetCode:0x%02x]\n",makeDateString(sysConfig.lastTime).c_str(),horas,min,secs,
					makeDateString(sysConfig.preLastTime).c_str(),sysConfig.bootcount,sysConfig.lastResetCode);
			for(int a=0;a<2;a++)
				if(sysConfig.ssid[a][0]!=0)
					printf("[SSID[%d]:%s-%s %s\n",a,sysConfig.ssid[a],sysConfig.pass[a],curSSID==a ?"*":" ");
			printf( "[IP:" IPSTR "] ", IP2STR(&localIp));

			u8 mac[6];
			esp_wifi_get_mac(WIFI_IF_STA, mac);
			sprintf(textl,"[MAC %2x%2x] ",mac[4],mac[5]);
			string mmac=string(textl);
			printf("%s",mmac.c_str());
			mmac="";
			printf("[AP Name:%s] Mongoose%d\n",AP_NameString.c_str(),mongf);
			printf("Meter Name:%s Working:%s\n",sysConfig.lightName,sysConfig.working?"On":"Off");
			printf("MQTT Server:[%s:%d] Connected:%s User:[%s] Passw:[%s]\n",sysConfig.mqtt,sysConfig.mqttport,mqttf?"Yes":"No",sysConfig.mqttUser,sysConfig.mqttPass);
			printf("Cmd Queue:%s\n",cmdTopic.c_str());
			printf("Answer Queue:%s\n",spublishTopic.c_str());
		//	printf("Alert Queue:%s\n",alertTopic.c_str());
			printf("Update Server:%s\n",sysConfig.domain);
			nameStr=string(APP)+".bin";
			printf("[Version OTA-Updater %s] ",sysConfig.actualVersion);
			printf("[Firmware %s @ %s]\n",nameStr.c_str(),makeDateString(sysConfig.lastUpload).c_str());
			printf("Station Id:%d NodeId:%d Clone:%s\n",sysConfig.whoami,sysConfig.nodeid,sysConfig.clone?"Yes":"No");
		}

		printf("[DispMgrTimer %d] Factor %d Leds %d HeartBeat %d\n",sysConfig.DISPTIME,FACTOR,sysConfig.showLeds,kalive);
//Trace Flags
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

		//Connected Users

		if(sonUid>0)
		{
			printf("Connected Users %d\n",sonUid);
			for (int a=0;a<sonUid;a++){
				printf("Uid %s ",montonUid[a].c_str());
				print_date_time(string("LogIn"),uidLogin[a] );
			}
		}

		//Station Stuff

		algo="Ports:";

		for(int a=0;a<sysLights.numLuces;a++)
		{
			if (sysLights.thePorts[a]>=0)
			{
				if(a<sysLights.numLuces-1)
					sprintf(textl,"%d(%s)-",sysLights.thePorts[a],sysLights.theNames[a]);
				else
					sprintf(textl,"%d(%s)",sysLights.thePorts[a],sysLights.theNames[a]);

				algo+=string(textl);
			}
		}
		printf("%s\n",algo.c_str());

			printf("Lights %d Default %d Blink %d\n",sysLights.numLuces,sysLights.defaultLight,sysLights.blinkLight);


// Lights sequence
			for(int a=0;a<sysLights.numLuces;a++)
			{
				printf("LightSeq #%d Ports %s (%s-%s) Time %d secs\n",a,byte_to_binary_porttxt(sysLights.lasLuces[a].ioports).c_str(),
						sysLights.lasLuces[a].opt?"Blk":"On ",sysLights.lasLuces[a].typ?"%":"F",sysLights.lasLuces[a].valor);
			}

		if(sysConfig.mode==1)
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
			time_t local;
			printf("Active Nodes\n");
			for (int a=0;a<20;a++)
			{
				if(activeNodes.nodesReported[a]!=-1)
				{
					local=activeNodes.lastTime[a]-5*3600;
					localtime_r(&local, &ts);
					printf("Node[%d] reported at %s",a,asctime(&ts));
				}
			}

		    tcpip_adapter_sta_list_t tcpip_adapter_sta_list;
			wifi_sta_list_t wifi_sta_list;
			esp_wifi_ap_get_sta_list(&wifi_sta_list);
			tcpip_adapter_get_sta_list(&wifi_sta_list, &tcpip_adapter_sta_list);
			if(wifi_sta_list.num==0)
				printf("NO Stations connected\n");
			else
				printf("%d Connected TLights\n",wifi_sta_list.num);
			for (int i=0; i<wifi_sta_list.num; i++)
				printf("TLight[%d]->MAC[" MACSTR "]-IP{" IPSTR "}\n",i,MAC2STR(wifi_sta_list.sta[i].mac),IP2STR(&tcpip_adapter_sta_list.sta[i].ip));

			printf("\nGeneral Status\n");
			int este=scheduler.seqNum[scheduler.voy];
			ts = *localtime((const time_t*)&sysSequence.sequences[este].startSeq);
			strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
			int cyc=sysSequence.sequences[scheduler.seqNum[scheduler.voy]].cycleId;
			printf("RxTxf %d Timef %d Connected %d Semaphores are %s in Cycle %s of Schedule %s-",rxtxf,timef,totalConnected,semaphoresOff?"Off":"On",parseCycle(allCycles.nodeSeq[cyc]).c_str(),textl);
			ts = *localtime((const time_t*)&sysSequence.sequences[este].stopSeq);
			strftime(textl, sizeof(textl), "%H:%M:%S", &ts);
			printf("%s\n",textl);

			for (int a=0;a<6;a++)
				if(sysConfig.calles[a][0]!=0)
					printf("Street[%d] is %s\n",a,sysConfig.calles[a]);
		}
		sprintf(textl,"with total duration %d in Light %d %d ms\n",cuantoDura,globalLuz,globalLuzDuration);
		printf("Traffic light is %srunning %s",runHandle?"":"not ",runHandle?textl:"\n");
		if(sysConfig.mode)
			printf("Controller is in Street %s duration %d ms\n",sysConfig.calles[globalNode],globalDuration);

}




