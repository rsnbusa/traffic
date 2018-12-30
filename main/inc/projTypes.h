#ifndef projT_h
#define projT_h

#include "includes.h"
#include <string>
using namespace std;

typedef struct {
	char *mensaje;
	void *nc;
	int sizer;
} mqttMsg;

typedef struct {
	gpio_num_t sdaport,sclport;
	i2c_port_t i2cport;
} i2ctype;

typedef struct {
	gpio_num_t pin;
	int counter;
	SemaphoreHandle_t mimutex;
	int timinter;
} argumento;

typedef struct {
	char ap[40];
	char pass[10];
} firmware_type;

enum  vmstate_type {VMOFF,VMBOOT,VMWIFI,VMLOGIN,VMREADY,VMRUN};
enum {RED,YELLOW,GREEN,LEFT,RIGHT,DONOTWALK,WALKB};
enum {CLIENT,SERVER,REPEATER};
enum {ACK,NAK,DONE,PING,PONG,QUIET,RESET,NEWID,RUN,OFF,ON,RUALIVE,IMALIVE,KILL,BLINK,LEDS,FWARE,WALK,EXECW,SENDCLONE,CLONE,LOGIN,ALARM,ACKL,EMERGENCY};
enum {BLINKc,FACTORc,PORTSc,LIGHTSc,CYCLEc,SCHEDULEc,CONNECTEDc,IDc,FIRMWAREc,LOGCLEARc,LOGc,QUIETc,TRACEc,TEMPc,STATUSc,MQTTIDc,APc,
	MODEc,PINGc,RESETc,NEWIDc,STATSc,ZEROc,DISPLAYc,SETTINGSc,RUALIVEc,STOPCYCLEc,ALIVEc,STREETc,HELPc,KALIVEc,
	BULBT,DATEc,COREc,LOGLEVELc};
typedef enum {NOREP,REPLACE} overType;
typedef enum {NODISPLAY,DISPLAYIT} displayType;
typedef enum {SYSBOOT,DLOGCLEAR,FWUPDATE,GERROR,OPENCLOSE,LOGM,DRESET,APSET,LINTERNAL,DCONTROL,DBORN,OPENABORT,DSTUCK,SLEEPMODE,ACTIVEMODE,BREAKMODE,GUARDISCO} nnada;

#define u16		uint16_t
#define u8		uint8_t
#define u32		uint32_t

typedef struct {
	 uint16_t code;
	 uint16_t code1;
} logq;


typedef struct {
	u32 centinel;
	u16 nodeId;
	u8 cmd;
	u8 towho;
	u8 fromwho;
	u8 alignn;
	u32 lapse;
	u16 seqnum;
	u16 free1,free2;
	ip4_addr_t myip;
	tcpip_adapter_ip_info_t ipstuff; //12 size
	time_t theTime;
	char buff[216]; //for whatever. Try make struct 256bytes
} cmd_struct;

typedef struct {
	u32 time,llevo;
	u16 avgTime,lastTran;
}stationGuard_struct;

typedef struct {
	u32 ioports;
	u8 opt;
	u8 typ;
	u32 valor;
	u32 inports;
} TrafficCompStruct;

typedef struct {
	u8 numcycles;
	u16 totalTime[MAXCYCLES];
	char nodeSeq[MAXCYCLES][50];
	unsigned char md5[16];
} cycle_struct;

typedef struct {
	u8 howmany;
	u8 nodeid[MAXNODES];
	u16 timeval[MAXNODES];
	u32 tranNum[MAXNODES];
} node_struct;

typedef struct {
	u8 seqNum,weekDay,cycleId;
	u32 startSeq,stopSeq;
	u16 countTimes;
} Sequence;

typedef struct{
	u8 reported;
	int8_t nodesReported[MAXNODES];
	time_t lastTime[MAXNODES];
	bool dead[MAXNODES];
} sta_status;

typedef struct {
	u8 nodel;
	u8 stationl;
	char namel[20];
	u32 timestamp;
} login_struct;

typedef struct  {
    u32 centinel;
    char ssid[2][MAXCHARS],pass[2][10],lightName[MAXCHARS];
    u8 working;
    time_t lastUpload;
    char mqtt[MAXCHARS];
    char domain[MAXCHARS];
    u16 bootcount,lastSSID;
    time_t lastTime,preLastTime,lastOpen;
    char actualVersion[20];
    char groupName[MAXCHARS];
    u16 DISPTIME;
    u16 lastResetCode;
    u16 mqttport;
    char mqttUser[MAXCHARS];
    char mqttPass[MAXCHARS];
    u16 sendMqtt;
    u16 traceflag;
    u8 mode,whoami;
    u16 nodeid;
    u16 reserved,reserved2;
    u8 clone;
    char calles[6][MAXCHARS];
    u16 free;
    u8 totalLights;
    u8 showLeds;
    u32 keepAlive;
    u8 stationid;
    char stationName[20];
    unsigned char md5[16];
} config_flash;

typedef struct {
	u8 howmany,voy;
	u8 schNum[30];
	u32 duration[30];
} scheduler_struct;

typedef struct {
    u8 numSequences;
    Sequence sequences[MAXSEQUENCES]; //repeat weekly
    unsigned char md5[16];
} sequence_struct;

typedef struct {
    // Lights for Nodes
	u8 numLuces;
    u32 outbitsPorts,lastGivenTime;
    int8_t outPorts[MAXLIGHTS];
    u8 theNames[MAXLIGHTS];
    TrafficCompStruct lasLuces[MAXLIGHTS];
	u8 defaultLight,blinkLight;
	u32 inbitsPorts;
	int8_t inPorts[MAXLIGHTS];
	u32 failed;
	unsigned char md5[16];
} lights_struct;

typedef struct {
	time_t session_start;
	u32 schedule_changes,killed;
	u16 boots;
	u32 started[MAXCYCLES][MAXNODES],confirmed[MAXCYCLES][MAXNODES],timeout[MAXCYCLES][MAXNODES];
} statistics_struct;

typedef struct {
	u16 controllerid;
	u8 node;
	u16 stationid;
	u8 seqnum;
	u16 outports,inports,delay;
} trace_struct;

typedef struct {
	u32 expected;
	u32 readleds;
} report_struct;

// Bootup sequence, WIFI related, MQTT, publishsubscribe, Mongoose, CMD like find,Web cmds,General trace,Laser stuff,DOOR STATES,
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,MONGOOSED,CMDD,WEBD,GEND,TRAFFICD,ALIVED,MQTTT,HEAPD,TIMED,SENDMD,RUND};

typedef struct { char key[10]; int val; } t_symstruct;

typedef void (*functp)(void *);
typedef esp_err_t (*url_cb)(httpd_req_t *req);

typedef struct{
    char comando[20];
    functp code;
}cmdRecord;

typedef struct{
    void *pMessage;
    uint8_t typeMsg;
    void *pComm;
}arg;

#endif
