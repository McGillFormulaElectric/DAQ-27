/*
 * BoardManager.c
 *
 *  Created on: Jul 11, 2026
 *      Author: aniru
 */

#include "../Inc/BoardManager.h"

bool initializeCAN(){
	can_handle = CAN_create(&hfdcan1, 0x0, 0x7FF); //create pointer to can_driver class

	if (!CAN_initialize(can_handle)){return false;} //enable fdcan peripheral

	if (!CAN_configureTransmission(can_handle)) {return false;} //configure settings

	return true;
}

bool fdcan_tx(uint16_t payload){

	//little endian (Intel): low byte first
	uint8_t buf[2] = { (uint8_t)(payload & 0xFF), (uint8_t)(payload >> 8) };
	
	if (!CAN_addMessageToQueue(can_handle, FRONT_CAN_ID, buf, PAYLOAD_LENGTH)) {return false;}

	return CAN_transmitMessage(can_handle);
}

// Reboots the sensor's internal firmware (software equivalent of toggling XSHUT).
// Fixes the cold-start state where the sensor answers I2C but measures nothing (sig=0, amb=0).
void sensor_soft_reset(){
	VL53L4CD_WrByte(DEVICE_INSTANCE, 0x0000, 0x00);   // hold internal reset
	HAL_Delay(2);
	VL53L4CD_WrByte(DEVICE_INSTANCE, 0x0000, 0x01);   // release -> sensor reboots
	HAL_Delay(5);                                      // SensorInit also waits for boot
}

bool sensor_start(){
	uint16_t id = 0;
	VL53L4CD_Error id_err = VL53L4CD_GetSensorId(DEVICE_INSTANCE, &id);
	printf("[INIT] Sensor ID: 0x%04X (expected 0xEBAA), status=%u\r\n", id, id_err);

	VL53L4CD_Error init_err = VL53L4CD_SensorInit(DEVICE_INSTANCE);
	if (init_err != 0) {
		printf("[INIT] VL53L4CD_SensorInit failed: error=%u\r\n", init_err);
		return false;
	}
	printf("[INIT] SensorInit OK\r\n");

	// Lower signal threshold to 200 kcps (default is ~1024 kcps) so readings
	// with moderate signal (e.g. ~750 kcps at longer distances) are treated as valid
	VL53L4CD_SetSignalThreshold(DEVICE_INSTANCE, 200);

	VL53L4CD_Error range_err = VL53L4CD_StartRanging(DEVICE_INSTANCE);
	if (range_err != 0) {
		printf("[INIT] VL53L4CD_StartRanging failed: error=%u\r\n", range_err);
		return false;
	}
	printf("[INIT] StartRanging OK\r\n");
	return true;
}

void get_data_it(){
	uint16_t payload = 0xFFFF; //error value unless we get a valid reading

	VL53L4CD_Error st = VL53L4CD_GetResult(DEVICE_INSTANCE, &data);

	printf("[TOF] st=%u | status=%u | raw_dist=%u mm | sig=%lu kcps | amb=%lu kcps | sigma=%u\r\n",
	       st, data.range_status, data.distance_mm,
	       (unsigned long)data.signal_rate_kcps,
	       (unsigned long)data.ambient_rate_kcps,
	       data.sigma_mm);

	// Dead sensor: no signal AND no ambient light means the ranging core isn't running.
	// Send the error value, then soft-reset and re-init the sensor.
	if (st == 0 && data.signal_rate_kcps == 0 && data.ambient_rate_kcps == 0) {
		printf("[TOF] -> DEAD reading (sig=0, amb=0). Soft-resetting sensor\r\n");
		VL53L4CD_ClearInterrupt(DEVICE_INSTANCE);
		fdcan_tx(0xFFFF);
		sensor_soft_reset();
		for (int i = 0; i < 5 && !sensor_start(); i++) {
			HAL_Delay(20);
		}
		return;
	}

	// Status codes: 0 = Valid, 1 = Sigma warning, 2 = Signal warning
	// All three provide a measured distance. Discard true errors (3, 4, 5, etc.)
	if (st == 0 && (data.range_status == 0 || data.range_status == 1 || data.range_status == 2)) {
		int16_t raw_dist = (int16_t)data.distance_mm;
		uint16_t dist = (raw_dist > 15) ? (uint16_t)(raw_dist - 15) : 0;
		printf("[TOF] -> VALID Distance: %u mm (raw=%d)\r\n", dist, raw_dist);
		payload = dist;
	} else {
		printf("[TOF] -> INVALID reading (status=%u)! Sending 0xFFFF\r\n", data.range_status);
	}

	VL53L4CD_ClearInterrupt(DEVICE_INSTANCE); //clear interrupt
	bool tx_ok = fdcan_tx(payload);
	if (!tx_ok) {
		printf("[CAN] Tx failed for payload 0x%04X\r\n", payload);
	}
}
