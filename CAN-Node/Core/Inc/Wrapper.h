/*
 * Wrapper.h
 *
 *  Created on: Feb 1, 2026
 *      Author: aniru
 */

#ifndef INC_WRAPPER_H_
#define INC_WRAPPER_H_
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void* BoardManager_create(bool isFrontNode);
bool BoardManager_setup(void* bm);
bool BoardManager_publishAllChannels(void* bm);
void BoardManager_fdcan_bulk_tx(void* bm);
void BoardManager_destroy(void* bm);
bool readChannel(void* bm, uint8_t adc_num, uint16_t channel_num);
bool BoardManager_publishChannel(void* bm, uint8_t adc_num, uint16_t channel_num, int can_id);
bool BoardManager_checkADCPower(void* bm);


#ifdef __cplusplus

}
#endif
#endif

/* INC_WRAPPER_H_ */
