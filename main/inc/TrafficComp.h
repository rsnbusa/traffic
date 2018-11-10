/*
 * TrafficComp.h
 *
 *  Created on: Oct 31, 2018
 *      Author: rsn
 */
#include <stdint.h>

#ifndef MAIN_CMDS_TRAFFICCOMP_H_
#define MAIN_CMDS_TRAFFICCOMP_H_

class TrafficComp {
private:
	uint32_t ioports;
	uint8_t options;
	uint8_t typ;
	uint32_t val;
public:
	TrafficComp();
	virtual ~TrafficComp();
	void setComp(uint32_t io, uint8_t opt, uint8_t ty, uint32_t value);
	void playComponent(uint16_t time, uint32_t allbits);
};

#endif /* MAIN_CMDS_TRAFFICCOMP_H_ */
