/*
 * UARTDriver.h
 *
 *  Created on: Feb 21, 2025
 *      Author: mariobouzakhm
 */

#ifndef INC_UARTDRIVER_H_
#define INC_UARTDRIVER_H_

#include "usart.h"
#include "UARTMessage.hpp"

class UARTDriver {
	private:
		UART_HandleTypeDef *uartInstance;

		int queueSize;
		int queueCapacity;
		int queueHeadIndex;
		int queueTailIndex;
		UARTMessage** uartQueue;
	   	bool uartBusy;

	public:
		UARTDriver(UART_HandleTypeDef *uartInstance, int queueCapacity = 50);
	    	~UARTDriver();

		bool addMessageToQueue(UARTMessage* message);
		bool transmit();
	    	void handleTxComplete();
		bool receive(uint8_t* rData, uint16_t size);

		bool isQueueFull();
		bool isQueueEmpty();
		int getQueueSize();
};





#endif /* INC_UARTDRIVER_H_ */
