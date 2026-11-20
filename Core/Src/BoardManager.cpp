/*
 * BoardManagar.cpp
 *
 *  Created on: Jan 22, 2026
 *      Author: aniru
 */


#include "BoardManager.hpp"
#include <cstring>
#include <cstdio>
#include <cmath>


BoardManager::BoardManager(bool isFrontNode)
	: can1(&hfdcan1, 0x000, 0x7FF),
	  spi1(&hspi1),
	  adsCS1(ADC_CS1_GPIO_Port,ADC_CS1_Pin), //seperates into 2 different chip select along spi line
	  adsCS2(ADC_CS2_GPIO_Port,ADC_CS2_Pin),
	  adsData1{},
	  adsData2{},
	  ads1(spi1, adsCS1, adsData1),
	  ads2(spi1, adsCS2, adsData2),
	  isFrontNode(isFrontNode) //this->isFrontNode = isFrontNode

{
		  this->canReady = false;
		  this->spiReady = false;
		  this->adsReady = false;
	}

bool BoardManager::setup(){
    while (!(canReady && spiReady && adsReady)) {
        if (!canReady) canReady = initializeCAN();   // each guard = "don't redo if done"
        if (!spiReady) spiReady = initializeSPI();
        if (!adsReady) adsReady = initializeADSManual();

        if (!(canReady && spiReady && adsReady)){
            HAL_Delay(50); // backoff so you're not hammering a failing peripheral
        	return false;
        }
    }
    return true;
}

bool BoardManager::initializeCAN(){
	if (!can1.initialize()) return false;
	if (!can1.configureTransmission()) return false;
	printf("FDCAN successfully initialized\r\n");
	return true;
}

bool BoardManager::initializeSPI(){
	if (!ads1.hello_world()) {
        printf("adc1 not found using spi\r\n");
        return false;
    }

	if (!ads2.hello_world()) {
        printf("adc2 not found using spi\r\n");
        return false;
    }
	printf("adc detected with spi\r\n");
	return true;
}

bool BoardManager::checkADCPower(){
	// same check as initializeSPI: hello_world() reads register 0x0 and confirms bit 7 is
	// still set, which only happens if the chip is powered and responding over SPI
	if (!ads1.hello_world()) {
		printf("adc1 not responding, possible power loss\r\n");
		return false;
	}

	if (!ads2.hello_world()) {
		printf("adc2 not responding, possible power loss\r\n");
		return false;
	}

	printf("adc still alive\r\n");
	return true;
}

bool BoardManager::initializeADSManual(){
	if (!ads1.configureManualMode()){
		printf("adc 1 not initialized\r\n");
		return false;
	}

	if (!ads2.configureManualMode()){
		printf("adc 2 not initialized\r\n");
		return false;
	}

	if (!ads1.readChannel(ADS7066_CHID_AIN0) || !ads2.readChannel(ADS7066_CHID_AIN0)){
		printf("Channel could not be read\r\n");
		return false;
	}

	printf("adc initialized (manual)\r\n");
	return true;
}

bool BoardManager::initializeADSAuto(){
	if (!ads1.configureAutonomousMode(0b11111111)){
		printf("adc 1 not initialized\r\n");
		return false;
	}

	if (!ads2.configureAutonomousMode(0b11111111)){
		printf("adc 2 not initialized\r\n");
		return false;
	}
	if (!ads1.readChannelAuto() || !ads2.readChannelAuto()){
		printf("Channel could not be read\r\n");
		return false;
	}

	printf("adc initialized (auto)\r\n");
	return true;
}
bool BoardManager::publishAllChannels() {
    if (!ads1.readAll()){
    	printf("Could not read ADC 1\r\n");
    	return false;
    }
    printf("\r\n");
    if (!ads2.readAll()){
    	printf("Could not read ADC 2\r\n");
    	return false;
    }
    printf("ADC has been read\r\n");
    return true;
}

bool BoardManager::readchannel(uint8_t adc_num, uint16_t channel_num)
{
    if (channel_num > ADS7066_CHID_AIN7) {
        return false;
    }

    ADS7066* adc;
    ADS7066_channel_data* data;

    if (adc_num == 1) {
        adc = &ads1;
        data = &adsData1;
    } else if (adc_num == 2) {
        adc = &ads2;
        data = &adsData2;
    } else {
        return false;
    }

    if (!adc->readChannel(static_cast<uint8_t>(channel_num))) {
        printf("ADC read error\r\n");
        return false;
    }

    const uint32_t* channels = &data->AIN0;

    printf(
        "Data: %lu\r\n\r\n",
        static_cast<unsigned long>(channels[channel_num])
    );

    return true;
}




// ── Transfer Functions ──

// KTY84: R = 5.9093*T + 433.41  →  T = (R - 433.41) / 5.9093
// Assumes voltage divider with fixed 1kΩ resistor, Vref = 5V
// R_sensor = (V * 1000) / (5 - V)
// Every KTY84-based DBC signal (200, 600, 700, 720) uses factor 0.1 / offset -40,
// so the temperature is pre-scaled to raw CAN counts here: raw = (T - offset) / factor.
float KTY84_function(uint32_t adc_raw) {
    // 16-bit ADC: 0–5 V
    float voltage = (adc_raw / 65535.0f) * 5.0f;
    if (voltage >= 5.0f) return 999.0f; // Saturated

    // 1 kΩ pull-up to 5 V, sensor to ground
    float resistance = (voltage * 1000.0f) / (5.0f - voltage);

    // Linear approximation over 60–120°C: T = mR + b
    float temp_c = 0.151106f * resistance - 51.6298f;

    // Return raw CAN counts: factor 0.1, offset -40
    return (temp_c + 40.0f) / 0.1f;
}

float GE1711_function(uint32_t adc_raw)
{
    if (adc_raw == 0 || adc_raw >= 65535u) {
        return NAN;
    }

    // 16-bit ADC: 0–5 V
    const float voltage = (adc_raw / 65535.0f) * 5.0f;

    // Linear R(V): 1 kΩ pull-up to 5 V, sensor to ground
    const float resistance = 694.7334f * voltage - 725.567f;

    // Linear T(R), based on y=mx+b from the 85°C and 100°C table points
    const float tempC = -0.03827507f * resistance + 125.95433f;

    // Raw CAN counts: factor 0.1, offset -40
    return (tempC + 40.0f) / 0.1f;
}

// RE22 Potentiometer: angle = (adc_value / 65535) * 90
// Both RotPotAngles DBC signals (300, 500) use factor 0.01 / offset 0,
// so the angle is pre-scaled to raw CAN counts here: raw = angle / 0.01.
float RE22_function(uint32_t adc_raw) {
    float angle_deg = (adc_raw / 65535.0f) * 90.0f;
    return angle_deg / 0.01f;
}

// INFKL-800 IR brake temp sensor: V = 0.005*T + 0.5  →  T = 200*V − 100
// Slope: 5 mV/°C | Range: 0–800°C (0.50–4.50 V) | Linear fit across full range
// BrakeRotorTemps DBC signals (800, 810) use factor 0.1 / offset 0,
// so the temperature is pre-scaled to raw CAN counts here: raw = (T - offset) / factor.
float INFKL800_function(uint32_t adc_raw) {
  float voltage = (adc_raw / 65535.0f) * 10.0f;
  float temp_c = (voltage*1000 - 500) / 5;
  return temp_c / 0.1f;
}

bool BoardManager::publishChannel(uint8_t adc_num, uint16_t channel_num, int can_id){
	if (channel_num > ADS7066_CHID_AIN7) return false; // ADS7066 only has AIN0–AIN7

	// pointer into the struct, avoids copying the value before we need it
	uint32_t* sample = nullptr;

	if (adc_num == 1) {
		if (!ads1.readChannel(channel_num)) return false;
		// AIN fields are contiguous uint32_t, treat as array via pointer arithmetic
		sample = (&adsData1.AIN0 + channel_num);
	} else {
		if (!ads2.readChannel(channel_num)) return false;
		sample = (&adsData2.AIN0 + channel_num);
	}

	uint8_t data[8] = {0}; // CAN frame is fixed 8 bytes; upper 4 bytes stay zero
	memcpy(data, sample, sizeof(*sample)); // s
	if (!fdcan_tx(data, can_id, 8)) {
		printf("Not pushed to bus\r\n\r\n");
		return false;
	}
	printf("Pushed data on bus\r\n\r\n");
	return true;
}

bool BoardManager::fdcan_tx(const uint8_t* data, int id, uint8_t len){
	CANMessage msg = CANMessage(id, data, len);
	if (!can1.addMessageToQueue(msg)) {return false; }
	if (!can1.transmitMessage()) {
		// The failed message is still at the queue head (transmitMessage only
		// advances the head on HAL success), so retry that same head entry once
		// after a brief delay. This keeps the queue in sync for the following
		// fdcan_tx calls instead of letting the stuck message shift the cycle.
//		HAL_Delay(2);
//		if (!can1.transmitMessage()) { printf("[BM] transmitMessage retry failed id: %d\r\n", id); return false; }
		return false;
	}
	HAL_Delay(5);//give time for message to be sent before checking error codes
	if (!can1.checkACK()){
		printf(" id: %d Not pushed to bus\r\n\r\n", id);
		return false;
	}

	printf("Pushed id: %d on bus\r\n\r\n",id);

	return true;
}


void BoardManager::fdcan_bulk_tx() {
    uint8_t data[8] = {0};
    uint64_t packed = 0;
    //void *memset(void *ptr, int value, size_t num);
    //sets all values in an array to a chosen num

    //void *memcpy(void *dest, const void *src, size_t n);
    //copies elements from one block of memory to another
    if (isFrontNode) {
        // Message 200 (0xC8): Front_ThermistorTemps (KTY84)
        // Pinout: Temp1=ANA_IN_5(ADC1.4) Temp2=ANA_IN_8(ADC1.7) Temp3=ANA_IN_16(ADC2.7) Temp4=ANA_IN_15(ADC2.6)
        float therm1_c = KTY84_function(adsData2.AIN6);
        float therm2_c = KTY84_function(adsData2.AIN5);
        float therm3_c = KTY84_function(adsData1.AIN5);
        float therm4_c = KTY84_function(adsData1.AIN4);
        uint16_t therm1 = static_cast<uint16_t>(therm1_c) & 0x0FFF;
        uint16_t therm2 = static_cast<uint16_t>(therm2_c) & 0x0FFF;
        uint16_t therm3 = static_cast<uint16_t>(therm3_c) & 0x0FFF;
        uint16_t therm4 = static_cast<uint16_t>(therm4_c) & 0x0FFF;
        packed = static_cast<uint64_t>(therm1) | (static_cast<uint64_t>(therm2) << 12) | (static_cast<uint64_t>(therm3) << 24) | (static_cast<uint64_t>(therm4) << 36);
        memset(data, 0, 8); memcpy(data, &packed, 6);
        fdcan_tx(data, 200, 8);

        // Message 210 (0xD2): BrakeLineTemp_Front (GE-1711)
        // Pinout: ANA_IN_7 (ADC1.6)
        float blt_c = GE1711_function(adsData2.AIN4);
        uint16_t brake_line = static_cast<uint16_t>(blt_c);
        memset(data, 0, 8); memcpy(data, &brake_line, 2);
        fdcan_tx(data, 210, 8);

        // Message 300 (0x12C): Front_RotPotAngles (RE22)
        // Pinout: Angle1(FR)=ANA_IN_9(ADC2.0) Angle2(FL)=ANA_IN_10(ADC2.1)
        float rot_fr_deg = RE22_function(adsData1.AIN0);
        float rot_fl_deg = RE22_function(adsData1.AIN1);
        uint16_t rot_fr = static_cast<uint16_t>(rot_fr_deg);
        uint16_t rot_fl = static_cast<uint16_t>(rot_fl_deg);
        packed = static_cast<uint64_t>(rot_fr) | (static_cast<uint64_t>(rot_fl) << 16);
        memset(data, 0, 8); memcpy(data, &packed, 8);
        fdcan_tx(data, 300, 8);

        // Message 700 (0x2BC): OilTemps (KTY84)
        // Pinout: OilTemp_Engine(FR)=ANA_IN_6(ADC1.5) OilTemp_Gearbox(FL)=ANA_IN_13(ADC2.4)
        float oil_fr_c = KTY84_function(adsData2.AIN7);
        float oil_fl_c = KTY84_function(adsData1.AIN6);
        uint16_t oil_fr = static_cast<uint16_t>(oil_fr_c) & 0x0FFF;
        uint16_t oil_fl = static_cast<uint16_t>(oil_fl_c) & 0x0FFF;
        packed = static_cast<uint64_t>(oil_fr) | (static_cast<uint64_t>(oil_fl) << 12);
        memset(data, 0, 8); memcpy(data, &packed, 8);
        fdcan_tx(data, 700, 8);

        // Message 800 (0x320): BrakeRotorTemps (INFKL-50)
        // Pinout: BrakeTemp_FL=ANA_IN_14(ADC2.5) BrakeTemp_FR=ANA_IN_1(ADC1.0)
        float rotor_fl_c = INFKL800_function(adsData1.AIN7);
        float rotor_fr_c = INFKL800_function(adsData2.AIN0);
        uint16_t rotor_fl = static_cast<uint16_t>(rotor_fl_c);
        uint16_t rotor_fr = static_cast<uint16_t>(rotor_fr_c);
        packed = static_cast<uint64_t>(rotor_fl) | (static_cast<uint64_t>(rotor_fr) << 16);
        memset(data, 0, 8); memcpy(data, &packed, 8);
        fdcan_tx(data, 800, 8);

    } else {
        // Message 500 (0x1F4): Rear_RotPotAngles (RE22)
        float rot_rr_deg = RE22_function(adsData1.AIN4);
        float rot_rl_deg = RE22_function(adsData1.AIN6);
        uint16_t rot_rr = static_cast<uint16_t>(rot_rr_deg);
        uint16_t rot_rl = static_cast<uint16_t>(rot_rl_deg);
        packed = static_cast<uint64_t>(rot_rr) | (static_cast<uint64_t>(rot_rl) << 16);
        memset(data, 0, 8); memcpy(data, &packed, 8);
        fdcan_tx(data, 500, 8);

        // Message 600 (0x258): Rear_ThermistorTemps (KTY84)
        float therm1_c = KTY84_function(adsData1.AIN3);
        float therm2_c = KTY84_function(adsData1.AIN2);
        float therm3_c = KTY84_function(adsData2.AIN0);
        float therm4_c = KTY84_function(adsData2.AIN1);
        uint16_t therm1 = static_cast<uint16_t>(therm1_c) & 0x0FFF;
        uint16_t therm2 = static_cast<uint16_t>(therm2_c) & 0x0FFF;
        uint16_t therm3 = static_cast<uint16_t>(therm3_c) & 0x0FFF;
        uint16_t therm4 = static_cast<uint16_t>(therm4_c) & 0x0FFF;
        packed = static_cast<uint64_t>(therm1) | (static_cast<uint64_t>(therm2) << 12) | (static_cast<uint64_t>(therm3) << 24) | (static_cast<uint64_t>(therm4) << 36);
        memset(data, 0, 8); memcpy(data, &packed, 8);
        fdcan_tx(data, 600, 8);

        // Message 720 (0x2D0): Rear_OilTemps + BrakeLineTemp (KTY84 + GE-1711)
        float oil_rr_c   = KTY84_function(adsData1.AIN1);
        float oil_rl_c   = KTY84_function(adsData1.AIN7);
        float blt_rear_c = GE1711_function(adsData1.AIN5);
        uint16_t oil_rr   = static_cast<uint16_t>(oil_rr_c) & 0x0FFF;
        uint16_t oil_rl   = static_cast<uint16_t>(oil_rl_c) & 0x0FFF;
        uint16_t blt_rear = static_cast<uint16_t>(blt_rear_c) & 0x0FFF;
        packed = static_cast<uint64_t>(oil_rr) | (static_cast<uint64_t>(oil_rl) << 12) | (static_cast<uint64_t>(blt_rear) << 24);
        memset(data, 0, 8); memcpy(data, &packed, 8);
        fdcan_tx(data, 720, 8);

        // Message 810 (0x32A): Rear_BrakeRotorTemp (INFKL-50)
        float rotor_r_c = INFKL800_function(adsData2.AIN6);
        uint16_t rotor_r = static_cast<uint16_t>(rotor_r_c);
        memset(data, 0, 8); memcpy(data, &rotor_r, 2);
        fdcan_tx(data, 810, 8);
    }

}
