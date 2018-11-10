/*
 * Nodo.cpp
 *
 *  Created on: Oct 31, 2018
 *      Author: rsn
 */

#include "Nodo.h"
#include "TrafficComp.h"

Nodo::Nodo() {
	allbits=0;
	numComp=0;
}
void Nodo::setPorts(uint32_t lasLuces)
{
	allbits=lasLuces;
}

int Nodo::addComponent(uint32_t io, uint8_t opt, uint8_t typ, uint32_t value){
	if(numComp<9){
		components[numComp++].setComp(io,opt,typ,value);
		return 0;
	}
	else
		return -1;
}

void Nodo::startSequence(uint16_t time)
{
	for (int a=0;a<numComp;a++)
		components[a].playComponent(time,allbits);
}

void Nodo::abortSequence(){

}


