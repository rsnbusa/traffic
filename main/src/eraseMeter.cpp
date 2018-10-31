/*
 * eraseMeter.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */

#include "eraseMeter.h"
extern void write_to_flash();
extern void postLog(int code, string mensaje);

void erase_config() //do the dirty work
{
	memset(&aqui,0,sizeof(aqui));
	aqui.centinel=CENTINEL;
	aqui.DISPTIME=DISPMNGR;
	aqui.mqtt[sizeof(MQTTSERVER)]=0;
	aqui.mqttport=MQTTPORT;
	strcpy(aqui.mqtt,"m13.cloudmqtt.com");
	strcpy(aqui.mqttUser,"wckwlvot");
	strcpy(aqui.mqttPass,"MxoMTQjeEIHE");
	printf("Mqtt Erase %s:%d\n",aqui.mqtt,aqui.mqttport);
	memcpy(aqui.domain,"feediot.co.nf",13);// mosquito server feediot.co.nf
	aqui.domain[13]=0;
	aqui.relay=700;
	aqui.wait=5000;
	aqui.sleepTime=20000;
	aqui.openTimeout=30000;
	aqui.closeTimeout=30000;
	aqui.sendMqtt=1;
	aqui.menos=aqui.wait/2;
	aqui.guardOn=true;
	aqui.working=true;
	write_to_flash();
}



