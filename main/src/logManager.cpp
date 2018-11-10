#include "logManager.h"
#include "errno.h"

extern string makeDateString(time_t t);

void postLog(int code, int code1)
{

	logq mensaje;
	mensaje.code=code;
	mensaje.code1=code1;
	if(!llogf)
		return;

	if(logSem)
	{
		if(	xSemaphoreTake(logSem, portMAX_DELAY))
		{
			if (logQueue)
				if(!xQueueSend(logQueue,&mensaje,1000))
					printf("Error logging message %d\n",code);
			xSemaphoreGive(logSem);
			//	vTaskDelay(1000 /  portTICK_RATE_MS);
		}
	}
	else
		printf("Log Semaphore failed\n");
}

void logManager(void *pArg)
{
	logq mensaje;
	time_t t;
	while(1)
	{
		if(logQueue)
		{
			if( xQueueReceive( logQueue, &mensaje, portMAX_DELAY ) )
			{
				time(&t);
				if (llogf)
				{
				fseek(bitacora,0,SEEK_END);
				//write date
				int wr=fwrite(&t,1,4,bitacora);
				if(wr!=4)
					printf("Failedw log time\n");

				// write code
				wr=fwrite(&mensaje.code,1,2,bitacora);
				if(wr!=2)
					printf("Failedw log code\n");
				wr=fwrite(&mensaje.code1,1,2,bitacora);
				if(wr!=2)
					printf("Failedw log code1\n");


				if(sysConfig.traceflag & (1<<CMDD))
					printf("[CMDD]To write date %s code %d code1 %d\n",makeDateString(t).c_str(),mensaje.code,mensaje.code1);

				fclose(bitacora);
				if(errno!=ENOMEM)
					bitacora = fopen("/spiflash/log.txt", "r+");
				}
			}
		}
		else
			vTaskDelay(100 /  portTICK_RATE_MS);
	}

}



