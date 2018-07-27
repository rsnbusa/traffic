
/*
 * set_statusSend.cpp

 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
using namespace std;
#include "setInternal.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void write_to_flash();
extern void delay(uint16_t a);
extern void postLog(int code,int code1);

void set_internal(void * pArg){
	arg *argument=(arg*)pArg;
	int8_t cual;
	string webstring,state,state1;
	bool res=false,wf=false;

	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]Internal\n");
	webstring="Internal ";

	if(!set_commonCmd(argument,false))
		goto exit;


	state=getParameter(argument,"password");
	if(state!="zipo")
	{
		if(aqui.traceflag & (1<<CMDD))
			printf("[CMDD]Internal Not Authorized %s\n",state.c_str());
		state="Not authorized ";
		sendResponse( argument->pComm,argument->typeMsg, state,state.length(),ERRORAUTH,false,false);            // send to someones browser when asked
		goto exit;
	}
	state="";
	state=getParameter(argument,"reset");
	res=atoi(state.c_str());
	state="";

	// Set SSID and Password and use iiii as the position to save. Up to 5 ssid/passwords
	state=getParameter(argument,"ssss"); //this command can will end call. If more need do not use this option
	if(state!="")
	{
		webstring+="AP-";
		cual=0;

		memcpy((void*)&aqui.ssid[cual][0],(void*)state.c_str(),state.length()+1);
		state="";
		printf("SSSS %s\n",aqui.ssid[cual]);

		state=getParameter(argument,"pppp");
		if(state!="")
		{
			memcpy(&aqui.pass[cual][0],state.c_str(),state.length()+1);
			postLog(APSET,0);
		}
		else
			webstring+="Missing Password ";
	//	goto sale;
	}
	state="";

	// Set MQTT Server and Port
	state=getParameter(argument,"qqqq");
	if(state!="")
	{
		memcpy(&aqui.mqtt,state.c_str(),state.length()+1);
		res=true;
		printf("Mqtt %s\n",aqui.mqtt);
		state=getParameter(argument,"port");
		if(state!="")
			aqui.mqttport=atoi(state.c_str());
		else
			aqui.mqttport=1883; //default
		webstring+="Q-";

	}
	state="";

	state=getParameter(argument,"wwww");
	if(state!="")
	{
		aqui.wait=atoi(state.c_str());
		webstring+="W-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"cctt");
	if(state!="")
	{
		aqui.closeTimeout=atoi(state.c_str());
		aqui.openTimeout=aqui.closeTimeout;
		webstring+="CTO-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"rrrr");
	if(state!="")
	{
		aqui.relay=atoi(state.c_str());
		webstring+="R-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"sstt");
	if(state!="")
	{
		aqui.sleepTime=atoi(state.c_str());
		webstring+="S-";
		wf=true;
		if( xTimerIsTimerActive( dispTimer ) != pdFALSE )
					xTimerStop(dispTimer,0);
		if(aqui.sleepTime>0){
			xTimerChangePeriod(dispTimer,aqui.sleepTime/portTICK_PERIOD_MS,0);
				xTimerStart(dispTimer,0);
		}
	}
	state="";

	state=getParameter(argument,"stats");
	if(state!="")
	{
		aqui.opens=aqui.stucks=aqui.guards=aqui.aborted=0;
		webstring+="RST";
		wf=true;
	}
	state="";

	state=getParameter(argument,"menos");
	if(state!="")
	{
		aqui.menos=atoi(state.c_str());
		webstring+="%-";
		wf=true;
	}
	state="";
	state=getParameter(argument,"guard");
	if(state!="")
	{
		aqui.guardOn=atoi(state.c_str());
		webstring+="G-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"motor");
	if(state!="")
	{
		aqui.motorw=atoi(state.c_str());
		webstring+="M-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"apindex");
	if(state!="")
	{
		aqui.lastSSID=atoi(state.c_str());
		webstring+="I-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"auto");
	if(state!="")
	{
		aqui.sendMqtt=atoi(state.c_str());
		webstring+="A-";
		wf=true;
	}
	state="";

	state=getParameter(argument,"trace");
	if(state!="")
	{
		tracef=atoi(state.c_str());
		webstring+="T-";
	}
	state="";

	if (wf)
		write_to_flash();

	// Set MQTT User and Password
	state=getParameter(argument,"uupp");
	if(state!="")
	{
		memcpy(&aqui.mqttUser,state.c_str(),state.length()+1);
		state=getParameter(argument,"passq");
		printf("Mqtt password %s\n",state.c_str());
		if(state!="")
		{
			memcpy(&aqui.mqttPass,state.c_str(),state.length()+1);
			res=true; //restart
		}
		webstring+="QU-";
	}
	state="";

	// Set Meter Name
	state=getParameter(argument,"nnnn");
	if(state!="")
	{
		memcpy(&aqui.meterName,state.c_str(),state.length()+1);
		memcpy(&aqui.groupName,state.c_str(),state.length()+1);
		webstring+="N-";
	}
	state="";


//	postLog(LINTERNAL,0);

//	sale:
	if(aqui.traceflag & (1<<CMDD))
		PRINT_MSG("[CMDD]Internal setup\n");
	write_to_flash();

	sendResponse( argument->pComm,argument->typeMsg, webstring,webstring.length(),MSHOW,false,false);            // send to someones browser when asked
	if(res)
	{
		delay(2000);
		esp_restart();
	}

	exit:
	webstring=state="";
//	free(pArg);
//	vTaskDelete(NULL);
}



