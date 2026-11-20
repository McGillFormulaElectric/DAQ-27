/*
 * UARTMessage.cpp
 *
 *  Created on: Feb 21, 2025
 *      Author: mariobouzakhm
 */

#include "UARTMessage.hpp"

UARTMessage::UARTMessage(uint8_t *data, uint8_t length) {
	this->data = data;
	this->length = length;
}


bool UARTMessage::setData(uint8_t *data, uint8_t length) {
	this->data = data;
	this->length = length;

	return 1;
}

uint8_t* UARTMessage::getData() {
	return this->data;
}

uint8_t UARTMessage::getLength() {
	return this->length;
}



