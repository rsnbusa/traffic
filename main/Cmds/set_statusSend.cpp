
/*
 * set_statusSend.cpp

 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
using namespace std;
#include "set_statusSend.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void write_to_flash();
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern  string makeDateString(time_t t);
;

void set_statusSend(void * pArg){
	char textl[100];
	arg *argument=(arg*)pArg;
	string webString;
	time_t now = 0;
	struct tm timeptr ;
	if(!set_commonCmd(argument,false))
		return;
	time(&now);
	localtime_r(&now, &timeptr);

	sprintf(textl,"%d!%d!%d!%d!%d!%s!%d!%d",aqui.opens,aqui.aborted,aqui.stucks,aqui.guards,aqui.countCycles>0?aqui.totalCycles/aqui.countCycles:0,
			makeDateString(aqui.lastOpen).c_str(),aqui.working,stateVM);
	webString=string(textl);
	sendResponse( argument->pComm,argument->typeMsg, webString,webString.length(),MSTATUS,false,false);            // send to someones browser when asked
	if(aqui.traceflag & (1<<GEND))
		printf("[GEND]Status %s\n",textl);                  // A general status condition for display. See routine for numbers.
	webString="";
//	free(pArg);
//	vTaskDelete(NULL);
}



