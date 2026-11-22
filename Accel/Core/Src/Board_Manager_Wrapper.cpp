/*
 * Board_Manager_Wrapper.cpp
 *
 *  Created on: Dec 18, 2025
 *      Author: Nathan
 */

/*
 * BoardManager_Wrapper.cpp
 *
 *  Created on: Dec 18, 2025
 *      Author: Nathan
 */

/*
 * BoardManagerWrapper.cpp
 */

#include "Board_Manager_Wrapper.h"
#include "Board_Manager.hpp"
#include "ADXL366.hpp"

extern "C" {

void* BoardManager_create(uint32_t filterLowID, uint32_t filterHighID)
{
    static BoardManager* bm_ptr = nullptr;

    if (bm_ptr == nullptr) {
        static BoardManager bm_instance(
            filterLowID,
            filterHighID
        );

        bm_ptr = &bm_instance;
    }

    return bm_ptr;
}

bool BoardManager_setup(void* bm)
{
    if (bm == nullptr) return false;
    return static_cast<BoardManager*>(bm)->setup();
}

bool BoardManager_readTransmitAccelData(void* bm)
{
    if (bm == nullptr) return false;
    return static_cast<BoardManager*>(bm)->readTransmitAccelData();
}

}
