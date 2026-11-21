/*
 * CANMessage.cpp
 *
 *  Created on: Feb 12, 2025
 *      Author: mariobouzakhm
 */

#include "CANMessage.hpp"
#include <cstring>

CANMessage::CANMessage()
        : id(0), length(0) {}

CANMessage::CANMessage(uint32_t id)
    : id(id), length(0) {}

void CANMessage::setMessage(const uint8_t* src, uint8_t length) {
    if (length > 8) length = 8;
    this->length = length;
    memcpy(data, src, length);
}

uint8_t *CANMessage::getData() {
	return data;
}
uint32_t CANMessage::getID() {
	return id;
}
uint8_t CANMessage::getLength() {
	return length;
}

