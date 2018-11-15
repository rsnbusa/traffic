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
 EXTERN char 							APP[20];
 EXTERN char 							MQTTSERVER[18];
 EXTERN	char                   	 		meses[12][4];
 EXTERN u8                       	  	daysInMonth [12] ;
 EXTERN QueueHandle_t 					cola;
 EXTERN cmdRecord 						cmds[MAXCMDS];

// Critical Variables

 EXTERN config_flash        			sysConfig 	__attribute__((aligned(4)));
 EXTERN sequence_struct        			sysSequence __attribute__((aligned(4)));
 EXTERN lights_struct					sysLights 	__attribute__((aligned(4)));
 EXTERN cycle_struct					allCycles 	__attribute__((aligned(4)));

 EXTERN char                			ipaddr[16];

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
 EXTERN bool                			mqttf,tracef,showVoltage,verbose,timef,firmwf;
 EXTERN debugflags						traceflag;

 EXTERN string              			spublishTopic,cmdTopic,alertTopic;
 EXTERN string              			AP_NameString,publishString,nameStr,uidStr,montonUid[5];
 EXTERN uint8_t							sonUid;
 EXTERN int                				RSSI;

// =========================

 EXTERN char 							payload[256];
 EXTERN char 							TAGG[10];

 EXTERN SemaphoreHandle_t 				I2CSem,logSem;
 EXTERN QueueHandle_t 					uart0_queue;

 EXTERN bool 							llogf,connf,mongf,sntpf,displayf,rxtxf,semaphoresOff,kalive;
 EXTERN ip4_addr_t 						localIp;
 EXTERN cJSON 							*root;
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
 EXTERN struct mg_mgr 					mgr;
 EXTERN RESET_REASON 					reboot;
 EXTERN struct mg_send_mqtt_handshake_opts opts;
 EXTERN int								addHTTP,llevoHTTP;
 EXTERN u32								curSSID;
 EXTERN FILE 							*bitacora;
 EXTERN QueueHandle_t 					logQueue;
EXTERN uint8_t							sensors[1][8],numsensors;
//EXTERN int								cuenta,gwait;
EXTERN uint8_t							quiet;
EXTERN uint32_t							uidLogin[5];
EXTERN TimerHandle_t 					scheduleTimer,doneTimer;
EXTERN float							oldtemp;
EXTERN QueueHandle_t 					mqttQ;
EXTERN void*							mqttCon;
EXTERN string							idd;
EXTERN string							logText[17];
EXTERN t_symstruct 						lookuptable[NKEYS];
EXTERN char 							kbdTable[KCMDS][20];
EXTERN esp_mqtt_client_config_t  		settings;
EXTERN nvs_handle 						nvshandle,seqhandle,lighthandle;
EXTERN esp_mqtt_client_handle_t 		clientCloud, clientThing;
EXTERN u32								entran,salen,howmuch,interval,entrats;
EXTERN esp_adc_cal_characteristics_t 	*adc_chars;
EXTERN adc1_channel_t 					adcchannel;     //GPIO34 if ADC1, GPIO14 if ADC2
EXTERN adc_atten_t 						atten;
EXTERN uint32_t 						connectedToAp[10];
EXTERN cmd_struct 						answer;
EXTERN scheduler_struct					scheduler;
EXTERN u8								nextSchedule,totalConnected;
EXTERN u8								TODAY,globalNode,globalLuz;
EXTERN TaskHandle_t 					cycleHandle,runHandle,rxHandle,blinkHandle,mqttHandle,mongoHandle,mdnsHandle;
EXTERN u16								FACTOR,FACTOR2,vanconnect,globalDuration,globalLuzDuration;
EXTERN sta_status						activeNodes;
EXTERN char								tcmds[30][10];
EXTERN string							calles[6];
EXTERN int								gCycleTime;
EXTERN int								theSock,cuantoDura;
#endif /* MAIN_GLOBALS_H_ */
