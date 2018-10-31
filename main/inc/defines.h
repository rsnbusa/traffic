#ifndef defines_h
#define defines_h

#define EXAMPLE_WIFI_SSID 	"taller"
#define EXAMPLE_WIFI_PASS 	"csttpstt"

#define UDP_PORT 			333
#define MULTICAST_TTL 		5
#define MULTICAST_IPV4_ADDR "232.10.11.12"

#define SENDLED				23
#define RXLED				22
#define EVERYBODY 			255

#define THECENTINEL 		0xFAFBFCFD

#define FOREVER								true
#define MINHEAP								20000
#define NKEYS 								24

#define MG_LISTEN_ADDR						"80"
#define MG_TASK_STACK_SIZE 					4096 /* bytes */
#define MGOS_TASK_PRIORITY 					1
//#define AP_CHAN 							9
#define BLINKT 								100
#define BLINKPR 							100

#define CONFIG_SECTOR        				0xc000/ SPI_FLASH_SEC_SIZE
#define CONFIG_ADDRESS       				0xc000

#define BUFFSIZE 							4096
#define TEXT_BUFFSIZE 						4096
#define FW_SERVER_IP   						"185.176.43.60"
#define FW_SERVER_PORT 						"80"
#define FW_FILENAME 						"http://feediot.co.nf/GarageIoT.bin"

#define MQTTPORT                   			18747

// pins
#define SDAW                				GPIO_NUM_18      // SDA
#define SCLW                				GPIO_NUM_19       // SCL for Wire service
#define OPENSW								GPIO_NUM_27
#define CLOSESW								GPIO_NUM_26
#define LASERSW								GPIO_NUM_25
#define RELAY								GPIO_NUM_33     // Relay
#define LASER								GPIO_NUM_14		//Laser On Off
#define WIFILED 							GPIO_NUM_23
#define DOORLED								GPIO_NUM_17     // DoorLed
#define DSPIN 								GPIO_NUM_22
#define MQTTLED								GPIO_NUM_21


#define LASERON								1
#define LASEROFF							0

// Varios
#define MAXEMAILS           				3
#define CENTINEL            				0x12112299  //our chip id
#define TIMECHECK           				1000
#define MAXRETRIESGUARD						5
#define ALERT_TYPE          				0
#define ERROR_TYPE          				1
#define MAXCHARS            				40
#define VERSION             				"3.0.0" // May 7/2017 Version 1
#define NOALERT            					0
#define NOMQTT              				1
#define INITLOG             				2
#define GENERAL             				255
#define MQTTCONN            				3
#define FIRMWARE            				4

// alert errors
#define MAXALERTS           				15
#define MAXHTTP             				2000
#define DISPMNGR		     				100

#define YB                 					15
#define WB                 					4
#define RSSIVAL             				15

#define GUARDX								106
#define GUARDY								0
#define RELAYX								118
#define RELAYY								0

#define NOERROR             				0
#define ERROROB             				1
#define ERRORFULL           				2
#define ERRORAUTH           				3
#define ERRORCMD            				4
#define MSTATUS								5
#define MINFO								6
#define MSHOW								8
#define MSLEEP								7
#define MOPEN								8
#define MCLOSED								9
#define MOPENINING							10
#define MCLOSING							11

#define HIDESSID            				false // used to received internal commands Strategy
#define MAXCMDS             				17
#define MAXDEVS             				3
#define MINELAPSEDAMPS      				100

#define RTCTIME             				60000  //every minute

#define PRINT_MSG							printf
#define u8									uint8_t
#define u16									uint16_t
#define u32									uint32_t

#define DEFAULT_VREF   						3300
#define SAMPLES  							64          //Multisampling
#define ADCCHAN								ADC_CHANNEL_7;     //GPIO35 if ADC1
#define ADC_COUNTS  						(1<<12)

#endif
