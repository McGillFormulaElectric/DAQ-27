
#include "ADS7066.hpp"

ADS7066::ADS7066(SPIDriver& spiDriver, IOPin& spiCS, ADS7066_channel_data& channelData)
	: SPIDevice(spiDriver, spiCS),
	  channelData(channelData)
{}

bool ADS7066::configureManualMode() {
    uint8_t reg = 0;

    // Clear CRCERR_IN / BOR
    if (!readReg(ADS7066_REG_SYSTEM_STATUS, &reg)) return false;

    uint8_t clr = 0;
    if (reg & ADS7066_SYSTEM_STATUS_CRCERR_IN_MASK) clr |= ADS7066_SYSTEM_STATUS_CRCERR_IN_MASK;
    if (reg & ADS7066_SYSTEM_STATUS_BOR_MASK)       clr |= ADS7066_SYSTEM_STATUS_BOR_MASK;

    if (clr) {
        if (!writeReg(ADS7066_REG_SYSTEM_STATUS, &clr)) return false;
    }

    // all analog inputs (0 = analog, 1 = GPIO)
    uint8_t pin_cfg = 0x00u; // all AIN
    if (!writeReg(ADS7066_REG_PIN_CFG, &pin_cfg)) return false;

    // internal ref off, CRC OFF, RANGE = 2x, then CAL
    if (!readReg(ADS7066_REG_GENERAL_CFG, &reg)) return false;
    reg &= ~ADS7066_GENERAL_CFG_REF_EN_MASK;   // internal ref off
    reg &= ~ADS7066_GENERAL_CFG_CRC_EN_MASK;  // CRC OFF
    reg |= ADS7066_GENERAL_CFG_RANGE_MASK;  //2 x VREF

    // Start offset calibration
    reg |= ADS7066_GENERAL_CFG_CAL_MASK;
    if (!writeReg(ADS7066_REG_GENERAL_CFG, &reg)) return false;

    // Poll until CAL clears
    for (uint32_t i = 0; i < 5000; i++) {
        if (!readReg(ADS7066_REG_GENERAL_CFG, &reg)) return false;
        if ((reg & ADS7066_GENERAL_CFG_CAL_MASK) == 0u) break;
        if (i == 4999) return false; // timeout
    }

    // set SEQ_MODE = MANUAL (00b)
    if (!readReg(ADS7066_REG_SEQUENCE_CFG, &reg)) return false;

    reg &= ~ADS7066_SEQUENCE_CFG_SEQ_MODE_MASK;
    reg |= ADS7066_SEQ_MODE_MANUAL; // 00b
    reg &= ~ADS7066_SEQUENCE_CFG_SEQ_START_MASK;

    if (!writeReg(ADS7066_REG_SEQUENCE_CFG, &reg)) return false;

    // set default MANUAL_CHID (AIN0)
    const uint8_t manual_chid = ADS7066_CHID_AIN0;

    // Must not be GPIO in PIN_CFG (sanity check)
    if ((pin_cfg >> manual_chid) & 0x1u) return false;

    if (!readReg(ADS7066_REG_CHANNEL_SEL, &reg)) return false;
    reg &= ~ADS7066_CHANNEL_SEL_MANUAL_CHID_MASK;
    reg |= (manual_chid << ADS7066_CHANNEL_SEL_MANUAL_CHID_SHIFT) & ADS7066_CHANNEL_SEL_MANUAL_CHID_MASK;

    if (!writeReg(ADS7066_REG_CHANNEL_SEL, &reg)) return false;

    printf("ADC Configured\r\n");
    return true;
}


bool ADS7066::readReg(uint8_t address, uint8_t *rData) {
	if (rData == nullptr) return false;

	uint8_t tx[3] = { ADS7066_OPCODE_RD_REG, address, 0x00u }; //ask to prepare data
	uint8_t rx[3] = { 0 };

	if (!transmit_Receive(tx, 3, rx, 3)) return false;

	tx[0] = ADS7066_OPCODE_NOP;
	tx[1] = 0x00u;
	tx[2] = 0x00u;

	if (!transmit_Receive(tx, 3, rx, 3)) return false; //once data has been requested and it is fetched (shifted out)

	rData[0] = rx[0]; //copies result into user's buffer
	return true;
}

bool ADS7066::writeReg(uint8_t address, uint8_t *data) {
	if (data == nullptr) return false;
	uint8_t tx[3];
	tx[0] = ADS7066_OPCODE_WR_REG;
	tx[1] = address;
	tx[2] = data[0];

	if(!transmit(tx, sizeof(tx))) return false;

	return true;
}

bool ADS7066::writeReg(uint8_t address, uint8_t value) //overload, when i want to pass in a value and not a pointer
{
    return writeReg(address, &value);
}

bool ADS7066::readChannel(uint8_t channel) {
	if(channel > ADS7066_CHID_AIN7) return false;

	uint8_t reg = 0;
	//ADS7066_REG_CHANNEL_SEL = mux selector register
	if (!readReg(ADS7066_REG_CHANNEL_SEL, &reg)) return false; //previously selected channel
	reg &= ~ADS7066_CHANNEL_SEL_MANUAL_CHID_MASK; //clear the bits that choose the channel id, prepare for new id
	reg |= (channel << ADS7066_CHANNEL_SEL_MANUAL_CHID_SHIFT) & ADS7066_CHANNEL_SEL_MANUAL_CHID_MASK; //moves channel values into correct position

	if (!writeReg(ADS7066_REG_CHANNEL_SEL, &reg)) return false; //switches mux to new channel

	uint8_t tx[3];
	uint8_t rx[3] = {0};
	tx[0] = ADS7066_OPCODE_NOP;
	tx[1] = 0x00u;
	tx[2] = 0x00u;

	if(!transmit(tx, sizeof(tx))) return false; //output is still old channel, clock it out to clear

	if (!transmit_Receive(tx, 3, rx, 3)) return false; //this is the data of the new channel

	uint32_t sample = (static_cast<uint32_t>(rx[0]) << 8) | static_cast<uint32_t>(rx[1]);
	float voltage = (static_cast<float>(sample) / 65535.0f) * 5.0f; //convert adc readings to voltage

	// Store into struct
	switch (channel) {
		case ADS7066_CHID_AIN0: channelData.AIN0 = sample; printf("Channel 0: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN1: channelData.AIN1 = sample; printf("Channel 1: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN2: channelData.AIN2 = sample; printf("Channel 2: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN3: channelData.AIN3 = sample; printf("Channel 3: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN4: channelData.AIN4 = sample; printf("Channel 4: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN5: channelData.AIN5 = sample; printf("Channel 5: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN6: channelData.AIN6 = sample; printf("Channel 6: %.2f V ", voltage); break;
		case ADS7066_CHID_AIN7: channelData.AIN7 = sample; printf("Channel 7: %.2f V ", voltage); break;
		default: return false;
	}
	printf("Channel has been manually read\r\n");
	return true;
}

bool ADS7066::readAll() {
    for (uint8_t channel = ADS7066_CHID_AIN0; channel <= ADS7066_CHID_AIN7; channel++) {
        if (!readChannel(channel)) return false;
    }
    return true;
}

bool ADS7066::configureAutonomousMode(uint8_t channel_select){

	if (!writeReg(ADS7066_AUTO_SEQ_CH_SEL_REG, channel_select)) return false; //choose the channels you want to loop over

	if (!writeReg(ADS7066_REG_SEQUENCE_CFG, ADS7066_SEQ_MODE_AUTO | ADS7066_SEQUENCE_CFG_SEQ_START_MASK)) return false; //choose auto mode (same selector bit) and the start the adc

	if (!writeReg(ADS7066_REG_OPMODE_CFG, 0x0D)) return false; //set internal oscillator to 10.4 kSPS

	if (!writeReg(ADS7066_REG_OSR_CFG, 0x03)) return false; //averages every 8 samples

	if (!writeReg(ADS7066_REG_DATA_CFG, 0b01 << 4)) return false; //append channel id to data

	uint8_t dummy[3] = {0x00, 0x00, 0x00};
	if (!transmit(dummy, 3)) return false; //clocks out first result since it is not accurate

	printf("ADC Initialized, Auto mode activated\r\n");
	return true;
}


bool ADS7066::readChannelAuto(){
	if (!receive(rx, 3)) return false; //receive data and channel id

	uint32_t raw = ((uint32_t)rx[0] << 16) | //ddddddddddddddddddddcccc (d = data, c = channel_id)
	               ((uint32_t)rx[1] << 8)  |
	               ((uint32_t)rx[2]);


	uint32_t value = (raw >> 4) & 0xFFFFF;  // only want 20 data bits
	uint8_t channel = raw & 0x0F; //4-bit channel id

	// Store into struct
//		0000b = AIN0
//
//		0001b = AIN1
//
//		0010b = AIN2
//
//		0011b = AIN3
//
//		0100b = AIN4
//
//		0101b = AIN5
//
//		0110b = AIN6
//
//		0111b = AIN7
	switch (channel) {
		case ADS7066_CHID_AIN0: channelData.AIN0 = value; break;
		case ADS7066_CHID_AIN1: channelData.AIN1 = value; break;
		case ADS7066_CHID_AIN2: channelData.AIN2 = value; break;
		case ADS7066_CHID_AIN3: channelData.AIN3 = value; break;
		case ADS7066_CHID_AIN4: channelData.AIN4 = value; break;
		case ADS7066_CHID_AIN5: channelData.AIN5 = value; break;
		case ADS7066_CHID_AIN6: channelData.AIN6 = value; break;
		case ADS7066_CHID_AIN7: channelData.AIN7 = value; break;
		default: return false;
	}

	return true;

}

bool ADS7066::hello_world() {
    uint8_t status = 0;
    if (!readReg(ADS7066_REG_SYSTEM_STATUS, &status)){
        return false;
    }

    return (status & (1u << 7)) != 0u;
}


