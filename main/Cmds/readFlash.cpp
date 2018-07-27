/*
 * readFlash.cpp
 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */

#include "readFlash.h"

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

void show_config( u8 meter, bool full) // read flash and if HOW display Status message for terminal
{
	char textl[100];
	time_t now = 0;

		time(&now);
		print_date_time(string("Flash Read Garage"),now );
		if(full)
		{
			printf ("Last Compile %s-%s\n",__DATE__,__TIME__);

			if(aqui.centinel==CENTINEL)
				printf("Valid Centinel SNTP:%s Temp %.02f\n",timef?"Y":"N",DS_get_temp(&sensors[0][0]));
			u32 diffd=now-aqui.lastTime;
			u16 horas=diffd/3600;
			u16 min=(diffd-(horas*3600))/60;
			u16 secs=diffd-(horas*3600)-(min*60);
			printf("[Last Boot: %s] [Elapsed %02d:%02d:%02d] [Previous Boot %s] [Count:%d ResetCode:0x%02x]\n",makeDateString(aqui.lastTime).c_str(),horas,min,secs,
					makeDateString(aqui.preLastTime).c_str(),aqui.bootcount,aqui.lastResetCode);
			for(int a=0;a<5;a++)
				if(aqui.ssid[a][0]!=0)
					printf("[SSID[%d]:%s-%s %s\n",a,aqui.ssid[a],aqui.pass[a],curSSID==a ?"*":" ");
			//      printf("Image w:%d h:%d\n",aqui.imw,aqui.imh);
			printf( "[IP:" IPSTR "] ", IP2STR(&localIp));

			u8 mac[6];
			esp_wifi_get_mac(WIFI_IF_STA, mac);
			sprintf(textl,"[MAC %2x%2x] ",mac[4],mac[5]);
			string mmac=string(textl);
			printf("%s",mmac.c_str());
			mmac="";
			printf("[AP Name:%s] Mongoose%d\n",AP_NameString.c_str(),mongf);
			printf("Meter Name:%s Working:%s State:%d\n",aqui.meterName,aqui.working?"On":"Off",stateVM);
			printf("MQTT Server:[%s:%d] Connected:%s User:[%s] Passw:[%s]\n",aqui.mqtt,aqui.mqttport,mqttf?"Yes":"No",aqui.mqttUser,aqui.mqttPass);
			printf("Cmd Queue:%s\n",cmdTopic.c_str());
			printf("Answer Queue:%s\n",spublishTopic.c_str());
			printf("Alert Queue:%s\n",alertTopic.c_str());
			printf("Update Server:%s\n",aqui.domain);
			nameStr=string(APP)+".bin";
			printf("[Version OTA-Updater %s] ",aqui.actualVersion);
			printf("[Firmware %s @ %s]\n",nameStr.c_str(),makeDateString(aqui.lastUpload).c_str());
			if (aqui.ecount>0)
			{
				printf("Emails %d\n",aqui.ecount);
				for (int a=0;a<aqui.ecount;a++)
					if(a<MAXEMAILS) //Guard corruption
						printf("%s @ %s {%s}\n",aqui.emailName[a],aqui.email[a],aqui.except[a]?"EXCEPTION":"ALWAYS");
			}
			//          printf("Accepted Ids %d\n",aqui.ucount);
			//       print_log();
		}

		printf("[DispMgrTimer %d]\n",aqui.DISPTIME);
		printf("Opens %d Stucks %d Guards %d Aborted %d last %s ms:%d Guard:%s Auto:%s waitBreak:%s\n",aqui.opens,aqui.stucks,aqui.guards,aqui.aborted,
				makeDateString(aqui.lastOpen).c_str(),aqui.elapsedCycle,gGuard?"On":"Off",aqui.sendMqtt?"On":"Off",aqui.waitBreak?"On":"Off");

		printf("Timers Relay %d Wait %d Sleep %d openTO %d CloseTO %d Menos %d AvgOpen %d Motor %d\n",aqui.relay,aqui.wait,aqui.sleepTime,
				aqui.openTimeout,aqui.closeTimeout,aqui.menos,aqui.countCycles>0?aqui.totalCycles/aqui.countCycles:0,aqui.motorw);
		printf("Display timer time out %d\n",globalTotalDisp);
		if(aqui.traceflag>0)
			{
			printf("Trace Flags ");

						for (int a=0;a<NKEYS;a++)
							if (aqui.traceflag & (1<<a))
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
		if(sonUid>0)
		{
			printf("Connected Users %d\n",sonUid);
			for (int a=0;a<sonUid;a++){
				printf("Uid %s ",montonUid[a].c_str());
				print_date_time(string("LogIn"),uidLogin[a] );
			}
		}

}




