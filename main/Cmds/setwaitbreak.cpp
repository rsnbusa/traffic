using namespace std;
#include "setwaitbreak.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void postLog(int code,int code1);
extern void delay(uint16_t a);
extern void write_to_flash();
extern void relay(stateType estado);

void set_waitbreak(void * pArg){
	arg *argument=(arg*)pArg;
	string algo;

	if(!set_commonCmd(argument,false))
		return;

	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someone's browser when asked
		goto exit;
	}

	aqui.waitBreak=true;
	write_to_flash();

	algo="Break On";
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MSHOW,false,false);            // send to someone's browser when asked
//	if(tracef)
//		printf("Break Relay %d\n",stateVM);
	if(stateVM==CLOSED)
		relay(stateVM);
//	postLog(BREAKMODE,0);

	exit:
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}

