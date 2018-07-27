/*
 * I2C.cpp
 *
 *  Created on: Feb 24, 2017
 *      Author: kolban
 */
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_err.h>
#include <stdint.h>
#include <sys/types.h>
#include "I2C.h"
#include "sdkconfig.h"
#include <esp_log.h>

static bool driverInstalled = false;

/**
 * @brief Create an instance of an %I2C object.
 *
 * @param[in] sdaPin The pin number used for the SDA line.
 * @param[in] sclPin The pin number used for the SCL line.
 */
I2C::I2C() {
	directionKnown = false;
	address = 0;
	cmd = 0;
	portNum=(i2c_port_t)0;
}

void I2C::beginTransaction() {
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	directionKnown = false;
}

int I2C::endTransaction() {
	i2c_master_stop(cmd);
	int ret=i2c_master_cmd_begin(I2C_NUM_0, cmd,1000/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

void I2C::write(uint8_t byte, bool ack) {
	if (directionKnown == false) {
		directionKnown = true;
		i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, 1);
	}
	i2c_master_write_byte(cmd, byte, ack);
}

void I2C::write(uint8_t *bytes, size_t length, bool ack) {
	if (directionKnown == false) {
		directionKnown = true;
		i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, 1); //By definiton MUST be ack
	}
	i2c_master_write(cmd, bytes, length, ack); //DITTO
}

void I2C::start() {
	i2c_master_start(cmd);
}

void I2C::readByte(uint8_t* byte, bool ack) {
	if (directionKnown == false) {
		directionKnown = true;
		i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, 1);
	}
	i2c_ack_type_t akker = (!ack) ? i2c_ack_type_t::I2C_MASTER_NACK : i2c_ack_type_t::I2C_MASTER_ACK ;
	i2c_master_read_byte(cmd, byte, akker);
}

void I2C::read(uint8_t* bytes, size_t length, bool ack) {
	if (directionKnown == false) {
		directionKnown = true;
		i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, 1);
	}
	i2c_ack_type_t akker = (!ack) ? i2c_ack_type_t::I2C_MASTER_NACK : i2c_ack_type_t::I2C_MASTER_ACK ;

i2c_master_read(cmd, bytes, length, akker);
}

void I2C::readBytes(uint8_t* bytes, size_t length) {
	if (directionKnown == false) {
		directionKnown = true;
		i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, 1);
	}
	if(length>1)
		i2c_master_read(cmd, bytes, length-1, 0);//0 = ACK
	i2c_master_read_byte(cmd, bytes+length-1, 1);
}

void I2C::stop() {
	i2c_master_stop(cmd);
}

void I2C::init(i2c_port_t portNum,gpio_num_t sdaPin, gpio_num_t sclPin, int speed,SemaphoreHandle_t *i2cSem) {
	this->portNum=portNum;
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = sdaPin;
	conf.scl_io_num = sclPin;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = speed;
	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
	if (!driverInstalled) {
		i2c_driver_install(portNum, I2C_MODE_MASTER, 0, 0, 0);
		driverInstalled = true;
		*i2cSem= xSemaphoreCreateBinary();
		if(*i2cSem)
			xSemaphoreGive(*i2cSem);  //SUPER important else its born locked
		else
			printf("Can't allocate I2C Sem\n");
	}
}
