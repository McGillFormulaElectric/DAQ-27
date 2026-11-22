/*
 * Board_Manager.cpp
 *
 *  Created on: Dec 18, 2025
 *      Author: Nathan
 */

#include "Board_Manager.hpp"
#include "ADXL366.hpp"
#include <cstdio> //debugging
#include <cstring> //||
#include "usart.h" //||

BoardManager::BoardManager(uint32_t filterLowID, uint32_t filterHighID)
	: canDriver(&hfdcan1, filterLowID, filterHighID),
	  spiDriver(&hspi1),
	  spiCS(SPI_CS_GPIO_Port, SPI_CS_Pin),
	  accel(spiDriver, spiCS, config, accelData)
{
	config.dataRate        = ADXL366_FILTER_ODR_100HZ;
	config.measurementMode = ADXL366_POWER_MEASURE_MEAS;
	config.noiseMode       = ADXL366_PWRCTL_LOWNOISE;
	config.range           = ADXL366_RANGE_2G;
}

bool BoardManager::setup() {
	bool initCAN = initializeCAN();

	if(!(initCAN)) {
		return 0;
	}

	bool initAccel = initializeAccel();

	return initAccel;
}

bool BoardManager::initializeCAN() {
	canDriver.initialize();
	canDriver.configureTransmission();
	return 1;
}

bool BoardManager::initializeAccel() {
	bool initAccel = accel.initialize();
	bool configureAccel = 0;

	if(initAccel) {
		configureAccel = accel.configure();
	}

	return configureAccel;
}

bool BoardManager::readTransmitAccelData() {
    if (!accel.readAccelData()) return false;

    const int16_t x = accel.getX();
    const int16_t y = accel.getY();
    const int16_t z = accel.getZ();
    const float x_g = static_cast<float>(x) / 4096.0f;
    const float y_g = static_cast<float>(y) / 4096.0f;
    const float z_g = static_cast<float>(z) / 4096.0f;

    // ---- UART debug print ----
       char buf[64];
       const int n = snprintf(buf, sizeof(buf), "ADXL x=%.3f y=%.3f z=%.3f g\r\n",
                              static_cast<double>(x_g),
                              static_cast<double>(y_g),
                              static_cast<double>(z_g));
       if (n > 0) {
           HAL_UART_Transmit(&huart1,
                             reinterpret_cast<uint8_t*>(buf),
                             static_cast<uint16_t>(strnlen(buf, sizeof(buf))),
                             10); // short timeout so you don't block forever
       }
     // --------------------------

    // Scale to mg for CAN: +-2g range is ~4000 LSB/g, so 1 mg = 4 LSB
    const int16_t x_mg = static_cast<int16_t>(x / 4);
    const int16_t y_mg = static_cast<int16_t>(y / 4);
    const int16_t z_mg = static_cast<int16_t>(z / 4);

    uint8_t payload[8] = {0};

    payload[0] = static_cast<uint8_t>(x_mg & 0xFF);
    payload[1] = static_cast<uint8_t>((x_mg >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>(y_mg & 0xFF);
    payload[3] = static_cast<uint8_t>((y_mg >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>(z_mg & 0xFF);
    payload[5] = static_cast<uint8_t>((z_mg >> 8) & 0xFF);
    
//    Accel1 -- FL
//    CANMessage msg(0x384);

    //Accel2 -- FL
//    CANMessage msg(0x38E);

    //Accel3 -- RL
//    CANMessage msg(0x398);

//    //Accel4 -- RR
    CANMessage msg(0x3A2);


    msg.setMessage(payload, 8);

    if (!canDriver.addMessageToQueue(msg)) return false;
    return canDriver.transmitMessage();
}







