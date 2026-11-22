/*
 * BoardManager_Wrapper.hpp
 *
 *  Created on: Dec 18, 2025
 *      Author: Nathan
 */

#ifndef INC_BOARDMANAGERWRAPPER_H_
#define INC_BOARDMANAGERWRAPPER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void* BoardManager_create(uint32_t filterLowID, uint32_t filterHighID);

bool BoardManager_setup(void* bm);

bool BoardManager_readTransmitAccelData(void* bm);

#ifdef __cplusplus
}
#endif

#endif /* INC_BOARDMANAGERWRAPPER_H_ */
