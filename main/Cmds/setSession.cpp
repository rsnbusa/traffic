using namespace std;
#include "setSession.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);


void set_session(void * pArg){
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
	algo="Session established";
	exit:

		vTaskDelay(1000 /  portTICK_RATE_MS);

	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MSHOW,false,false);            // send to someones browser when asked
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}

