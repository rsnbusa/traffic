
/*
 * setGeneral.cpp

 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */
using namespace std;
#include "setGeneral.h"
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void delay(uint16_t a);
extern void postLog(int code,int code1);

void set_generalap(void * pArg){
	arg *argument=(arg*)pArg; //set pet name and put name in bonjour list if changed
	char textl[20];
	string s1,webString,state;
	set_commonCmd(argument,false);
	s1=getParameter(argument,"meter");
	if (s1 !="")
	{
		memcpy((void*)aqui.meterName,s1.c_str(),s1.length()+1);
		if(aqui.traceflag & (1<<CMDD))
			PRINT_MSG("[CMDD]Name %s\n",aqui.meterName);
		webString="General Info set";
		//    sendResponse( webString);            // send to someones browser when asked
		if(aqui.traceflag & (1<<CMDD))
			PRINT_MSG("[CMDD]%s\n",webString.c_str());

		s1=getParameter(argument,"group");
		if (s1 == "" )
			memcpy(aqui.groupName,aqui.meterName,sizeof(aqui.meterName));
		else
			memcpy(aqui.groupName,s1.c_str(),s1.length()+1);
//		postLog(DBORN,0);
		s1=getParameter(argument,"email");
		memcpy(aqui.email[0],s1.c_str(),s1.length()+1);
		memcpy(aqui.emailName[0],"Owner",5);
		aqui.emailName[0][5]=0;
		aqui.ecount=1;

		s1=getParameter(argument,"reset");
		int res=atoi(s1.c_str());

		s1=getParameter(argument,"index");
		int index=atoi(s1.c_str());
		if (index>4)
			index=4;

		s1=getParameter(argument,"ap");

		if (s1=="")
		{// it the update cmd not a AP option
			write_to_flash();
			webString="General Erased SSID";
			sendResponse( argument->pComm,argument->typeMsg, webString,webString.length(),MINFO,false,false);            // send to someones browser when asked
			free(pArg);
			return;
		}
		memcpy(aqui.ssid[index],s1.c_str(),s1.length()+1);
		sprintf(textl,"Ap[%d] set %s",index,s1.c_str());
		s1=string(textl);
		postLog(APSET,0);
		s1=getParameter(argument,"pass");
		memcpy(aqui.pass[index],s1.c_str(),s1.length()+1);

		spublishTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/MSG";
		cmdTopic=string(APP)+"/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/CMD";
		alertTopic=string(APP)+"/"+"EcuaHeat/Monitor/"+string(aqui.groupName)+"/"+string(aqui.meterName)+"/ALERT";

		// Set MQTT Server and Port
			state=getParameter(argument,"qqqq");
			if(state!="")
			{
				memcpy(&aqui.mqtt,state.c_str(),state.length()+1);
				res=true;
				printf("Mqtt %s\n",aqui.mqtt);
				state=getParameter(argument,"port");
				if(state!="")
					aqui.mqttport=atoi(state.c_str());
				else
					aqui.mqttport=1883; //default
			//	webstring+="Q-";

			}
			state="";

		state=getParameter(argument,"uupp");
		if(state!="")
		{
			memcpy(&aqui.mqttUser,state.c_str(),state.length()+1);
			state=getParameter(argument,"passq");
			printf("Mqtt password %s\n",state.c_str());
			if(state!="")
			{
				memcpy(&aqui.mqttPass,state.c_str(),state.length()+1);
				res=true; //restart
			}
		//	webstring+="QU-";
		}
		state="";

		write_to_flash();
		sendResponse( argument->pComm,argument->typeMsg, webString,webString.length(),MINFO,false,false);            // send to someones browser when asked
		delay(3000);
		int son=10;
		if(res)
		{
			while (son--)
					mg_mgr_poll(&mgr, 10);
			printf("Restart\n");
			delay(1000);
			esp_restart();
		}

	}
	s1=webString="";
//	free(pArg);
//	vTaskDelete(NULL);
}





