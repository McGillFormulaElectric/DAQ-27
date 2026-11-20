/*
 * MCP251xDriver.cpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#include "MCP251xDriver.hpp"


bool MCP251X::reset() {
	uint8_t tData[2];
	tData[0] = ((RESET_CMD) << 4) | 0b0000;
	tData[1] = 0;

	return this->transmit(tData, 2);
}

bool MCP251X::setup() {
	/* RESET THE DEVICE TO ENSURE ITS IN A KNOWN STATE */
	bool resetResult = this->reset();
	if(!resetResult) {
		return 0;
	}


	/* OSCILLATOR CONFIGURATION */

	/* SET CAN BUS SPEED */

	/* CONFIGURE CAN MODE */

	/* ENABLE INTERRUPTS */

	/* SETUP TX FIFO */

	/* SETUP RX FIXO */

	/* CONFIGURE RETRANSMISSION */

	return 1;
}


bool MCP251X::read_address(uint16_t address, uint8_t *rData, uint8_t numberOfBytes) {
	uint8_t tData[2];

	tData[0] = ((READ_CMD) << 4) | ((address & 0xF00) >> 8);
	tData[1] = (uint8_t) (address & 0xFF);

	return this->transmit_Receive(tData, 2, rData, numberOfBytes);
}

bool MCP251X::write_address(uint16_t address, uint8_t *data, uint8_t numberOfBytes) {
	uint8_t tData[2+numberOfBytes];

	tData[0] = ((WRITE_CMD) << 4) | ((address & 0xF00) >> 8);
	tData[1] = (uint8_t) (address & 0xFF);

	for(int i = 0; i < numberOfBytes; i++) {
		tData[2+i] = data[i];
	}

	return this->transmit(tData, 2+numberOfBytes);
}

bool MCP251X::isValidDeviceID() {
    uint8_t rData[4];

    this->read_address(DEVID_ADD, rData, 4);
    return ((rData[0] & 0xF0) >> 4) != 0;

}



