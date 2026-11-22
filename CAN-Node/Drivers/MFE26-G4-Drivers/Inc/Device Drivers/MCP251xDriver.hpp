/*
 * MCP251xDriver.hpp
 *
 *  Created on: Feb 15, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_MCP251XDRIVER_HPP_
#define INC_MCP251XDRIVER_HPP_

#include "SPIDevice.hpp"
#include "DataField.hpp"

#define RESET_CMD 0b0000
#define READ_CMD 0b0011
#define WRITE_CMD 0b0010
#define READ_CRC_CMD 0b1011
#define WRITE_CRC_CMD 0b1010

#define DEVID_ADD 0xE14

class MCP251X: public SPIDevice {
	private:
		DataField *accelX;
		DataField *accelY;
		DataField *accelZ;


	public:
		bool setup();

		bool reset();
		bool read_address(uint16_t address, uint8_t *rData, uint8_t numberOfBytes);
		bool write_address(uint16_t address, uint8_t *data, uint8_t numberOfBytes);

		bool isValidDeviceID();
};




#endif /* INC_MCP251XDRIVER_HPP_ */
