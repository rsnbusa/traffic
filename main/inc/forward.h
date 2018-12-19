#ifndef fw
#define fw
void set_FirmUpdateCmd(void *pArg);               // FOTA of latest firmware
void logManager(void *arg);
void sendMsg(int cmd,int aquien,int f1,int f2,char * que,int len);
void mcast_example_task(void *pvParameters);
void set_test(void *pArg);               // FOTA of latest firmware
void set_OnOff(void *pArg);               // FOTA of latest firmware
void set_blink(void *pArg);               // FOTA of latest firmware
void set_reset(void *pArg);               // FOTA of latest firmware
void set_run(void *pArg);               // FOTA of latest firmware
void set_leds(void *pArg);               // FOTA of latest firmware
void set_walk(void *pArg);               // FOTA of latest firmware
void set_emergency(void *pArg);               // FOTA of latest firmware

#endif
