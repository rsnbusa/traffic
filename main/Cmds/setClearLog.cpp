using namespace std;
#include "setClearLog.h"

extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void postLog(int code,int code1);

void set_clearLog(void * pArg){
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
	fclose(bitacora);
	bitacora = fopen("/spiflash/log.txt", "w");//truncate to 0 len
	fclose(bitacora);
	bitacora = fopen("/spiflash/log.txt", "a");//open append

	exit:algo="Log cleared";
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
	postLog(DLOGCLEAR,0);
	if(aqui.traceflag & (1<<GEND))
		printf("[GEND]Set clearlog\n");
	//useless but....
	algo="";
//	free(pArg);
//	vTaskDelete(NULL);
}

