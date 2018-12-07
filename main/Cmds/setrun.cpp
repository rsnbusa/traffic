using namespace std;
#include "setErase.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern int getParameter(arg* argument, string cual,char * donde);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void postLog(int code,int code1);
extern uint32_t IRAM_ATTR millis();
extern void sendMsg(int cmd,int aquien,int f1,int f2,char * que,int len);

void set_run(void * pArg){
	arg *argument=(arg*)pArg;
	cmd_struct cual;
	string algo;
	char textl[50];

	if(!set_commonCmd(argument,false))
		return;

	memset(textl,0,sizeof(textl));

	if(getParameter(argument,"password",textl)==ESP_OK)
		algo=string(textl);
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);            // send to someones browser when asked
		goto exit;
	}
	memset(textl,0,sizeof(textl));

	if(getParameter(argument,"street",textl)==ESP_OK)
			algo=string(textl);
	int cualst=atoi(algo.c_str());

	memset(textl,0,sizeof(textl));
	if(getParameter(argument,"long",textl)==ESP_OK)
				algo=string(textl);
	int longg=atoi(algo.c_str());

	sendMsg(RUN,cualst,longg,0,0,0); //Send date/time as server
	sprintf(textl,"Run Light %d for %d",cualst,longg);
	algo=string(textl);
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Run\n");                  // A general status condition for display. See routine for numbers.
#endif
	exit:
	algo="";
}



