using namespace std;
#include "setClearLog.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void postLog(int code,int code1);
extern void delay(uint16_t a);
extern void relay(stateType estado);

void set_opendoor(void * pArg){
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
	if(!aqui.working)
	{
			algo="System is Off";
			sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);
			goto exit;
	}
	if(aqui.traceflag & (1<<CMDD))
		printf("[CMDD]OpenDoor\n");
	relay(0);
	algo="Door Activated";
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);

//	gpio_set_level((gpio_num_t)RELAY, 1);
//	delay(aqui.relay);
//	gpio_set_level((gpio_num_t)RELAY, 0);
//	postLog(DCONTROL,0);
	exit:
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}

