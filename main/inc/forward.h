#ifndef fw
#define fw
void set_FirmUpdateCmd(void *pArg);               // FOTA of latest firmware
void logManager(void *arg);
void sendMsg(int cmd,int aquien,int f1,int f2,char * que,int len);
static void mcast_example_task(void *pvParameters);
#endif
