/*
 * Wrapper.cpp
 *
 *  Created on: Jan 31, 2026
 *      Author: aniru
 */

#include "BoardManager.hpp"
#include "Wrapper.h"

extern "C" {

void* BoardManager_create(bool isFrontNode) {
	return new BoardManager(isFrontNode);
}

bool BoardManager_setup(void* bm) {
	return static_cast<BoardManager*>(bm)->setup();
}

bool BoardManager_publishAllChannels(void* bm) {
	return static_cast<BoardManager*>(bm)->publishAllChannels();
}
void BoardManager_destroy(void* bm) {
	delete static_cast<BoardManager*>(bm);
}

void BoardManager_fdcan_bulk_tx(void* bm){
	return static_cast<BoardManager*>(bm)->fdcan_bulk_tx();
}

bool readChannel(void* bm, uint8_t adc_num, uint16_t channel_num){
	return static_cast<BoardManager*>(bm)->readchannel(adc_num, channel_num);
}

bool BoardManager_publishChannel(void* bm, uint8_t adc_num, uint16_t channel_num, int can_id){
	return static_cast<BoardManager*>(bm)->publishChannel(adc_num, channel_num, can_id);
}

bool BoardManager_checkADCPower(void* bm){
	return static_cast<BoardManager*>(bm)->checkADCPower();
}
}



