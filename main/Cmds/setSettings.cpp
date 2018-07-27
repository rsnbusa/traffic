
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

void set_settings(void * pArg){
	arg *argument=(arg*)pArg;
	char textl[100];
	string algo;

	if(!set_commonCmd(argument,false))
		goto exit;
	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someones browser when asked
		goto exit;
	}
	sprintf(textl,"%d!%d!%d!%d!%d!%d",aqui.wait,aqui.openTimeout,aqui.sleepTime,aqui.guardOn,aqui.sendMqtt,aqui.motorw);
	algo=string(textl);
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]settings\n");                  // A general status condition for display. See routine for numbers.

	exit:
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}
