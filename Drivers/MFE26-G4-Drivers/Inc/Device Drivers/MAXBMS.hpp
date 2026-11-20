/*
 * MAXBMS.hpp
 *
 *  Created on: Jan 14, 2026
 *      Author: peste
 */

#ifndef MFE_DRIVERS_INC_DEVICE_DRIVERS_MAXBMS_HPP_
#define MFE_DRIVERS_INC_DEVICE_DRIVERS_MAXBMS_HPP_

#include "main.h"
#include "SPIDevice.hpp"

#define MAX_HELLOALL 0x57
#define MAX_ALERTPACKET 0x21
#define MAX_WRITEDEVICE(addr) static_cast<uint8_t>(((addr & 0x1F) << 3) | 0b100)
#define MAX_WRITEALL 0x02
#define MAX_READDEVICE(addr) static_cast<uint8_t>(((addr & 0x1F) << 3) | 0b101)
#define MAX_READALL 0x03
#define MAX_READBLOCK(addr) static_cast<uint8_t>(((addr & 0x1F) << 3) | 0b110)
#define MAX_DOWNHOST 0x09
#define MAX_UPHOST 0x08

#endif /* MFE_DRIVERS_INC_DEVICE_DRIVERS_MAXBMS_HPP_ */
