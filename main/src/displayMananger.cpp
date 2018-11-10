/*
 * displayMananger.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */

#include "displayManager.h"
using namespace std;
extern  string makeDateString(time_t t);
extern void write_to_flash();
extern void loadDayBPK(u16 hoy);
extern uint32_t IRAM_ATTR millis();
extern void eraseMainScreen();
extern uint32_t readADC();
extern void delay(uint16_t a);
extern uint32_t millis();

void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase)
{
//	return;
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
		int xx=0;
		switch (lastalign) {
		case TEXT_ALIGN_LEFT:
			xx=x;
			break;
		case TEXT_ALIGN_CENTER:
			xx=x-w/2;
			break;
		case TEXT_ALIGN_RIGHT:
			xx=x-w;
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
		drawString(64, 20, cual.c_str(),24, TEXT_ALIGN_CENTER,DISPLAYIT, NOREP);
		xSemaphoreGive(I2CSem);
	}
		drawBars();
	//	display.drawLine(0,18,0,50);
	//	display.drawLine(127,18,127,50);
	//	display.display();
		oldtemp=0.0;
}

void timerManager(void *arg) {
	time_t t = 0;
	struct tm timeinfo ;
	char textd[20],textt[20];
	u32 nheap;

	while(true)
	{
		nheap=xPortGetFreeHeapSize();

		if(sysConfig.traceflag & (1<<HEAPD))
			printf("[HEAPD]Heap %d\n",nheap);

		vTaskDelay(1000/portTICK_PERIOD_MS);
		time(&t);
		localtime_r(&t, &timeinfo);

		if (displayf)
		{
			if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				sprintf(textd,"%02d/%02d/%04d",timeinfo.tm_mday,timeinfo.tm_mon+1,1900+timeinfo.tm_year);
				sprintf(textt,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
				drawString(16, 5, mqttf?string("m"):string("   "), 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
				drawString(0, 51, string(textd), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				drawString(86, 51, string(textt), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				drawString(61, 51, sysConfig.working?"On  ":"Off", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				if(gCycleTime>0)
				{
					sprintf(textd,"   %3ds   ",gCycleTime--);
					drawString(90, 0, textd, 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
				}
				xSemaphoreGive(I2CSem);
			}
		}
	}
}
void displayManager(void *arg) {
	//   gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
	//	int level = 0;
	char textl[20];
//	stateType oldState,moldState=CLOSED;
	string local;
	uint32_t tempwhen=0,t1,oldt1=0,lasttime=0;
	float temp,diff;
//	oldState=stateVM;
	oldtemp=0.0;

	gpio_set_direction((gpio_num_t)0, GPIO_MODE_INPUT);

	xTaskCreate(&timerManager,"timeMgr",4096,NULL, MGOS_TASK_PRIORITY, NULL);

	if (sysConfig.DISPTIME==0)
		sysConfig.DISPTIME=DISPMNGR;
	while (true) {

		if(millis()-tempwhen>1000 && numsensors>0)
		{
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
			tempwhen=millis();
		}

		vTaskDelay(sysConfig.DISPTIME/portTICK_PERIOD_MS);
	}
}

