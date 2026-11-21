/*
 * Board_Manager.hpp
 *
 *  Created on: Dec 18, 2025
 *      Author: Nathan
 */

#ifndef INC_BOARD_MANAGER_H_
#define INC_BOARD_MANAGER_H_

#include "CANDriver.h"
#include "SPIDriver.h"
#include "ADXL366.hpp"
#include "IOPin.hpp"
#include "fdcan.h"

class BoardManager {
	private:
		CANDriver canDriver;
		SPIDriver spiDriver;
		IOPin spiCS;
		ADXL366 accel;

		ADXL366_config config{};
		ADXL366_accel_data accelData{};

		// Calibration offsets (subtracted from raw readings)
		int16_t offsetX = 0;
		int16_t offsetY = 0;
		int16_t offsetZ = 0;

		bool initializeCAN();
		bool initializeAccel();
		bool calibrate(uint16_t numSamples = 50);


	public:
		BoardManager(uint32_t filterLowID, uint32_t filterHighID);
		bool setup();

		bool readTransmitAccelData();

};

#endif /* INC_BOARDMANAGER_H_ */
