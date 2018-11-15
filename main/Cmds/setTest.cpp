
/*
 * set_statusSend.cpp

 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
using namespace std;
#include "setErase.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void erase_config();
extern void postLog(int code,int code1);
extern uint32_t IRAM_ATTR millis();

void set_test(void * pArg){
	arg *argument=(arg*)pArg;
	string algo;
	char textl[50];

	printf("Test\n");
	if(!set_commonCmd(argument,false))
		return;

	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someones browser when asked
		goto exit;
	}

	sprintf(textl,"Test response %d",millis());
	algo=string(textl);
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Test\n");                  // A general status condition for display. See routine for numbers.
#endif
	exit:
	algo="";

}



