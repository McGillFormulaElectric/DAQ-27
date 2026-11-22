/*
 * UARTMessage.hpp
 *
 *  Created on: Feb 21, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_UARTMESSAGE_HPP_
#define INC_UARTMESSAGE_HPP_

#include <stdint.h>

class UARTMessage {
	private:
		uint8_t *data;
		uint8_t length;

	public:
		UARTMessage(uint8_t *data = nullptr, uint8_t length = 0);
		bool setData(uint8_t *data, uint8_t length);

		uint8_t* getData();
		uint8_t getLength();
};




#endif /* INC_UARTMESSAGE_HPP_ */
