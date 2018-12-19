using namespace std;
#include "setErase.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern int getParameter(arg* argument, string cual,char * donde);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern uint32_t IRAM_ATTR millis();
extern void sendMsg(int cmd,int aquien,int f1,int f2,char * que,int len);

void set_emergency(void * pArg){
	arg *argument=(arg*)pArg;
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
	algo="No station defined for Emergency";
	if(getParameter(argument,"stationid",textl)==ESP_OK)
	{
			int stationid=atoi(textl);
			if(stationid<sysConfig.totalLights)
			{
				sprintf(textl,"Emergency Priority StationId %d",stationid);
				algo=string(textl);
				//launch emergency task
				if(getParameter(argument,"duration",textl)==ESP_OK)
				{
						int duration=atoi(textl);
						sendMsg(EMERGENCY,EVERYBODY,stationid,duration,NULL,0);
				}
				else
					algo="No duration given for Emergency";
			}
			else
				algo="StationId out of range for Controller";
	}

	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<CMDD))
		printf("[CMDD]Emergency\n");                  // A general status condition for display. See routine for numbers.
#endif
	exit:
	algo="";
}



