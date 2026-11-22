/*
 * UARTDriver.cpp
 *
 *  Created on: Mar 4, 2025
 *      Author: peste
 */

#include "UARTDriver.h"

UARTDriver* gUartDriver = nullptr;

UARTDriver::UARTDriver(UART_HandleTypeDef *uartInstance, int queueCapacity) {
	this->uartInstance = uartInstance;
	this->queueSize = 0;
	this->queueCapacity = queueCapacity;

	this->queueHeadIndex = 0;
	this->queueTailIndex = 0;
	this->uartBusy = 0;

	this->uartQueue = new UARTMessage*[queueCapacity];
	for(int i = 0; i < queueCapacity; i++) {
	    	uartQueue[i] = nullptr;
	}

	gUartDriver = this;
}

UARTDriver::~UARTDriver() {
	for (int i = 0; i < queueSize; i++) {
        	int index = (queueHeadIndex + i) % queueCapacity;
        	if(uartQueue[index] != nullptr) {
            		delete uartQueue[index];
        	}
	}
    	delete[] uartQueue;
}

bool UARTDriver::addMessageToQueue(UARTMessage* message) {
	if (queueSize >= queueCapacity) {
		return 0;
	}

	uartQueue[queueTailIndex] = message;
	queueTailIndex = (queueTailIndex + 1) % queueCapacity;
	queueSize++;

	if (!uartBusy) {
	    	transmit();
	}
	return 1;
}

bool UARTDriver::transmit() {
	if (queueSize == 0 || uartBusy) {
	    	return 0;
	}
	uartBusy = 1;
	UARTMessage* msg = uartQueue[queueHeadIndex];

	if (HAL_UART_Transmit_DMA(uartInstance, msg->getData(), msg->getLength()) != HAL_OK) {
	    	uartBusy = 0;
	    	return 0;
	}
	return 1;
}

void UARTDriver::handleTxComplete() {


	UARTMessage* msg = uartQueue[queueHeadIndex];
    	if (msg != nullptr) {
        	delete msg;
        	uartQueue[queueHeadIndex] = nullptr;
    	}
    	queueHeadIndex = (queueHeadIndex + 1) % queueCapacity;
    	queueSize--;
    	uartBusy = 0;
	
    	if (queueSize > 0) {
        	transmit();
    	}
}

bool UARTDriver::receive(uint8_t* rData, uint16_t size) {
	if(HAL_UART_Receive(this->uartInstance, rData, size, 100*size) != HAL_OK) {
		return 0;
	}
	return 1;
}

bool UARTDriver::isQueueFull() {
	return this->queueSize >= this->queueCapacity;
}

bool UARTDriver::isQueueEmpty() {
	return this->queueSize == 0;
}

int UARTDriver::getQueueSize() {
	return this->queueSize;
}
