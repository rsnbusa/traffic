/*
 * Nodo.h
 *
 *  Created on: Oct 31, 2018
 *      Author: rsn
 */
#include "TrafficComp.h"

#ifndef MAIN_INC_NODO_H_
#define MAIN_INC_NODO_H_

class Nodo {
private:
	int numComp;
	TrafficComp components[10];
	uint32_t allbits;
public:
	Nodo();
	void setPorts(uint32_t lasLuces);
	int addComponent(uint32_t io, uint8_t opt, uint8_t typ, uint32_t value);
	void startSequence(uint16_t time);
	void abortSequence();
};

#endif /* MAIN_INC_NODO_H_ */
