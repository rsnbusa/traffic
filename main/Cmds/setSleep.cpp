using namespace std;
#include "setSleep.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void postLog(int code,int code1);
extern void delay(uint16_t a);
extern void write_to_flash();

void set_sleepmode(void * pArg){
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

	aqui.working=!aqui.working;
	write_to_flash();
	if(aqui.traceflag & (1<<CMDD))
		aqui.working?printf("[CMDD]ActiveMode\n"):printf("[CMDD]SleepMode\n");


	algo=aqui.working?"Active Mode":"Sleep Mode";
	aqui.working?postLog(ACTIVEMODE,0):postLog(SLEEPMODE,0);
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MSLEEP,false,false);            // send to someones browser when asked
	exit:
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}

