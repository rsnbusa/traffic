
/*
 * set_statusSend.cpp

 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
using namespace std;
#include "setResets.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern string makeDateString(time_t t);
extern void delay(uint16_t a);
extern void write_to_flash();
extern void postLog(int code,int code1);

void set_reset(void * pArg){
	arg *argument=(arg*)pArg;
	string algo;

	if(!set_commonCmd(argument,false))
		return;

	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someones browser when asked
		goto exit;
	}



	algo="Will reset in 5 seconds";

	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]reset\n");                  // A general status condition for display. See routine for numbers.
	delay(5000);
	esp_restart();
	exit:
	algo="";
	//useless but....
//	free(pArg);
//	vTaskDelete(NULL);
}

void set_resetstats(void * pArg){
	arg *argument=(arg*)pArg;
	string algo;

	if(!set_commonCmd(argument,false))
		return;

	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someones browser when asked
		goto exit;
	}
	aqui.opens=aqui.stucks=aqui.guards=aqui.aborted=aqui.totalCycles=aqui.countCycles=0;
	write_to_flash();
	if(  xSemaphoreTake(logSem, portMAX_DELAY))
	{
		fclose(bitacora);
		bitacora = fopen("/spiflash/log.txt", "w"); //truncate
		if(bitacora)
		{
			fclose(bitacora); //Close and reopen r+
			if(aqui.traceflag & (1<<CMDD))
				printf("[CMDD]Log Cleared\n");
			xSemaphoreGive(logSem);
			bitacora = fopen("/spiflash/log.txt", "r+"); //truncate

			postLog(DLOGCLEAR,0);
		}
		else
			xSemaphoreGive(logSem);

	}
	algo="Reset stats";

	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]ResetStats\n");                  // A general status condition for display. See routine for numbers.

	exit:
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}



