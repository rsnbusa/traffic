/*
 * TrafficComp.cpp
 *
 *  Created on: Oct 31, 2018
 *      Author: rsn
 */

#include "TrafficComp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

TrafficComp::TrafficComp() {
	ioports=0;
	options=0;
	typ=0;
	val=0;
}

TrafficComp::~TrafficComp() {
	// TODO Auto-generated destructor stub
}

void TrafficComp::setComp(uint32_t io, uint8_t opt, uint8_t ty, uint32_t value)
{
	this->ioports=io;
	this->options=opt;
	this->typ=ty;
	this->val=value;
}

void TrafficComp::playComponent(uint16_t time,uint32_t allbits)
{
	int espera=0;
	REG_WRITE(GPIO_OUT_W1TC_REG,allbits);
	REG_WRITE(GPIO_OUT_W1TS_REG,ioports);
	if(typ==1)
		espera=val;
	else
		espera=val*time/100;
	vTaskDelay(espera / portTICK_PERIOD_MS);
}
