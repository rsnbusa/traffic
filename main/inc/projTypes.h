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
    int type;                  /*!< event type */
    int group;                 /*!< timer group */
    int idx;                   /*!< timer number */
    uint64_t counter_val;      /*!< timer counter value */
    double time_sec;           /*!< calculated time from counter value */
} timer_event_t;

typedef struct {
    int 							pin;                  /*!< event type */
    SemaphoreHandle_t				mimutex;
    uint32_t 						timestamp;                 // milliseconds
} interrupt_type;

enum {START,STOP,ACK,NAK,DONE,PING,PONG,SENDC,COUNTERS,TEST,INTERVAL,DELAY,QUIET,RESETC,RESET,NEWID};

typedef enum {CLOSED,OPENING,OPENED,CLOSING,UNKNOWN,TIMERSTATE,GFAULT,VOLTS} stateType;
typedef enum {ONCE,TIMER,REPEAT,TIMEREPEAT} resetType;
typedef enum {NOTSENT,SENT} sendType;
typedef enum {NOREP,REPLACE} overType;
typedef enum {NODISPLAY,DISPLAYIT} displayType;
typedef enum {DISPLAYPULSES,DISPLAYKWH,DISPLAYUSER,DISPLAYMQTT,DISPLAYMONTH,DISPLAYDAY,DISPLAYHOUR} displayModeType;
typedef enum {NORTC,LOGCLEAR,UPDATED,UPDATEFAIL} alertId;
typedef enum {SYSBOOT,DLOGCLEAR,FWUPDATE,GERROR,OPENCLOSE,LOGM,DRESET,APSET,LINTERNAL,DCONTROL,DBORN,OPENABORT,DSTUCK,SLEEPMODE,ACTIVEMODE,BREAKMODE,GUARDISCO} nnada;
#define u16		uint16_t
#define u8		uint8_t
#define u32		uint32_t

typedef struct {
	 uint16_t code;
	 uint16_t code1;
} logq;

typedef struct{
    u16      state;
    u16      meter;
    u32      life;
    u16      month;
    u16      day;
    u16      cycle;
    u16      hora;
    u16      mesg,diag,horag;
    u16      yearg;
} scratchType;

typedef struct {
    resetType resendType;
    char alertName[MAXCHARS];
    sendType status;
    bool retain;
    u32 countLimit,counter;
    time_t whenLast;
} alertType;

typedef struct {
    u32 curBeat,curLife,curCycle,date;
    u16 curMonth,curDay,bpk;
    u8 curHour,state;
    char mid[MAXCHARS];
} rawStatus;

typedef struct {
	u32 centinel;
	u8 cmd;
	u8 towho;
	u8 fromwho;
	u8 alignn;
	u16 lapse;
	u16 seqnum;
	u16 free1,free2;
	tcpip_adapter_ip_info_t ipstuff; //12 size
	char buff[228]; //for whatever. Try make struct 256bytes
} cmd_struct;

typedef struct  {
    u32 centinel;
    char ssid[5][MAXCHARS],pass[5][10],meterName[MAXCHARS];
    u8 working;
    time_t lastUpload;
    char email [MAXEMAILS][MAXCHARS];
    char emailName[MAXEMAILS][30];
    u8 except[MAXEMAILS];
    char mqtt[MAXCHARS];
    char domain[MAXCHARS];
    u16 ecount;
    u16 bootcount;
    time_t lastTime,preLastTime,lastOpen;
    char actualVersion[20];
    char groupName[MAXCHARS];
    u16 DISPTIME;
    u16 lastResetCode;
    u16 mqttport;
    char mqttUser[MAXCHARS];
    char mqttPass[MAXCHARS];
    u16 opens,stucks;
    u32 elapsedCycle;
    u16 guards,aborted;
    u16 relay,wait,sleepTime,openTimeout,closeTimeout,lastSSID,menos,guardOn;
    u32 totalCycles;
    u16 countCycles;
    u16 sendMqtt;
    u16 waitBreak;
    u16 traceflag;
    u16 motorw;
    u8 mode,whoami;
} config_flash;

// Bootup sequence, WIFI related, MQTT, publishsubscribe, Mongoose, CMD like find,Web cmds,General trace,Laser stuff,DOOR STATES,
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,MONGOOSED,CMDD,WEBD,GEND,LASERD,DOORD,MQTTT,HEAPD};

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
