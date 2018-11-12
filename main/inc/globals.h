/*
 * globals.h
 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */

#ifndef MAIN_GLOBALS_H_
#define MAIN_GLOBALS_H_

#ifdef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif
#include "defines.h"
#include "projTypes.h"
#include "freertos/timers.h"
using namespace std;


// =========================
 EXTERN char 							APP[9];
 EXTERN char 							WHOAMI[23];
 EXTERN char 							MQTTSERVER[18];
 EXTERN	char                   	 		meses[12][4];
 EXTERN u8                       	  	daysInMonth [12] ;

 EXTERN  cmdRecord 						cmds[MAXCMDS];

// Critical Variables

 EXTERN config_flash        			sysConfig 	__attribute__((aligned(4)));
 EXTERN sequence_struct        			sysSequence __attribute__((aligned(4)));
 EXTERN lights_struct					sysLights 	__attribute__((aligned(4)));
 EXTERN cycle_struct					allCycles 	__attribute__((aligned(4)));

 EXTERN char                			ipaddr[16];

 EXTERN Nodo							esteNodo;
// Devices and Services
 EXTERN	I2C								miI2C;
#ifdef GLOBAL
 EXTERN	SSD1306             			display;
#else
 EXTERN SSD1306 						display(0x3c, &miI2C);
#endif

 EXTERN	i2ctype 						i2cp;
// General Use

 EXTERN struct rst_info   				*rtc_info; //Restart System variable. States Reason for reboot. Look first line for reasons
 EXTERN u8               				mesg,diag,horag,oldHorag,oldDiag,oldMesg,lastalign,lastFont;
 EXTERN u8                				barX[3],barH[3],userNum;
 EXTERN u16								yearg,daysg;
 EXTERN bool                			mqttf,tracef,showVoltage;
 EXTERN bool                			verbose,timef;
 EXTERN debugflags						traceflag;

 EXTERN string              			spublishTopic,cmdTopic,alertTopic;
 EXTERN string              			AP_NameString,publishString,decirlo,nameStr,fromDateStr,toDateStr,lastDateStr,uidStr,montonUid[5];
 EXTERN uint8_t							sonUid;
 EXTERN char                			AP_NameChar[MAXCHARS];
 EXTERN char   							WIFIME[12];//must be 8 chars Password by default of ESP8266 MeterIoT Access Point
 EXTERN char   							eserver[40];
 EXTERN int                				eport ,RSSI;

// =========================

 EXTERN char 							payload[256];
 EXTERN char 							s_address[40];
 EXTERN char 							s_user_name[20];
 EXTERN char 							s_password[20];
 EXTERN char 							s_topic[20];
 EXTERN char 							a_topic[20];
 EXTERN char 							TAGG[10];
 EXTERN struct mg_mqtt_topic_expression s_topic_expr;

 EXTERN SemaphoreHandle_t 				I2CSem,logSem,ackSem;
 EXTERN QueueHandle_t 					uart0_queue;
 EXTERN uint8_t 						recontimes;

 EXTERN bool 							llogf,timerF,reconf,connf,mongf,mdnsf,timerf,sntpf,guardf,displayf,breakf,errorf,gMotorMonitor,guardfopen,mqttThingf;
 EXTERN bool							gGuard,rxtxf,semaphoresOff;
 EXTERN ip4_addr_t 						localIp;
 EXTERN struct 							timeval tvStart;
 EXTERN cJSON 							*root;
 EXTERN cJSON 							*mensa,*counterJ,*timeJ;
 EXTERN char 							ota_write_data[BUFFSIZE + 1] ;
/*an packet receive buffer*/
 EXTERN char 							text[BUFFSIZE + 1] ;
/* an image total length*/
 EXTERN int 							binary_file_length ;
/*socket id*/
 EXTERN int 							socket_id,keepAlive;
 EXTERN char 							http_request[100] ;
/* operate handle : uninitialized value is zero ,every ota begin would exponential growth*/
 EXTERN esp_ota_handle_t 				update_handle ;
 EXTERN esp_partition_t 				operate_partition;
 EXTERN argumento 						args[MAXDEVS];
 EXTERN struct mg_mgr 					mgr;
 EXTERN cJSON *							cmdItem;
 EXTERN RESET_REASON 					reboot;
 EXTERN struct mg_send_mqtt_handshake_opts opts;
 EXTERN int								addHTTP,llevoHTTP;
 EXTERN struct mg_connection*			globalComm;
 EXTERN u32								curSSID,usertime,lastGRelay,lastGuard;
 EXTERN FILE 							*bitacora;
 EXTERN QueueHandle_t 					logQueue;
EXTERN uint8_t							sensors[20][8],numsensors,guardCount;
EXTERN uint16_t							GMAXLOSSPER;
EXTERN int								cuenta,gwait;
EXTERN uint8_t							cuentaRelay,quiet;
EXTERN uint32_t							startCycle,wait,menos,endCycle,startClosing,uidLogin[5],globalDisp,globalTotalDisp,basePulse,motorp[4];//,motorcop[4];
EXTERN QueueSetHandle_t					closeQueueSet,openQueueSet;
EXTERN TimerHandle_t 					scheduleTimer,doneTimer;
EXTERN float							oldtemp;
EXTERN QueueHandle_t 					mqttQ;
EXTERN void*							mqttCon;
EXTERN string							idd;
EXTERN string							logText[17];
EXTERN t_symstruct 						lookuptable[NKEYS];
EXTERN char 							kbdTable[KCMDS][20];
EXTERN esp_mqtt_client_config_t  		settings,settingsThing;
EXTERN uint16_t							startLed,gMotorCount;//,pulsecnt;
EXTERN 	nvs_handle 						nvshandle,seqhandle,lighthandle;
EXTERN TaskHandle_t 					oBHandle, OBHandle,cBHandle,CBHandle; //openingBreak, OpenedBreak, closingBreak,ClosedBreak
EXTERN esp_mqtt_client_handle_t 		clientCloud, clientThing;
EXTERN u32								gsleepTime,entran,salen,howmuch,interval,entrats;
EXTERN esp_adc_cal_characteristics_t 	*adc_chars;
EXTERN adc1_channel_t 					adcchannel;     //GPIO34 if ADC1, GPIO14 if ADC2
EXTERN adc_atten_t 						atten;
EXTERN uint32_t 						connectedToAp[10];
EXTERN cmd_struct 						answer;
EXTERN scheduler_struct					scheduler;
EXTERN u8								nextSchedule,totalConnected;
EXTERN u8								TODAY;
EXTERN TaskHandle_t 					cycleHandle,runHandle,rxHandle,blinkHandle;
EXTERN u16								FACTOR,FACTOR2;
EXTERN sta_status						activeNodes;
EXTERN char								tcmds[30][10];
EXTERN string							calles[6];
EXTERN int								gCycleTime;
EXTERN int								theSock,cuantoDura;
#endif /* MAIN_GLOBALS_H_ */
