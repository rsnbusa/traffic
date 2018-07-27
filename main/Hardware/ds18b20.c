/*

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "ds18b20.h"
#include "string.h"

#define SKIPROM		0xCC
#define SEARCHROM	0xF0
#define READROM		0x33
#define MATCHROM	0x55
#define ALARMSEARCH	0xEC
#define CONVERT		0x44
#define WRITESCR	0x4E
#define READSCR		0xBE
#define COPYSCR		0x48
#define RECALLE		0xB8
#define READPWR		0xB4

int DS_GPIO;
bool init=false;
int wtime=750;
uint8_t param;
uint8_t romid[8];

int romsearch( uint8_t diff, uint8_t *id)
{
	uint8_t i, j, next_diff;
	uint8_t b;

	if( !  RST_PULSE())
		return -1; // error, no device found

	dsend_byte(SEARCHROM);// ROM search command
	next_diff = 0; // unchanged on last device

	i = 64; // 8 bytes

	do
	{
		j = 8; // 8 bits
		do
		{
			b = dread(); // read bit
			ets_delay_us(1); //1us between read slots

			if( (dread()) )
			{
				if( b) // 11
					return -2; // data error
			} else
			{
				if( !b)
				{ // 00 = 2 devices
					if( diff > i || (( *id & 1) && diff != i))
					{
						b = 1; // now 1
						next_diff = i; // next pass 0
					}
				}
			}
			dsend( b); // write bit
			*id >>= 1;
			if( b)
				*id |= 0x80; // store bit

			i--;

		} while( --j);

		id++; // next byte

	} while( i);

	return next_diff; // to continue search
}


int DS_init(int GPIO, resol resolution, uint8_t *sensors){

	//DS18B20 with default wtime ms per reading.
	// so no setup of conversion time
	DS_GPIO = GPIO;
	gpio_pad_select_gpio(DS_GPIO);
	init=true;
	uint8_t aqui[8],son=0;

	switch (resolution)
	{
	case bit9: //9 bit
		wtime=100;//in ms
		param=0x1f;
		break;
	case bit10: //10 bit
		wtime=195;
		param=0x3f;
		break;
	case bit11: // 11 bit
		wtime=380;
		param=0x5f;
		break;
	case bit12://default 12bit resolution
	default:
		wtime=750;
		param=0x7f;
	}
	//Set resolution
	if(RST_PULSE())
	{
		dsend_byte(WRITESCR);
		dsend_byte(0); //temp =0
		dsend_byte(0); //temp =0
		dsend_byte(param);
		int nextt=0xff;


			while(1)
			{
			//	vTaskDelay(100 /  portTICK_RATE_MS);
				int ret=romsearch(nextt,aqui);
				if (ret>=0)
				{
				//	printf("Pos %d ",son++);
				//	for (int a=0;a<8;a++)
					//	printf("%02x",*(aqui+a));
				//	printf("\nNext:%d\n",ret);
					memcpy((sensors+son*8),aqui,8);
					son++;
					nextt=ret;
					if(ret==0)
						break;
				}
				else
				{
					printf("Rom error %d\n",ret);
					break;
				}
			}

//		printf("fueron %d \n",son);
//		RST_PULSE();
//		dsend_byte(READROM);
//		for (int a=0;a<8;a++)
//			romid[a]=dread_byte();
//		printf("Family 0x%02x Rom Id ",romid[0]);
//		for (int a=0;a<8;a++)
//			printf("%02x",romid[a]);
//		printf("\n");



	}
	else
		printf("Could not configure DS18b20 resolution. Defaults presumed\n");
	return son;
}

/// Sends one bit to bus
void dsend(char bit){
	gpio_set_level(DS_GPIO,0);
	ets_delay_us(10); //we have 15us to do it. Use 10us
	if(bit)
		gpio_set_level(DS_GPIO,1);
	//its a Zero leave it down for the time slot
	ets_delay_us(50); //10+50 =60 minimum write slot
	gpio_set_level(DS_GPIO,1);
}
// Reads one bit from bus
unsigned char dread(void){
	uint8_t PRESENCE=0;//,p2=0;
	//Line LOW, wait 1 us, High
	gpio_set_level(DS_GPIO,0);
	ets_delay_us(1);
	gpio_set_level(DS_GPIO,1);

	gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
	//read midway the reading slot 15us
	ets_delay_us(8);
	PRESENCE=gpio_get_level(DS_GPIO);
	ets_delay_us(51); //1+8+51=60 minimum read slot time
	gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
	return(PRESENCE);
}
// Sends one byte to bus
void dsend_byte(char data){
	//	gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
	//	gpio_set_direction(16, GPIO_MODE_OUTPUT);

	for(int i=0;i<8;i++)
	{
		dsend((data>>i)&1);
		if(i<7)
			ets_delay_us(1); //Delay between write slots

	}
}
// Reads one byte from bus
unsigned char dread_byte(void){
	uint8_t data = 0;
	for (int i=0;i<8;i++)
	{
		if(dread())
			data|=0x01<<i;
		ets_delay_us(1);
	}

	return(data);
}
// Sends reset pulse
unsigned char RST_PULSE(void){
	unsigned char PRESENCE;
	gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(DS_GPIO,0);
	ets_delay_us(480);
	gpio_set_level(DS_GPIO,1);
	gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
	ets_delay_us(30);//give the pull up resistor time to work
	PRESENCE=!gpio_get_level(DS_GPIO);//0 = presence
	ets_delay_us(450);
	gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
	return PRESENCE;
}
// Returns temperature from sensor
float DS_get_temp(uint8_t* romid) {
	if(init==1){
		unsigned char check;
		char temp1=0, temp2=0;
		check=RST_PULSE();
		if(check==1)
		{
			if(romid==NULL)
			{
				dsend_byte(SKIPROM);
				dsend_byte(CONVERT);
			}
			else
			{
				dsend_byte(MATCHROM);
				for (int a=0;a<8;a++)
					dsend_byte(*(romid+a));
				dsend_byte(CONVERT);
			}
			vTaskDelay(wtime / portTICK_RATE_MS);
			check=RST_PULSE();
			if(romid==NULL)
			{
				dsend_byte(SKIPROM);
				dsend_byte(READSCR);
			}
			else
			{
				dsend_byte(MATCHROM);
				for (int a=0;a<8;a++)
					dsend_byte(*(romid+a));
				dsend_byte(READSCR);
			}
			temp1=dread_byte();
			temp2=dread_byte();
			check=RST_PULSE();
			float temp=0;
			temp=(float)(temp1+(temp2*256))/16;
			return temp;

		}
		else{return 0;}

	}
	else{return 0;}
}
