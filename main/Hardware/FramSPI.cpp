#include <stdint.h>
#include "framDef.h"
#include "FramSPI.h"
extern "C"{
#include "driver/spi_master.h"
#include "esp_log.h"
}
static const char TAG[]="RSNSPI";
extern uint8_t *bbuffer;
extern void delay(uint16_t a);
//extern SemaphoreHandle_t *framSem;


/*========================================================================*/
/*                            CONSTRUCTORS                                */
/*========================================================================*/

/**************************************************************************/
/*!
 Constructor
 */
/**************************************************************************/
FramSPI::FramSPI(void)
{
	_framInitialised = false;
	spi =NULL;
}

/*========================================================================*/
/*                           PUBLIC FUNCTIONS                             */
/*========================================================================*/

/**************************************************************************/
/*!
 Send SPI Cmd. JUST the cmd.
 */
/**************************************************************************/
int  FramSPI::sendCmd (uint8_t cmd)
{
	esp_err_t ret;
	spi_transaction_t t;
	//    uint8_t data;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	//    data=cmd;
	t.length=8;                     //Command is 8 bits
	//    t.tx_buffer=&data;
	t.tx_buffer=&cmd;
	//    t.rxlength=0;
	ret=spi_device_transmit(spi, &t);  //Transmit!
	return ret;

}


/**************************************************************************/
/*!
 Status SPI Cmd.
 */
/**************************************************************************/
int  FramSPI::readStatus ( uint8_t* donde)
{
	esp_err_t ret;
	spi_transaction_t t;
	uint8_t data;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	data=MBRSPI_RDSR;
	t.length=8;                     //Command is 8 bits
	t.tx_buffer=&data;
	t.rxlength=8;
	t.rx_buffer=donde;
	ret=spi_device_transmit(spi, &t);  //Transmit!
	return ret;
}

int  FramSPI::writeStatus ( uint8_t streg)
{
	esp_err_t ret;
	spi_transaction_t t;
	uint8_t data[2];
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	data[0]=MBRSPI_WRSR;
	data[1]=streg;
	t.length=16;                     //Command is 8 bits
	t.tx_buffer=&data;
	//   t.rxlength=8;
	//  t.rx_buffer=donde;
	ret=spi_device_transmit(spi, &t);  //Transmit!
	return ret;
}
//bool FramSPI::begin(spi_device_handle_t spic,uint16_t *prod)
bool FramSPI::begin(int MOSI, int MISO, int CLK, int CS,SemaphoreHandle_t *framSem)
//bool FramSPI::begin(int MOSI, int MISO, int CLK, int CS)
{
	int ret;
	spi_bus_config_t 				buscfg;
	spi_device_interface_config_t 	devcfg;
	memset(&buscfg,0,sizeof(buscfg));
	memset(&devcfg,0,sizeof(devcfg));
	buscfg.mosi_io_num=MOSI;
	buscfg .miso_io_num=MISO;
	buscfg.sclk_io_num=CLK;
	buscfg.quadwp_io_num=-1;
	buscfg .quadhd_io_num=-1;

	//Initialize the SPI bus
	ret=spi_bus_initialize(VSPI_HOST, &buscfg, 0);
	assert(ret == ESP_OK);

	devcfg .clock_speed_hz=20000000;              	//Clock out at 20 MHz
	devcfg.mode=0;                                	//SPI mode 0
	devcfg.spics_io_num=CS;               			//CS pin
	devcfg.queue_size=7;                         	//We want to be able to queue 7 transactions at a time
	devcfg.flags=SPI_DEVICE_HALFDUPLEX;
	//devcfg.flags=0;

	ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
	if (ret==ESP_OK)
	{
		/*
	if(spic==NULL)
	{
		return false;
	}
	spi=spic; //save our port to work with
	//We assume spic channel has been setup and spi is the pointer to the port !!!!!!!! MOAFU Warning
		 */
		/* Make sure we're actually connected */
		uint16_t manufID,prod;
	//	uint8_t str;
		getDeviceID(&manufID, &prod);
		if (manufID != 0x47f)
			//{
			ESP_LOGI(TAG , "failed %d",manufID);
		//  return false;
		//}
		ESP_LOGI(TAG ,"Product %x",prod);
		printf("Fram product %x\n",prod);
		//Set write enable after chip is identified
		switch(prod)
		{
		case 0x409:
			addressBytes=2;
			intframWords=16384;
			break;
		case 0x509:
			addressBytes=2;
			intframWords=32768;
			break;
		case 0x2603:
			addressBytes=2;
			intframWords=65536;
			break;
		case 0x2703:
			addressBytes=3;
			intframWords=131072;
			break;
		default:
			addressBytes=2;
			intframWords=0;
		}

		_framInitialised = true;
		*framSem= xSemaphoreCreateBinary();
		if(*framSem)
			xSemaphoreGive(*framSem);  //SUPER important else its born locked
		else
			printf("Cant allocate Fram Sem\n");
		return true;
	}
	return false;
}

int FramSPI::writeMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	spi_transaction_t t;
	esp_err_t ret=0;
	uint8_t datos[4],st=0;
	int stcount,count,fueron; //retries

	memset(&t,0,sizeof(t));
//	printf("Many start %d addr %d\n",son,framAddr);
	count=son;
	while(count>0)
	{
		st=0;
		stcount=20;
		while(stcount>0 && st!=2)
		{
			readStatus(&st);
			st=st&2; //Were are looking for bit 2 only
			if(st!=2)
			{
				sendCmd(MBRSPI_WREN);
				stcount--;
				//	delay(1);
			}
		}
		if ((stcount<1) && (st != 2))
		{
			printf("SPI Read Status Timeout %d\n",count);
			return -1; // error internal cant get a valid status. Defective chip or whatever
		}

		datos[0]=MBRSPI_WRITE;
		if(addressBytes==2)
		{
			datos[1]=(framAddr & 0xff00)>>8;
			datos[2]=framAddr& 0xff;
			fueron=count>29?29:count;
			t.length=(fueron+3)*8;                     //Command is  (bytes+3) *8 =32 bits
			memcpy(bbuffer,datos,3);
			memcpy(&bbuffer[3],valores,fueron); //should check that son is less or equal to bbuffer size
		}
		else
		{
			datos[1]=(framAddr & 0xff0000)>>16;
			datos[2]=(framAddr & 0xff00)>>8;
			datos[3]=framAddr & 0xff;
			fueron=count>28?28:count;
			t.length=(fueron+4)*8;                     //Command is  (bytes+3) *8 =32 bits
			memcpy(bbuffer,datos,4);
			memcpy(&bbuffer[4],valores,fueron);
		}

		t.tx_buffer=bbuffer;
		ret=spi_device_transmit(spi, &t);  //Transmit!
		count-=fueron;
		framAddr+=fueron;
	}
	return ret;
}

/*
  int FramSPI::writeMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	spi_transaction_t t;
	esp_err_t ret;
	uint8_t datos[4],st=0;
	int count=20; //retries
//	count=20;
//	st=0;
	memset(&t,0,sizeof(t));

	while(count>0 && st!=2)
	{
		readStatus(&st);
		st=st&2; //Were are looking for bit 2 only
		if(st!=2)
		{
			sendCmd(MBRSPI_WREN);
			count--;
		//	delay(1);
		}
	}
	if ((count<1) && (st != 2))
	{
		printf("SPI Read Status Timeout %d\n",count);
		return -1; // error internal cant get a valid status. Defective chip or whatever
	}

	datos[0]=MBRSPI_WRITE;
	if(addressBytes==2)
	{
		datos[1]=(framAddr & 0xff00)>>8;
		datos[2]=framAddr& 0xff;
		t.length=(son+3)*8;                     //Command is  (bytes+3) *8 =32 bits
		memcpy(bbuffer,datos,3);
		memcpy(&bbuffer[3],valores,son); //should check that son is less or equal to bbuffer size
	}
	else
	{
		datos[1]=(framAddr & 0xff0000)>>16;
		datos[2]=(framAddr & 0xff00)>>8;
		datos[3]=framAddr & 0xff;
		t.length=(son+4)*8;                     //Command is  (bytes+3) *8 =32 bits
		memcpy(bbuffer,datos,4);
		memcpy(&bbuffer[4],valores,son);
	}

	printf("Many %d son %d\n",t.length,son);
	t.tx_buffer=bbuffer;
	ret=spi_device_transmit(spi, &t);  //Transmit!
	return ret;
}
 */



int FramSPI::format(uint8_t valor, uint8_t *buffer,uint32_t len)
{
	uint32_t add=0;
	int count=intframWords,ret;
	//add=0;
	while (count>0)
	{
		if (count>len)
		{

			//	printf("Format add %d len %d count %d\n",add,len,count);
			memset(buffer,valor,len);  //Should be done only once
			ret=writeMany(add,buffer,len);
			if (ret!=0)
				return ret;
		}
		else
		{
			//	printf("FinalFormat add %d len %d\n",add,count);
			memset(buffer,valor,count);
			ret=writeMany(add,buffer,count);
			if (ret!=0)
				return ret;
		}
		count-=len;
		add+=len;
		delay(5);
	}
	return ESP_OK;
}

int FramSPI::formatSlow(uint8_t valor)
{
	uint32_t add=0;
	int count=intframWords;
	while (count>0)
	{
		write8(add,valor);
		count--;
		add++;
	}
	return ESP_OK;
}


int FramSPI::formatMeter(uint8_t cual, uint8_t *buffer,uint16_t len)
{
	uint32_t add;
	int count=DATAEND-BEATSTART,ret;
	add=count*cual+100;
	while (count>0)
	{
		if (count>len)
		{
			memset(buffer,0,len);
			ret=writeMany(add,buffer,len);
			if (ret!=0)
				return ret;
		}
		else
		{
			memset(buffer,0,count);
			ret=writeMany(add,buffer,count);
			if (ret!=0)
				return ret;
		}
		count-=len;
		add+=len;
		delay(2);
	}
	return ESP_OK;

}

int FramSPI::readMany (uint32_t framAddr, uint8_t *valores,uint32_t son)
{
	esp_err_t ret=0;
	spi_transaction_t t;
	uint8_t tx[4];
	int cuantos,rlen;
	memset(&t, 0, sizeof(t));
	tx[0]=MBRSPI_READ;
	if(son<33)
	{
		if(addressBytes==2)
		{
			tx[1]=(framAddr & 0xff00)>>8;
			tx[2]=framAddr & 0xff;
			t.length=24;
		}
		else
		{
			tx[1]=(framAddr & 0xff0000) >>16;
			tx[2]=(framAddr & 0xff00)>>8;
			tx[3]=framAddr & 0xff;
			t.length=32;
		}
		t.tx_buffer=&tx;
		t.rxlength=son*8;

		t.rx_buffer=bbuffer; // MUST MUST use this buffer since its a dma alloc buffer. else data gets lost/misplaced.
		ret=spi_device_transmit(spi, &t);
		memcpy(valores,bbuffer,son); //move back to rx buffer
		return ret;
	}
	else
	{

		cuantos=son;
		while(cuantos>0)
		{
			memset(&t, 0, sizeof(t));
			if(addressBytes==2)
			{
				tx[1]=(framAddr & 0xff00)>>8;
				tx[2]=framAddr & 0xff;
				t.length=24;
			}
			else
			{
				tx[1]=(framAddr & 0xff0000) >>16;
				tx[2]=(framAddr & 0xff00)>>8;
				tx[3]=framAddr & 0xff;
				t.length=32;
			}
			t.tx_buffer=&tx;
			rlen=cuantos>32?32*8:cuantos*8;
			t.rxlength=rlen;
			t.rx_buffer=bbuffer;
			ret=spi_device_transmit(spi, &t);
			memcpy(valores,bbuffer,rlen/8); //move back to rx buffer
			cuantos-=rlen/8;
			framAddr+=rlen/8;
			valores+=rlen/8;
		}
	}
	return ret;
}

int FramSPI::write8 (uint32_t framAddr, uint8_t value)
{

	esp_err_t ret;
	spi_transaction_t t;
	uint8_t data[5],st=0;
	int count=20; //retries

	memset(&t,0,sizeof(t));
	while(count>0 && st!=2)
	{
		readStatus(&st);
		st=st&2;
		if(st!=2)
		{
			sendCmd(MBRSPI_WREN);
			count--;
			delay(1);
		}
	}
	if (count==0)
		return -1; // error internal cant get a valid status. Defective chip or whatever

	data[0]=MBRSPI_WRITE;
	if(addressBytes==2)
	{
		data[1]=(framAddr &0xff00)>>8;
		data[2]=framAddr& 0xff;
		data[3]=value;
		t.length=32;                     //Command is 4 bytes *8 =32 bits
	}
	else
	{
		data[1]=(framAddr & 0xff0000)>>16;
		data[2]=(framAddr& 0xff00)>>8;
		data[3]=framAddr& 0xff;
		data[4]=value;
		t.length=40;                     //Command is 5 bytes *8 =402 bits
	}

	t.tx_buffer=&data;
	t.rxlength=0;
	t.rx_buffer=NULL;
	ret=spi_device_transmit(spi, &t);  //Transmit!
	return ret;
}

int FramSPI::read8 (uint32_t framAddr,uint8_t *donde)
{
	spi_transaction_t t;
	uint8_t data[4];
	int ret;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	data[0]=MBRSPI_READ;
	if(addressBytes==2)
	{
		data[1]=(framAddr &0xff00)>>8;
		data[2]=framAddr& 0xff;
		t.length=24;                     //Command is 3 btyes *8 =24 bits
	}
	else
	{
		data[1]=(framAddr & 0xff0000)>>16;
		data[2]=(framAddr& 0xff00)>>8;
		data[3]=framAddr& 0xff;
		t.length=32;                     //Command is 4 btyes *8 =32 bits
	}

	t.rxlength=8;
	t.rx_buffer=donde;
	t.tx_buffer=&data;
	ret=spi_device_transmit(spi, &t);  //Transmit!
	return ret;


}

/**************************************************************************/
/*!
 @brief  Reads the Manufacturer ID and the Product ID frm the IC

 @params[out]  manufacturerID
 The 12-bit manufacturer ID (Fujitsu = 0x00A)
 @params[out]  productID
 The memory density (bytes 11..8) and proprietary
 Product ID fields (bytes 7..0). Should be 0x510 for
 the MB85RC256V.
 */
/**************************************************************************/
void FramSPI::getDeviceID(uint16_t *manufacturerID, uint16_t *productID)
{
	uint8_t aqui[4] = { 0 };
	//  ESP_LOGI(TAG , "read device");

	spi_transaction_t t;
	uint8_t data;
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	data=MBRSPI_RDID;
	t.length=8;                     //Command is 4 byes *8 =32 bits
	t.tx_buffer=&data;
	t.rxlength=32;
	t.rx_buffer=&aqui;
	spi_device_transmit(spi, &t);  //Transmit!
	//   ESP_LOGI(TAG , "Id %d %x %x %x",aqui[0],aqui[1],aqui[2],aqui[3]);


	// Shift values to separate manuf and prod IDs
	// See p.10 of http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf
	*manufacturerID=(aqui[0]<<8)+aqui[1];
	*productID=(aqui[2]<<8)+aqui[3];

}

uint16_t date2daysSPI(uint16_t y, uint8_t m, uint8_t d) {
	uint8_t daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };//offsets 0,31,59,90,120,151,181,212,243,273,304,334, +1 if leap year
	uint16_t days = d;
	for (uint8_t i = 0; i < m; i++)
		days += daysInMonth[ i];
	if (m > 1 && y % 4 == 0)
		++days;
	return days ;

}

int FramSPI::write_tarif_bytes(uint32_t add,uint8_t*  desde,uint32_t cuantos)
{
	int ret;
	add+=BPH;
	ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::read_tarif_bytes(uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	int ret;
	add+=BPH;
	ret=readMany(add,donde,cuantos);
	return ret;
}

int FramSPI::read_tarif_bpw(uint8_t tarNum, uint8_t*  donde)
{
	int ret;
	uint32_t add=tarNum*2;
	ret=read_tarif_bytes(add,donde,2);
	return ret;
}

int FramSPI::write_tarif_bpw(uint8_t tarNum, uint16_t valor)
{
	int ret;
	uint32_t add=tarNum*2;
	ret=write_tarif_bytes(add,(uint8_t* )&valor,2);
	return ret;
}

int FramSPI::read_tarif_day(uint16_t dia,uint8_t*  donde) //Read 24 Hours of current Day(0-30)
{
	int ret;
	uint32_t add=TARIFADIA+dia*24;
	ret=read_tarif_bytes(add,donde,24);
	return ret;
}

int FramSPI::read_tarif_hour(uint16_t dia,uint8_t hora,uint8_t*  donde) //Read specific Hour in a Day. Day 0-30(31 days) and Hour 0-23
{
	int ret;
	uint32_t add=TARIFADIA+dia*24+hora;
	ret=read_tarif_bytes(add,donde,1);
	return ret;
}

// Meter Data Management

int FramSPI::write_bytes(uint8_t meter,uint32_t add,uint8_t*  desde,uint32_t cuantos)
{
	int ret;
	add+=DATAEND*meter+SCRATCH;
	ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::read_bytes(uint8_t meter,uint32_t add,uint8_t*  donde,uint32_t cuantos)
{
	add+=DATAEND*meter+SCRATCH;
	int ret;
	ret=readMany(add,donde,cuantos);
	return ret;
}

int FramSPI::write_recover(scratchTypespi value)
{
	uint32_t add=SCRATCHOFF;
	//  PRINT_MSG("State %d Meter %d Life %x\n",value.state,value.meter,value.life);

	uint8_t*  desde=(uint8_t* )&value;
	uint8_t cuantos=sizeof(scratchTypespi);
	int ret=writeMany(add,desde,cuantos);
	return ret;
}

int FramSPI::write_beat(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=BEATSTART;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LONG);
	return ret;
}

int FramSPI::write_corte(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=FECHACORTADO;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LONG);
	return ret;
}

int FramSPI::write_lifedate(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LONG);
	return ret;
}

int FramSPI::write_lifekwh(uint8_t medidor, uint32_t value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LONG);
	return ret;
}

int FramSPI::write_pago(uint8_t medidor, float value)
{
	int ret;
	uint32_t badd=VALORPAGO;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LONG);
	return ret;
}

int FramSPI::write_month(uint8_t medidor,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*WORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,WORD);
	return ret;
}

int FramSPI::write_minamps(uint8_t medidor,uint16_t value)
{
	int ret;
	uint32_t badd=MINASTART;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,WORD);
	return ret;
}

int FramSPI::write_maxamps(uint8_t medidor,uint16_t value)
{
	int ret;
	uint32_t badd=MAXASTART;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,WORD);
	return ret;
}

int FramSPI::write_cycle(uint8_t medidor,uint8_t month,uint16_t value)
{
	int ret;
	uint32_t badd=CYCLECOUNT+month*WORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,WORD);
	return ret;
}

int  FramSPI::write_cycledate(uint8_t medidor,uint8_t month,uint32_t value)
{
	int ret;
	uint32_t badd=CYCLEDATE+month*LONG;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,LONG);
	return ret;
}

int FramSPI::write_day(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint16_t value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYSTART+days*WORD;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,WORD);
	return ret;
}

int FramSPI::write_hour(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURSTART+(days*24)+hora;
	ret=write_bytes(medidor,badd,(uint8_t* )&value,1);
	return ret;
}

int FramSPI::read_lifedate(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEDATE;
	ret=read_bytes(medidor,badd,value,LONG);
	return ret;
}

int FramSPI::read_recover(scratchTypespi* aqui)
{
	int ret;
	uint8_t*  donde=(uint8_t* )aqui;
	uint16_t cuantos = sizeof(scratchTypespi);
	uint32_t add=SCRATCHOFF;
	ret=readMany(add,donde,cuantos);
	if (ret!=0)
		printf("Read REcover error %d\n",ret);
	return ret;
}


int FramSPI::read_lifekwh(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=LIFEKWH;
	ret=read_bytes(medidor,badd,value,LONG);
	return ret;
}

int FramSPI::read_beat(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=BEATSTART;
	ret=read_bytes(medidor,badd,value,LONG);
	return ret;
}

int FramSPI::read_corte(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=FECHACORTADO;
	ret=read_bytes(medidor,badd,value,LONG);
	return ret;
}

int FramSPI::read_pago(uint8_t medidor, uint8_t*  value)
{
	int ret;
	uint32_t badd=VALORPAGO;
	ret=read_bytes(medidor,badd,value,LONG);
	return ret;
}


int FramSPI::read_minamps(uint8_t medidor,uint8_t*  value)
{
	int ret;
	uint32_t badd=MINASTART;
	ret=read_bytes(medidor,badd,value,WORD);
	return ret;
}

int FramSPI::read_maxamps(uint8_t medidor,uint8_t*  value)
{
	int ret;
	uint32_t badd=MAXASTART;
	ret=read_bytes(medidor,badd,value,WORD);
	return ret;
}


int FramSPI::read_month(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=MONTHSTART+month*WORD;
	ret=read_bytes(medidor,badd,value,WORD);
	return ret;
}

int FramSPI::read_day(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t*  value)
{
	int ret;
	int days=date2daysSPI(yearl,month,dia);
	uint32_t badd=DAYSTART+days*WORD;
	ret=read_bytes(medidor,badd,value,WORD);
	return ret;
}

int FramSPI::read_hour(uint8_t medidor,uint16_t yearl,uint8_t month,uint8_t dia,uint8_t hora,uint8_t*  value)
{
	int ret;
	uint16_t days=date2daysSPI(yearl,month,dia);
	uint32_t badd=HOURSTART+(days*24)+hora;
	ret=read_bytes(medidor,badd,value,1);
	return ret;
}

int FramSPI::read_cycle(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=CYCLECOUNT+month*WORD;
	ret=read_bytes(medidor,badd,value,WORD);
	return ret;
}

int FramSPI::read_cycledate(uint8_t medidor,uint8_t month,uint8_t*  value)
{
	int ret;
	uint32_t badd=CYCLEDATE+month*LONG;
	ret=read_bytes(medidor,badd,value,LONG);
	return ret;
}


