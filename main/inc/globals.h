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

 EXTERN config_flash        			sysConfig 	__attribute__((aligned(4)));
 EXTERN sequence_struct        			sysSequence __attribute__((aligned(4)));
 EXTERN lights_struct					sysLights 	__attribute__((aligned(4)));
 EXTERN cycle_struct					allCycles 	__attribute__((aligned(4)));

 EXTERN	I2C								miI2C;
#ifdef GLOBAL
 EXTERN	SSD1306             			display;
#else
 EXTERN SSD1306 						display(0x3c, &miI2C);
#endif

 EXTERN	i2ctype 						i2cp;

 EXTERN struct rst_info   				*rtc_info;
 EXTERN debugflags						traceflag;

 EXTERN ip4_addr_t 						localIp;
 EXTERN cJSON 							*root;

 EXTERN char 							ota_write_data[BUFFSIZE + 1] ;
/*an packet receive buffer*/
 EXTERN char 							text[BUFFSIZE + 1] ;
/* an image total length*/
 EXTERN esp_ota_handle_t 				update_handle ;
 EXTERN esp_partition_t 				operate_partition;

 EXTERN SemaphoreHandle_t 				I2CSem,logSem;
 EXTERN QueueHandle_t 					cola,uart0_queue,logQueue,upQ,downQ;
 EXTERN struct mg_mgr 					mgr;
 EXTERN RESET_REASON 					reboot;
 EXTERN struct mg_send_mqtt_handshake_opts opts;
 EXTERN FILE 							*bitacora;
 EXTERN TimerHandle_t 					scheduleTimer,doneTimer;
 EXTERN QueueHandle_t 					mqttQ;
 //EXTERN void*							mqttCon;
 EXTERN t_symstruct 					lookuptable[NKEYS];
 EXTERN esp_mqtt_client_config_t  		settings;
 EXTERN esp_mqtt_client_handle_t		gClient;
 EXTERN nvs_handle 						nvshandle,seqhandle,lighthandle,backhandle;
 EXTERN esp_mqtt_client_handle_t 		clientCloud;
 EXTERN cmd_struct 						answer;
 EXTERN scheduler_struct				scheduler;
 EXTERN TaskHandle_t 					cycleHandle,runHandle,rxHandle,blinkHandle,mqttHandle,mongoHandle,mdnsHandle;
 EXTERN sta_status						activeNodes;
 EXTERN cmdRecord 						cmds[MAXCMDS];
 EXTERN login_struct					logins[20];
 EXTERN mbedtls_md_context_t 			md5;
 EXTERN httpd_handle_t 					server;
 EXTERN httpd_uri_t 					loscmds[30];
 EXTERN functp							theCode[MAXCMDS];
 EXTERN statistics_struct				internal_stats;

 EXTERN string							logText[20],idd,calles[6],spublishTopic,cmdTopic,AP_NameString,nameStr,uidStr,montonUid[5];
 EXTERN bool 							rxmessagef,llogf,connf,mongf,sntpf,displayf,rxtxf,semaphoresOff,kalive,mqttf,tracef,timef,firmwf,globalWalk,backupf;
 EXTERN float							oldtemp;
 EXTERN u8								daysInMonth[12],sensors[1][8],numsensors,quiet,nextSchedule,totalConnected,TODAY,globalNode,globalLuz;
 EXTERN u8               				mesg,diag,horag,oldHorag,oldDiag,oldMesg,lastalign,lastFont,barX[3],barH[3],userNum,sonUid,numLogins;
 EXTERN u16								binary_file_length,yearg,daysg,FACTOR,FACTOR2,vanconnect,globalDuration,globalLuzDuration,curSSID,burnt[MAXNODES];
 EXTERN int								RSSI,gCycleTime,cuantoDura,addHTTP,llevoHTTP,socket_id,keepAlive;
 EXTERN uint32_t						uidLogin[5],entran,salen,howmuch,interval,entrats,connectedToAp[20],upstream,downstream;
 EXTERN char 							APP[20],MQTTSERVER[18],meses[12][4],http_request[100],kbdTable[KCMDS][20],tcmds[30][10],
 	 	 	 	 	 	 	 	 	 	cmdName[MAXCMDS][20],bulbColors[10][10];
 EXTERN const char						miclient[40];
#endif /* MAIN_GLOBALS_H_ */
