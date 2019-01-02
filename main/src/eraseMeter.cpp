/*
 * eraseMeter.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */

#include "eraseMeter.h"
extern void write_to_flash(bool andRecovery);
extern void postLog(int code, int code1,char *mensaje);
extern int write_backup_config();

void erase_config() //do the dirty work
{
	memset(&sysConfig,0,sizeof(sysConfig));
	sysConfig.centinel=CENTINEL;
	sysConfig.DISPTIME=DISPMNGR;
	sysConfig.mqtt[sizeof(MQTTSERVER)]=0;
	sysConfig.mqttport=MQTTPORT;
	strcpy(sysConfig.mqtt,"m15.cloudmqtt.com");
	strcpy(sysConfig.mqttUser,"yyfmmvmh");
	strcpy(sysConfig.mqttPass,"tREY8m3selD9");
	printf("Mqtt Erase %s:%d\n",sysConfig.mqtt,sysConfig.mqttport);
	memcpy(sysConfig.domain,"feediot.co.nf",13);// mosquito server feediot.co.nf
	sysConfig.domain[13]=0;
	sysConfig.working=true;
	sysConfig.keepAlive=60000;
	sysConfig.reserved=sysConfig.reserved2=1000;
	write_to_flash(true);
//	if(write_backup_config())
//		printf("Failed to write recover\n");
}



