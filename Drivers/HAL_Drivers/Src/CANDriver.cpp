/*
 * CANDriver.cpp
 *
 *  Created on: Feb 12, 2025
 *      Author: mariobouzakhm
 */

#include "CANDriver.h"
#include "CANMessage.hpp"

CANDriver::CANDriver(FDCAN_HandleTypeDef* canInstance, uint32_t filterLowID, uint32_t filterHighID)
    : canInstance(canInstance),
      queueSize(0),
      queueHeadIndex(0),
      queueTailIndex(0),
      filterLowID(filterLowID),
      filterHighID(filterHighID) {}

uint32_t CANDriver::dlcFromLen(uint8_t len) {
    if (len > 8) len = 8;

    switch (len) {
        case 0: return FDCAN_DLC_BYTES_0;
        case 1: return FDCAN_DLC_BYTES_1;
        case 2: return FDCAN_DLC_BYTES_2;
        case 3: return FDCAN_DLC_BYTES_3;
        case 4: return FDCAN_DLC_BYTES_4;
        case 5: return FDCAN_DLC_BYTES_5;
        case 6: return FDCAN_DLC_BYTES_6;
        case 7: return FDCAN_DLC_BYTES_7;
        default:return FDCAN_DLC_BYTES_8;
    }
}

bool CANDriver::initialize() {
	 FDCAN_FilterTypeDef sFilterConfig;

	  /* Configure Rx filter */
	  sFilterConfig.IdType = FDCAN_STANDARD_ID;
	  sFilterConfig.FilterIndex = 0;
	  sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
	  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
	  sFilterConfig.FilterID1 = this->filterLowID;
	  sFilterConfig.FilterID2 = this->filterHighID;

	  if(HAL_FDCAN_ConfigFilter(this->canInstance, &sFilterConfig) != HAL_OK) {
		  return 0;
	  }

	  if (HAL_FDCAN_ConfigGlobalFilter(this->canInstance, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
	  {
		return 0;
	  }

	  /* Start the FDCAN module */
	  if (HAL_FDCAN_Start(this->canInstance) != HAL_OK)
	  {
		return 0;
	  }

	  if (HAL_FDCAN_ActivateNotification(this->canInstance, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
	  {
		return 0;
	  }
	  return 1;
}

bool CANDriver::configureTransmission() {
	  FDCAN_TxHeaderTypeDef TxHeader;

	  TxHeader.IdType = FDCAN_STANDARD_ID;
	  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
	  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
	  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
	  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
	  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	  TxHeader.MessageMarker = 0;

	  this->TxHeader = TxHeader;

	  return 1;
}

bool CANDriver::addMessageToQueue(const CANMessage& message) {
    if (isQueueFull()) return false;

    canQueue[queueTailIndex] = message;
    queueTailIndex = (queueTailIndex + 1) % MAX_QUEUE_CAPACITY;
    queueSize++;
    return true;
}

bool CANDriver::transmitMessage() {
    if (isQueueEmpty()) return false;

    CANMessage msg = canQueue[queueHeadIndex];
    queueHeadIndex = (queueHeadIndex + 1) % MAX_QUEUE_CAPACITY;
    queueSize--;

    TxHeader.Identifier = msg.getID();
    TxHeader.DataLength = CANDriver::dlcFromLen(msg.getLength());

    if (HAL_FDCAN_AddMessageToTxFifoQ(canInstance, &TxHeader, msg.getData()) != HAL_OK)
        return false;

    return true;
}

bool CANDriver::transmitPriorityMessage() {
	//to be implemented
	return 1;
}

bool CANDriver::isQueueFull() const { return queueSize == MAX_QUEUE_CAPACITY; }

bool CANDriver::isQueueEmpty() const { return this->queueSize == 0;}

int CANDriver::getQueueSize() const { return this->queueSize; }

