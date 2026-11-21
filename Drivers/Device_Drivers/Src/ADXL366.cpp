/*
 * ADXL366.cpp
 *
 *  Created on: Nov 22, 2025
 *      Author: Nizar Kheireddine
 */

#include "ADXL366.hpp"
#include <cstdint>
#include <cstdio> //debugging
#include <cstring> //||
#include "usart.h" //||

ADXL366::ADXL366(SPIDriver& spiDriver, IOPin& spiCS, ADXL366_config& config, ADXL366_accel_data& accelData)
	: SPIDevice(spiDriver, spiCS),
	  config(config),
	  accelData(accelData),
	  xBuf{},
	  yBuf{},
	  zBuf{},
	  xSum(0),
	  ySum(0),
	  zSum(0),
	  filterIdx(0),
	  sampleCount(0)
{
}

bool ADXL366::write(uint8_t address, uint8_t numberOfBytes, uint8_t *data) {
	uint8_t tData[2+numberOfBytes];
	tData[0] = ADXL366_WRITE_CMD ;
    tData[1] = address;

	for(int i = 0; i < numberOfBytes; i++) {
		tData[2+i] = data[i];
	}

	return transmit(tData, 2+numberOfBytes);
}

bool ADXL366::read(uint8_t address, uint8_t numberOfBytes, uint8_t *rData) {
	uint8_t tx[2 + numberOfBytes];
	uint8_t rx[2 + numberOfBytes];

	tx[0] = ADXL366_READ_CMD;
	tx[1] = address;
	for (uint8_t i = 0; i < numberOfBytes; i++) {
		tx[2 + i] = 0x00; // dummy
	}

	bool result = transmit_Receive(tx, sizeof(tx), rx, sizeof(rx));
	if (!result) return false;

	for (uint8_t i = 0; i < numberOfBytes; i++) {
		rData[i] = rx[2 + i];
	}
	return true;

}

bool ADXL366::check_ID(uint8_t expected_ID, uint8_t register_address, uint8_t* rData){
    bool readIDs = read(register_address, 1, rData);
    if(!readIDs||rData[0]!=expected_ID){
        return readIDs;
    }
    return true;
}

bool ADXL366::check_IDs(uint8_t* rData){
	 bool result = true;
	 result &= check_ID(DEVID_ADXL366,     ADXL366_REG_DEVID_AD,  rData);
	 result &= check_ID(ADXL366_DEVID_MST, ADXL366_REG_DEVID_MST, rData);
	 result &= check_ID(ADXL366_PART_ID,   ADXL366_REG_PART_ID,   rData);
	 result &= check_ID(ADXL366_REV_ID,    ADXL366_REG_REV_ID,    rData);
	 return result;
}

bool ADXL366::configure(){
	uint8_t filterctl = 0;
	uint8_t pwr_standby = 0;
	uint8_t pwr_measure = 0;

	filterctl |= (config.dataRate & ADXL366_FILTER_ODR_MASK);
	filterctl |= (config.range    & ADXL366_FILTER_RANGE_MASK);

	//force standby
	pwr_standby |= (config.noiseMode & ADXL366_POWER_NOISE_MODE);
	if (!write(ADXL366_REG_POWER_CTL, 1, &pwr_standby))
		return false;

	HAL_Delay(1);

	if (!write(ADXL366_REG_FILTER_CTL, 1, &filterctl))
		return false;

	pwr_measure |= (config.noiseMode & ADXL366_POWER_NOISE_MODE);
	pwr_measure |= (config.measurementMode & ADXL366_POWER_MEASURE_MODE);

	if (!write(ADXL366_REG_POWER_CTL, 1, &pwr_measure))
		return false;

	HAL_Delay(5);

	// verifying power_ctl
	uint8_t verify = 0;
	if (!read(ADXL366_REG_POWER_CTL, 1, &verify))
		return false;
	if ((verify & ADXL366_POWER_MEASURE_MODE) == 0x00)
		return false;
	return true;
}

bool ADXL366::initialize() {
    //soft-reset
    uint8_t resetCmd = ADXL366_Soft_Reset_CMD;
    bool result = write(ADXL366_REG_SOFT_RESET, 1, &resetCmd);
    if(!result){
        return false;
    }

    //chceks to see if actually talking to accel
    HAL_Delay(25);
    uint8_t rData[1];
    result = check_IDs(rData);
    if(!result){
        return false;
    }
    //applies settings from config struct
    result = configure();
    return result;
}

bool ADXL366::readAccelData() {
    uint8_t rData[6];
    bool readAccel = read(ADXL366_REG_XDATA_H, 6, rData);
    if (!readAccel) return false;

    // Decode raw 14-bit signed values (MSB first, right-aligned with >>2)
    int16_t xRaw = (int16_t)(((uint16_t)rData[0] << 8) | rData[1]) >> 2;
    int16_t yRaw = (int16_t)(((uint16_t)rData[2] << 8) | rData[3]) >> 2;
    int16_t zRaw = (int16_t)(((uint16_t)rData[4] << 8) | rData[5]) >> 2;

    // --- Moving-average filter (circular buffer, running sum) ---
    // Subtract the oldest sample that is about to be overwritten
    xSum -= xBuf[filterIdx];
    ySum -= yBuf[filterIdx];
    zSum -= zBuf[filterIdx];

    // Write new sample into the circular buffer
    xBuf[filterIdx] = xRaw;
    yBuf[filterIdx] = yRaw;
    zBuf[filterIdx] = zRaw;

    // Add new sample to running sums
    xSum += xRaw;
    ySum += yRaw;
    zSum += zRaw;

    // Advance the index (wraps at ADXL366_MA_FILTER_SIZE)
    filterIdx = (filterIdx + 1) % ADXL366_MA_FILTER_SIZE;

    // Track how many valid samples we have (saturates at filter size)
    if (sampleCount < ADXL366_MA_FILTER_SIZE) {
        ++sampleCount;
    }

    // Divide by the number of valid samples (handles warmup period)
    accelData.x = static_cast<int16_t>(xSum / sampleCount);
    accelData.y = static_cast<int16_t>(ySum / sampleCount);
    accelData.z = static_cast<int16_t>(zSum / sampleCount);

    return true;
}

int16_t ADXL366::getX() const {
    return accelData.x;
}

int16_t ADXL366::getY() const {
    return accelData.y;
}

int16_t ADXL366::getZ() const {
    return accelData.z;
}
