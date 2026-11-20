#include "SPIDevice.hpp"
#include "SPIDriver.h"
#include "IOPin.hpp"
#include <cstdio>

// device registers
#define ADS7066_REG_SYSTEM_STATUS        0x00
#define ADS7066_REG_GENERAL_CFG          0x01
#define ADS7066_REG_DATA_CFG             0x02
#define ADS7066_REG_OSR_CFG              0x03
#define ADS7066_REG_OPMODE_CFG           0x04
#define ADS7066_REG_PIN_CFG              0x05
#define ADS7066_REG_GPIO_CFG             0x07
#define ADS7066_REG_GPO_DRIVE_CFG        0x09
#define ADS7066_REG_GPO_OUTPUT_VALUE     0x0B
#define ADS7066_REG_GPI_VALUE            0x0D
#define ADS7066_REG_SEQUENCE_CFG         0x10
#define ADS7066_REG_CHANNEL_SEL          0x11
#define ADS7066_REG_AUTO_SEQ_CH_SEL      0x12

#define ADS7066_REG_DIAGNOSTICS_KEY      0xBF
#define ADS7066_REG_DIAGNOSTICS_EN       0xC0
#define ADS7066_REG_BIT_SAMPLE_LSB       0xC1
#define ADS7066_REG_BIT_SAMPLE_MSB       0xC2

// system status
#define ADS7066_SYSTEM_STATUS_SEQ_STATUS_MASK   (1u << 6)  // 0=stopped, 1=in progress
#define ADS7066_SYSTEM_STATUS_CRCERR_FUSE_MASK  (1u << 2)  // 1=config CRC bad at power-up
#define ADS7066_SYSTEM_STATUS_CRCERR_IN_MASK    (1u << 1)  // 1=CRC error on incoming; write 1 to clear
#define ADS7066_SYSTEM_STATUS_BOR_MASK          (1u << 0)  // 1=brownout/powercycle; write 1 to clear

// general config
#define ADS7066_GENERAL_CFG_REF_EN_MASK         (1u << 7)  // internal reference enable
#define ADS7066_GENERAL_CFG_CRC_EN_MASK         (1u << 6)  // CRC appended + check incoming
#define ADS7066_GENERAL_CFG_RANGE_MASK          (1u << 3)  // 0=1xVREF, 1=2xVREF
#define ADS7066_GENERAL_CFG_CH_RST_MASK         (1u << 2)  // force all channels analog inputs
#define ADS7066_GENERAL_CFG_CAL_MASK            (1u << 1)  // write 1 to start offset cal; auto-clears
#define ADS7066_GENERAL_CFG_RST_MASK            (1u << 0)  // write 1 software reset; auto-clears

#define ADS7066_GENERAL_CFG_RANGE_1X            (0u << 3)
#define ADS7066_GENERAL_CFG_RANGE_2X            (1u << 3)

// general config
#define ADS7066_DATA_CFG_FIX_PAT_MASK           (1u << 7)  // 1=output 0xA5A5 repeating
#define ADS7066_DATA_CFG_APPEND_STATUS_MASK     (3u << 4)  // [5:4]
#define ADS7066_DATA_CFG_APPEND_STATUS_SHIFT    4
#define ADS7066_DATA_CFG_CPOL_CPHA_MASK         (3u << 0)  // [1:0]
#define ADS7066_DATA_CFG_CPOL_CPHA_SHIFT        0

#define ADS7066_DATA_CFG_APPEND_NONE            (0u << 4)  // 00b
#define ADS7066_DATA_CFG_APPEND_CHID            (1u << 4)  // 01b: 4-bit channel id appended
#define ADS7066_DATA_CFG_APPEND_STATUS_FLAGS    (2u << 4)  // 10b: 4-bit status flags appended

#define ADS7066_DATA_CFG_SPI_MODE0              (0u << 0)  // CPOL=0, CPHA=0
#define ADS7066_DATA_CFG_SPI_MODE1              (1u << 0)  // CPOL=0, CPHA=1
#define ADS7066_DATA_CFG_SPI_MODE2              (2u << 0)  // CPOL=1, CPHA=0
#define ADS7066_DATA_CFG_SPI_MODE3              (3u << 0)  // CPOL=1, CPHA=1

// oversampling rate
#define ADS7066_OSR_CFG_OSR_MASK                (7u << 0)  // [2:0]
#define ADS7066_OSR_CFG_OSR_SHIFT               0

#define ADS7066_OSR_NONE                        (0u << 0)  // 000b
#define ADS7066_OSR_2                           (1u << 0)  // 001b
#define ADS7066_OSR_4                           (2u << 0)  // 010b
#define ADS7066_OSR_8                           (3u << 0)  // 011b
#define ADS7066_OSR_16                          (4u << 0)  // 100b
#define ADS7066_OSR_32                          (5u << 0)  // 101b
#define ADS7066_OSR_64                          (6u << 0)  // 110b
#define ADS7066_OSR_128                         (7u << 0)  // 111b

// operation mode config
#define ADS7066_OPMODE_CFG__SEL_MASK         (1u << 4)  // 0=high-speed osc, 1=low-power osc
#define ADS7066_OPMODE_CFG_CLK_DIV_MASK         (0x0Fu << 0) // [3:0]
#define ADS7066_OPMODE_CFG_CLK_DIV_SHIFT        0

#define ADS7066_OPMODE_CFG_OSC_HIGH_SPEED       (0u << 4)
#define ADS7066_OPMODE_CFG_OSC_LOW_POWER        (1u << 4)

// pin config
#define ADS7066_PIN_CFG_MASK                    (0xFFu)    // bit i: 0=analog input, 1=GPIO

// GPIO conig
#define ADS7066_GPIO_CFG_MASK                   (0xFFu)    // bit i: 0=input, 1=output

// GPO drive config
#define ADS7066_GPO_DRIVE_CFG_MASK              (0xFFu)    // bit i: 0=open-drain, 1=push-pull

// GPO output value
#define ADS7066_GPO_OUTPUT_VALUE_MASK           (0xFFu)    // bit i: 0=low, 1=high

// GPI value
#define ADS7066_GPI_VALUE_MASK                  (0xFFu)    // bit i: 0=low, 1=high

// sequence config
#define ADS7066_SEQUENCE_CFG_SEQ_START_MASK     (1u << 4)  // auto-seq start/stop (SEQ_MODE=01b)
#define ADS7066_SEQUENCE_CFG_SEQ_MODE_MASK      (3u << 0)  // [1:0]
#define ADS7066_SEQUENCE_CFG_SEQ_MODE_SHIFT     0

#define ADS7066_SEQ_MODE_MANUAL                 (0u << 0)  // 00b
#define ADS7066_SEQ_MODE_AUTO                   (1u << 0)  // 01b
#define ADS7066_SEQ_MODE_ON_THE_FLY             (2u << 0)  // 10b

//channel select
#define ADS7066_CHANNEL_SEL_MANUAL_CHID_MASK    (0x0Fu << 0) // [3:0]
#define ADS7066_CHANNEL_SEL_MANUAL_CHID_SHIFT   0

#define ADS7066_CHID_AIN0                       (0u)
#define ADS7066_CHID_AIN1                       (1u)
#define ADS7066_CHID_AIN2                       (2u)
#define ADS7066_CHID_AIN3                       (3u)
#define ADS7066_CHID_AIN4                       (4u)
#define ADS7066_CHID_AIN5                       (5u)
#define ADS7066_CHID_AIN6                       (6u)
#define ADS7066_CHID_AIN7                       (7u)

//auto sequence channel select
#define ADS7066_AUTO_SEQ_CH_SEL_MASK            (0xFFu)    // bit i enables AINi in scan
#define ADS7066_AUTO_SEQ_CH_SEL_REG  			(0x12u) //autonmous mode register

//diagnostic key
#define ADS7066_DIAGNOSTICS_KEY_UNLOCK          0x96u      // write 0x96 to enable writes to 0xC0..0xC2

//diagonistic enable
#define ADS7066_DIAGNOSTICS_EN_VTEST_EN_MASK    (1u << 4)  // AIN6 internal 1.8V test (if BITWALK_EN=0)
#define ADS7066_DIAGNOSTICS_EN_BITWALK_EN_MASK  (1u << 0)  // enable bit-walk mode

// bit sample LSB
#define ADS7066_BIT_SAMPLE_LSB_MASK             (0xFFu)    // defines [7:0] bit positions during sampling
#define ADS7066_BIT_SAMPLE_MSB_MASK             (0xFFu)    // defines [15:8] bit positions during sampling

// Operation types
#define ADS7066_OPCODE_NOP        0x00u  // 0000 0000b
#define ADS7066_OPCODE_RD_REG     0x10u  // 0001 0000b  Single register read
#define ADS7066_OPCODE_WR_REG     0x08u  // 0000 1000b  Single register write
#define ADS7066_OPCODE_SET_BIT    0x18u  // 0001 1000b  Set bit(s) at address
#define ADS7066_OPCODE_CLR_BIT    0x20u  // 0010 0000b  Clear bit(s) at address

typedef struct {
	uint32_t AIN0;
	uint32_t AIN1;
	uint32_t AIN2;
	uint32_t AIN3;
	uint32_t AIN4;
	uint32_t AIN5;
	uint32_t AIN6;
	uint32_t AIN7;

} ADS7066_channel_data;

class ADS7066: SPIDevice {
	private:
		ADS7066_channel_data& channelData;
		uint8_t rx[8];

	public:
		ADS7066(SPIDriver& spiDriver, IOPin& spiCS, ADS7066_channel_data& channelData);
		bool configureManualMode();
		bool readReg(uint8_t address, uint8_t *rData);
		bool writeReg(uint8_t address, uint8_t *data);
		bool writeReg(uint8_t address, uint8_t data);
		bool readChannel(uint8_t channel);
		bool readAll();

		//autonomous mode
		bool configureAutonomousMode(uint8_t channel_select);
		bool readChannelAuto();

		bool hello_world();
};

