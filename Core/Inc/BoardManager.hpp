/*
 * BoardManager.hpp
 *
 *  Created on: Jan 22, 2026
 *      Author: aniru
 */

#ifndef INC_BOARDMANAGER_HPP_
#define INC_BOARDMANAGER_HPP_

#include "CANDriver.h"
#include "SPIDriver.h"
#include "ADS7066.hpp"
#include "IOPin.hpp"

class BoardManager {
	private:
		CANDriver can1;

		SPIDriver spi1;

		IOPin adsCS1;
		IOPin adsCS2;
		ADS7066_channel_data adsData1;
		ADS7066_channel_data adsData2;
		ADS7066 ads1;
		ADS7066 ads2;
		bool isFrontNode;

		bool initializeCAN();
		bool initializeSPI();
		bool initializeADSManual();
		bool initializeADSAuto();
		bool fdcan_tx(const uint8_t* data, int id, uint8_t len);

		bool canReady;
		bool spiReady;
		bool adsReady;

	public:

		BoardManager(bool isFrontNode);

		bool setup();
		bool publishAllChannels();
		void fdcan_bulk_tx();

		bool readchannel(uint8_t adc_num, uint16_t channel_num);
		bool publishChannel(uint8_t adc_num, uint16_t channel_num, int can_id);
		bool checkADCPower();




};




#endif /* INC_BOARDMANAGER_HPP_ */
