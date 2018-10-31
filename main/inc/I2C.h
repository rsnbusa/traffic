/*
 * I2C.h
 *
 *  Created on: Feb 24, 2017
 *      Author: kolban
 */

#ifndef MAIN_I2C_H_
#define MAIN_I2C_H_
#include <stdint.h>

#include <driver/i2c.h>
#include <driver/gpio.h>


/**
 * @brief Interface to %I2C functions.
 */
class I2C {
private:
	i2c_cmd_handle_t cmd;
	bool directionKnown;
	i2c_port_t portNum;

public:
	uint8_t address;

	I2C();
	void beginTransaction();
	int endTransaction();
	/**
	 * @brief Get the address of the %I2C slave against which we are working.
	 *
	 * @return The address of the %I2C slave.
	 */
	uint8_t getAddress() const
	{
		return address;
	}

	void init(i2c_port_t portNum,gpio_num_t sdaPin = DEFAULT_SDA_PIN, gpio_num_t sclPin = DEFAULT_CLK_PIN, int speed=400000,SemaphoreHandle_t *i2cSem=NULL);
	void read(uint8_t *bytes, size_t length, bool ack=true);
	void readByte(uint8_t *byte, bool ack=true);
	void readBytes(uint8_t *bytes, size_t length);


	/**
	 * @brief Set the address of the %I2C slave against which we will be working.
	 *
	 * @param[in] address The address of the %I2C slave.
	 */
	void setAddress(uint8_t address)
	{
		this->address = address;
	}
	void start();
	void stop();
	void write(uint8_t byte, bool ack=true);
	void write(uint8_t *bytes, size_t length, bool ack=true);
	static const gpio_num_t DEFAULT_SDA_PIN = GPIO_NUM_25;
	static const gpio_num_t DEFAULT_CLK_PIN = GPIO_NUM_26;
};

#endif /* MAIN_I2C_H_ */
