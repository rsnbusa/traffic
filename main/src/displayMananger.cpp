/*
 * displayMananger.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */

#include "displayManager.h"
using namespace std;
extern uint32_t IRAM_ATTR millis();
extern void delay(uint32_t a);
extern void heartBeat(void *pArg);
void sendAlert(string que, int len);


void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase)
{
 if (!displayf)
	 return;

	if(fsize!=lastFont)
	{
		lastFont=fsize;
		switch (fsize)
		{
		case 10:
			display.setFont(ArialMT_Plain_10);
			break;
		case 16:
			display.setFont(ArialMT_Plain_16);
			break;
		case 24:
			display.setFont(ArialMT_Plain_24);
			break;
		default:
			break;
		}
	}

	if(lastalign!=align)
	{
		lastalign=align;

		switch (align) {
		case TEXT_ALIGN_LEFT:
			display.setTextAlignment(TEXT_ALIGN_LEFT);
			break;
		case TEXT_ALIGN_CENTER:
			display.setTextAlignment(TEXT_ALIGN_CENTER);
			break;
		case TEXT_ALIGN_RIGHT:
			display.setTextAlignment(TEXT_ALIGN_RIGHT);
			break;
		default:
			break;
		}
	}

	if(erase==REPLACE)
	{
		int w=display.getStringWidth((char*)que.c_str());
		if(w<=0)
			return;
		int xx=0;
		switch (lastalign) {
		case TEXT_ALIGN_LEFT:
			xx=x;
			if (xx<0)
				xx=0;
			break;
		case TEXT_ALIGN_CENTER:
			xx=x-w/2;
			if(xx<0)
				xx=0;
			break;
		case TEXT_ALIGN_RIGHT:
			xx=x-w;
			if(xx<0)
				xx=0;
			break;
		default:
			break;
		}
		display.setColor(BLACK);
		display.fillRect(xx,y,w,lastFont+3);
		display.setColor(WHITE);
	}

	display.drawString(x,y,(char*)que.c_str());
	if (showit==DISPLAYIT)
		display.display();
}

void drawBars()
{
	//return;
	wifi_ap_record_t wifidata;
	if (esp_wifi_sta_get_ap_info(&wifidata)==0){
		//		printf("RSSI %d\n",wifidata.rssi);
		RSSI=80+wifidata.rssi;
	}
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		for (int a=0;a<3;a++)
		{
			if (RSSI>RSSIVAL)
				display.fillRect(barX[a],YB-barH[a],WB,barH[a]);
			else
				display.drawRect(barX[a],YB-barH[a],WB,barH[a]);
			RSSI -= RSSIVAL;
		}

		if (mqttf)
				drawString(16, 5, string("m"), 10, TEXT_ALIGN_LEFT,DISPLAYIT, NOREP);
		display.display();
		xSemaphoreGive(I2CSem);
	}
}

void setLogo(string cual)
{
	//return;
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		display.setColor(BLACK);
		display.clear();
		display.setColor(WHITE);
		display.drawLine(0,18,127,18);
		display.drawLine(0,50,127,50);
		drawString(64, 20, cual.c_str(),24, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
		xSemaphoreGive(I2CSem);
	}
		drawBars();
	//	display.drawLine(0,18,0,50);
	//	display.drawLine(127,18,127,50);
	//	display.display();
		oldtemp=0.0;
}

void checkAlive(void *pArg)
{
	struct tm  ts;
	time_t now;
	char textl[100];
	int cuales;

	while(true) //forever
	{
		delay(sysConfig.keepAlive+1000); //one second more than the current keepAlive. Give him chance to log he is alive
		if(kalive)
		{
			time(&now);
			for (int a=1;a<=numLogins;a++)
			{
				if((now-activeNodes.lastTime[a])>(sysConfig.keepAlive/1000) && !activeNodes.dead[a])
				{
					localtime_r(&activeNodes.lastTime[a], &ts);
					cuales=0;
					for (int b=0;b<numLogins;b++)
						if(logins[b].stationl==a)
							cuales=b;
					sprintf(textl,"Light[%d] %s is dead,last seen alive %s",a,logins[cuales].namel,asctime(&ts));
					activeNodes.dead[a]=true;
//					if(a<(numLogins-1))
//						memcpy(&logins[a],&logins[a+1],sizeof(logins[0])*(numLogins-a-1))
//					numLogins--;
					sendAlert(string(textl), strlen(textl));
				}

			}
		}
	}
}
void timerManager(void *arg) {
	time_t t = 0;
	struct tm timeinfo ;
	char textd[20],textt[20],textl[50];
//	u32 nheap;
	u8 countLoginTime=0;
	bool sentLogin=false;
	u16 tryMqtt=20;
	float temp=200.0,diff;
	u32 wasmillis=0;


	while(true)
	{
//		nheap=xPortGetFreeHeapSize();

//#ifdef DEBUGSYS
//		if(sysConfig.traceflag & (1<<HEAPD))
//			printf("[HEAPD]Heap %d\n",nheap);
//#endif

		vTaskDelay(1000/portTICK_PERIOD_MS);
		time(&t);
		localtime_r(&t, &timeinfo);

		if(!rxtxf)
		{
			countLoginTime++;

			if(numLogins>=(sysConfig.totalLights-1))
			{
				delay(1000);
				rxtxf=true;
				sendMsg(SENDCLONE,EVERYBODY,0,0,NULL,0);
				xTaskCreate(&checkAlive,"alive",4096,NULL, 5, NULL);
				xTaskCreate(&heartBeat, "heartB", 4096, NULL, 4, NULL);
				time(&t);
				internal_stats.session_start=t;
				localtime_r(&t, &timeinfo);
				sprintf(textl,"Boot Complete for %s  %d stations joined at %s",sysConfig.lightName,sysConfig.totalLights-1,asctime(&timeinfo));
				while(!mqttf && tryMqtt)
				{
					tryMqtt--;
					delay(500);
				}
				if(mqttf)
					sendAlert(string(textl), strlen(textl));
			}
			else
			{
				if(countLoginTime>MAXLOGINTIME && !sentLogin)
				{
					time(&t);
					localtime_r(&t, &timeinfo);
					sprintf(textl,"Login Timeout for %s at %s",sysConfig.lightName,asctime(&timeinfo));
					sentLogin=true;
					sendAlert(string(textl), strlen(textl));
				}
			}
		}

		if (displayf)
		{
			if(millis()-wasmillis>10000)
			{
				wasmillis=millis();
				temp=DS_get_temp(&sensors[0][0]);
				diff=temp-oldtemp;
				if(diff<0.0)
					diff*=-1.0;
				if (diff>0.3 && temp<130.0)
				{
					if(xSemaphoreTake(I2CSem, portMAX_DELAY))
					{
						sprintf(textl,"%.02fC\n",temp);
						drawString(50, 0, "     ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
						drawString(50, 0, string(textl), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
						oldtemp=temp;
						xSemaphoreGive(I2CSem);
					}
				}
			}

			if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				sprintf(textd,"%02d/%02d/%02d   ",timeinfo.tm_mday,timeinfo.tm_mon+1,1900+timeinfo.tm_year-2000);
				sprintf(textt,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
				drawString(16, 5, mqttf?string("m"):string("   "), 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
				drawString(0, 51, string(textd), 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
				drawString(86, 51, string(textt), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			//	drawString(61, 51, sysConfig.working?"On  ":"Off", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				if(gCycleTime>0)
				{
					sprintf(textd,"   %3ds   ",gCycleTime--);
					drawString(90, 0, textd, 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				}
				if(walk[globalNode])
					drawString(10, 28, " W ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				else
					drawString(10, 28, "    ", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);


				xSemaphoreGive(I2CSem);
			}
		}
	}
}
