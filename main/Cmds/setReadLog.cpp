
/*
 * set_statusSend.cpp

 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
using namespace std;
#include "setSettings.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern string makeDateString(time_t t);
extern void delay(uint16_t a);

void set_readlog(void * pArg){
	arg *argument=(arg*)pArg;
	char *buffer;
	string algo;

	if(argument->typeMsg!=1)
	{
		algo="Cmd only in mqtt format";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),NOERROR,false,false);            // send to someone's browser when asked
		goto exit;
	}
	if(!set_commonCmd(argument,false))
		goto exit;

	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someone's browser when asked
		goto exit;
	}

	//limit the size of the output to MAXHTTP
	fseek (bitacora , 0 , SEEK_END);
	int lSize;
	lSize=ftell (bitacora);
	rewind (bitacora);
	if(lSize>MAXHTTP)
		lSize=MAXHTTP;
	lSize+=4; //2 for code and 2 for centinel

	buffer = (char*) malloc (lSize);
	if(buffer==NULL)
	{
		printf("No Memory buffer\n");
		goto exit;
	}
	buffer[0]='9';
	buffer[1]='9';
	memset(&buffer[2],0xa0,2); //Centinel is A0A0

	fread (&buffer[4],1,lSize-2,bitacora);

	if(uidStr != "") //If did not received a UID send without UID as a queue subheader
					spublishTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/"+uidStr+"/MSG";
				else
					spublishTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/MSG";
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]Readlog Queue %s sizemsg %d comm %p\n",spublishTopic.c_str(),lSize,argument->pComm);
	esp_mqtt_client_handle_t mcomm=( esp_mqtt_client_handle_t)argument->pComm;
	esp_mqtt_client_publish(mcomm, (char *)spublishTopic.c_str(), buffer,lSize, 0, 0); //Log file Only in MQTT format

	delay(500);
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]readLog\n");                  // A general status condition for display. See routine for numbers.
	free(buffer);

	exit:algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}
