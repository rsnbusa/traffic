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

enum {START,STOP,ACK,NAK,DONE,PING,PONG,SENDC,COUNTERS,TEST,INTERVAL,DELAY,QUIET,RESETC,RESET,NEWID,RUN,OFF,ON,RUALIVE,IMALIVE,KILL};

typedef enum {NOTSENT,SENT} sendType;
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
	u16 lapse;
	u16 seqnum;
	u16 free1,free2;
	ip4_addr_t myip;
	tcpip_adapter_ip_info_t ipstuff; //12 size
	time_t theTime;
	char buff[218]; //for whatever. Try make struct 256bytes
} cmd_struct;

typedef struct {
	u32 ioports;
	u8 opt;
	u8 typ;
	u32 valor;
} TrafficCompStruct;

typedef struct {
	u8 numcycles;
	u16 totalTime[30];
	char nodeSeq[30][50];
} cycle_struct;

typedef struct {
	u8 howmany;
	u8 nodeid[10];
	u16 timeval[10];
} node_struct;

typedef struct {
	u8 seqNum,weekDay,cycleId;
	u32 startSeq,stopSeq;
	u16 countTimes;
} Sequence;

typedef struct{
	u8 reported;
	int8_t nodesReported[20];
	time_t lastTime[20];
} sta_status;

typedef struct  {
    u32 centinel;
    char ssid[2][MAXCHARS],pass[2][10],meterName[MAXCHARS];
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
    u16 keepAlive;
} config_flash;

typedef struct {
	u8 howmany,voy;
	u8 seqNum[30];
	u32 duration[30];
} scheduler_struct;

typedef struct {
    u8 numSequences;
    Sequence sequences[30]; //repeat weekly
} sequence_struct;

typedef struct {
    // Lights for Nodes
	u8 numLuces;
    u32 allbitsPort,lastGivenTime;
    int8_t thePorts[6];
    TrafficCompStruct lasLuces[6];
	u8 defaultLight;
} lights_struct;

// Bootup sequence, WIFI related, MQTT, publishsubscribe, Mongoose, CMD like find,Web cmds,General trace,Laser stuff,DOOR STATES,
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,MONGOOSED,CMDD,WEBD,GEND,TRAFFICD,ALIVED,MQTTT,HEAPD};

typedef struct { char key[10]; int val; } t_symstruct;

typedef void (*functrsn)(void *);

typedef struct{
    char comando[20];
    functrsn code;
}cmdRecord;

typedef struct{
    void *pMessage;
    uint8_t typeMsg;
    void *pComm;
}arg;

#endif
