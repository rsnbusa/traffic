/*
 * setCommon.cpp
 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */

#include "setCommon.h"
extern string getParameter(arg* argument,string cual);

void  set_latest_time(string datestr, string timestr, string utcStr){
	//set local internal Time and set RTC to received value. Its a total trust thing
#ifdef DEBUGSYS
	if(sysConfig.traceflag & (1<<GEND))
		PRINT_MSG("%d/%d/%d  %d:%d:%d\n",atoi(datestr.substr(6,4).c_str()),atoi(datestr.substr(0,2).c_str()),atoi(datestr.substr(3,2).c_str()),
				atoi(timestr.substr(0,2).c_str()),atoi(timestr.substr(3,2).c_str()),atoi(timestr.substr(6,2).c_str()));
#endif
}
void addUid(string cual)
{
	time_t now = 0;


	if (sonUid>4)
				return;
	for (int a=0;a<sonUid;a++)
	{
		if(strcmp(cual.c_str(),montonUid[a].c_str())==0)
			return;
	}
		time(&now);
		montonUid[sonUid]=cual;
		uidLogin[sonUid]=now;
		sonUid++;
}

bool set_commonCmd(arg* pArg,bool check)  //not really uselfull but was to use a single routine to check common parameters.
{
//	string s1,datestr,timestr,utcStr,nameStr;
	// process common arguments expected: date , time, uid and name
//	datestr=getParameter(pArg,"date");
//	timestr=getParameter(pArg,"time");
//	utcStr=getParameter(pArg,"UTC");  //must be a global variable
	uidStr=getParameter(pArg,"uid");
	addUid(uidStr);
//	nameStr=getParameter(pArg,"bff");
	return true;

}




