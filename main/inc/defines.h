#ifndef defines_h
#define defines_h
#define DEBUGSYS

#define UDP_PORT 							333
#define MULTICAST_TTL 						5
#define MULTICAST_IPV4_ADDR 				"232.10.11.12"

#define SENDLED								23
#define RXLED								22
#define EVERYBODY 							255

#define THECENTINEL 						0xFAFBFCFD

#define FOREVER								true
#define MINHEAP								20000
#define NKEYS 								24
#define KCMDS								34
#define MAXLOGINTIME						60   //one minute

#define UIO									5  //GMT local time

#define MG_LISTEN_ADDR						"80"
#define MG_TASK_STACK_SIZE 					4096 /* bytes */
#define MGOS_TASK_PRIORITY 					1
#define BLINKT 								100

#define BUFFSIZE 							4096
#define TEXT_BUFFSIZE 						4096
#define FW_SERVER_IP   						"185.176.43.60"
#define FW_SERVER_PORT 						"80"
#define FW_FILENAME 						"http://feediot.co.nf/TrafficIoT.bin"

#define MQTTPORT                   			18247

// pins
#define SDAW                				GPIO_NUM_18      // SDA
#define SCLW                				GPIO_NUM_19       // SCL for Wire service
#define WIFILED 							GPIO_NUM_5
#define DSPIN 								GPIO_NUM_15
#define MQTTLED								GPIO_NUM_17
#define CENTINEL            				0x12112299  //our chip id
#define MAXCHARS            				40
#define DISPMNGR		     				100

#define YB                 					15
#define WB                 					4

#define RSSIVAL             				15
#define ERRORAUTH           				3
#define MINFO								6

#define MAXCYCLES							10   //change here will require to ERASE flash etc due to record size change
#define MAXNODES							10   //for cycle in memory max number of events in ONE cycle.
#define MAXSEQUENCES						30	 //change here will require to ERASE flash etc due to record size change
#define MAXLIGHTS							6    //change here will require to ERASE flash etc due to record size change
#define NUMCOLORS							7
#define MAXCMDS             				7
#define MAXDEVS             				3
#define u8									uint8_t
#define u16									uint16_t
#define u32									uint32_t

#define DEFAULT_VREF   						3300
#define SAMPLES  							64          //Multisampling
#define ADCCHAN								ADC_CHANNEL_7;     //GPIO35 if ADC1
#define ADC_COUNTS  						(1<<12)

#endif
