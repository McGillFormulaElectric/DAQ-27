/*
 * ADXL366.hpp
 *
 *  Created on: Nov 22, 2025
 *      Author: Nizar Kheireddine
 */

#ifndef MFE_DRIVERS_INC_DEVICE_DRIVERS_ADXL366_HPP_
#define MFE_DRIVERS_INC_DEVICE_DRIVERS_ADXL366_HPP_

#include "SPIDevice.hpp"
#include "IOPin.hpp"


//Primary commands
#define ADXL366_READ_CMD 0x0B
#define ADXL366_WRITE_CMD 0x0A
#define ADXL366_READ_FIFO_CMD 0x0D

//Secondary commands
#define ADXL366_Soft_Reset_CMD 0x52
//------------------------------------------------------------------------------
// ADXL366 REGISTER MAP
//------------------------------------------------------------------------------

#define ADXL366_REG_DEVID_AD              0x00
#define ADXL366_REG_DEVID_MST             0x01
#define ADXL366_REG_PART_ID               0x02
#define ADXL366_REG_REV_ID                0x03

#define ADXL366_REG_RESERVED_04           0x04
#define ADXL366_REG_SERIAL_NUM_2          0x05
#define ADXL366_REG_SERIAL_NUM_1          0x06
#define ADXL366_REG_SERIAL_NUM_0          0x07

#define ADXL366_REG_XDATA                 0x08
#define ADXL366_REG_YDATA                 0x09
#define ADXL366_REG_ZDATA                 0x0A

#define ADXL366_REG_STATUS                0x0B

#define ADXL366_REG_FIFO_ENTRIES_L        0x0C
#define ADXL366_REG_FIFO_ENTRIES_H        0x0D

#define ADXL366_REG_XDATA_H               0x0E
#define ADXL366_REG_XDATA_L               0x0F
#define ADXL366_REG_YDATA_H               0x10
#define ADXL366_REG_YDATA_L               0x11
#define ADXL366_REG_ZDATA_H               0x12
#define ADXL366_REG_ZDATA_L               0x13

#define ADXL366_REG_TEMP_H                0x14
#define ADXL366_REG_TEMP_L                0x15

#define ADXL366_REG_EX_ADC_H              0x16
#define ADXL366_REG_EX_ADC_L              0x17

#define ADXL366_REG_I2C_FIFO_DATA         0x18   // I2C-only

#define ADXL366_REG_SOFT_RESET            0x1F

// Activity / Inactivity Thresholds & Timers
#define ADXL366_REG_THRESH_ACT_H          0x20
#define ADXL366_REG_THRESH_ACT_L          0x21
#define ADXL366_REG_TIME_ACT              0x22
#define ADXL366_REG_THRESH_INACT_H        0x23
#define ADXL366_REG_THRESH_INACT_L        0x24
#define ADXL366_REG_TIME_INACT_H          0x25
#define ADXL366_REG_TIME_INACT_L          0x26

#define ADXL366_REG_ACT_INACT_CTL         0x27

// FIFO
#define ADXL366_REG_FIFO_CONTROL           0x28
#define ADXL366_REG_FIFO_SAMPLES           0x29

// Interrupt Mapping (Lower)
#define ADXL366_REG_INTMAP1_LOWER         0x2A
#define ADXL366_REG_INTMAP2_LOWER         0x2B

// Filter & Power Control
#define ADXL366_REG_FILTER_CTL            0x2C
#define ADXL366_REG_POWER_CTL             0x2D

// Self-Test
#define ADXL366_REG_SELF_TEST             0x2E

// Tap Detection
#define ADXL366_REG_TAP_THRESH            0x2F
#define ADXL366_REG_TAP_DUR               0x30
#define ADXL366_REG_TAP_LATENT            0x31
#define ADXL366_REG_TAP_WINDOW            0x32

// User Offsets
#define ADXL366_REG_X_OFFSET              0x33
#define ADXL366_REG_Y_OFFSET              0x34
#define ADXL366_REG_Z_OFFSET              0x35

// User Sensitivity
#define ADXL366_REG_X_SENS                0x36
#define ADXL366_REG_Y_SENS                0x37
#define ADXL366_REG_Z_SENS                0x38

// Timer Control
#define ADXL366_REG_TIMER_CTL             0x39

// Interrupt Mapping (Upper)
#define ADXL366_REG_INTMAP1_UPPER         0x3A
#define ADXL366_REG_INTMAP2_UPPER         0x3B

// ADC Control & Temperature Control
#define ADXL366_REG_ADC_CTL               0x3C
#define ADXL366_REG_TEMP_CTL              0x3D

// Temp ADC Thresholds
#define ADXL366_REG_TEMP_ADC_OVER_THRSH_H    0x3E
#define ADXL366_REG_TEMP_ADC_OVER_THRSH_L    0x3F
#define ADXL366_REG_TEMP_ADC_UNDER_THRSH_H   0x40
#define ADXL366_REG_TEMP_ADC_UNDER_THRSH_L   0x41

#define ADXL366_REG_TEMP_ADC_TIMER        0x42

// Axis Mask
#define ADXL366_REG_AXIS_MASK             0x43

// Status Copies
#define ADXL366_REG_STATUS_COPY           0x44
#define ADXL366_REG_STATUS_2              0x45


// Temperature
#define ADXL366_REG_TEMP_H           0x14
#define ADXL366_REG_TEMP_L           0x15

// ADC External Input
#define ADXL366_REG_ADC_H            0x16
#define ADXL366_REG_ADC_L            0x17

// FIFO
#define ADXL366_REG_FIFO_DATA        0x18
#define ADXL366_REG_FIFO_CTL         0x1A
#define ADXL366_REG_FIFO_SAMPLES     0x1B

// Activity / Inactivity
#define ADXL366_REG_THRESH_ACT_H     0x20
#define ADXL366_REG_THRESH_ACT_L     0x21
#define ADXL366_REG_TIME_ACT         0x22

#define ADXL366_REG_THRESH_INACT_H   0x23
#define ADXL366_REG_THRESH_INACT_L   0x24
#define ADXL366_REG_TIME_INACT_H     0x25
#define ADXL366_REG_TIME_INACT_L     0x26

#define ADXL366_REG_ACT_INACT_CTL    0x27

// Interrupts
#define ADXL366_REG_INTMAP1_L        0x2A
#define ADXL366_REG_INTMAP2_L        0x2B
#define ADXL366_REG_FILTER_CTL       0x2C
#define ADXL366_REG_POWER_CTL        0x2D
#define ADXL366_REG_SELFTEST         0x2E

// Tap detection
#define ADXL366_REG_TAP_THRESH       0x2F
#define ADXL366_REG_TAP_DUR          0x30
#define ADXL366_REG_TAP_LATENCY      0x31
#define ADXL366_REG_TAP_WINDOW       0x32

// Offsets
#define ADXL366_REG_X_OFF            0x33
#define ADXL366_REG_Y_OFF            0x34
#define ADXL366_REG_Z_OFF            0x35

// Sensitivity registers
#define ADXL366_REG_X_SENS           0x36
#define ADXL366_REG_Y_SENS           0x37
#define ADXL366_REG_Z_SENS           0x38

// Timer Control
#define ADXL366_REG_TIMER_CTL        0x3A

// Temperature Config + thresholds
#define ADXL366_REG_TEMP_CFG         0x3B
#define ADXL366_REG_TEMP_ACT_H       0x3C
#define ADXL366_REG_TEMP_ACT_L       0x3D
#define ADXL366_REG_TEMP_INACT_H     0x3E
#define ADXL366_REG_TEMP_INACT_L     0x3F

#define ADXL366_PWRCTL_MEASUREMENT   0x01    // Measure mode enable
#define ADXL366_PWRCTL_LOWNOISE      0x02
#define ADXL366_PWRCTL_ULTRALOWNOISE 0x03
#define ADXL366_PWRCTL_AUTOSLEEP     0x04
#define ADXL366_PWRCTL_WAKEUP        0x08

#define ADXL366_FILTER_ODR_MASK      0x07   // bits [2:0]
#define ADXL366_FILTER_ODR_12_5HZ    0x00
#define ADXL366_FILTER_ODR_25HZ      0x01
#define ADXL366_FILTER_ODR_50HZ      0x02
#define ADXL366_FILTER_ODR_100HZ     0x03
#define ADXL366_FILTER_ODR_200HZ     0x04
#define ADXL366_FILTER_ODR_400HZ     0x05

#define ADXL366_RANGE_2G             0x00
#define ADXL366_RANGE_4G             0x40
#define ADXL366_RANGE_8G             0x80

#define ADXL366_STATUS_DATA_READY    0x01
#define ADXL366_STATUS_FIFO_READY    0x02
#define ADXL366_STATUS_ACTIVITY      0x10
#define ADXL366_STATUS_INACTIVITY    0x20
#define ADXL366_STATUS_AWAKE         0x40


// Moving-average filter window size
#define ADXL366_MA_FILTER_SIZE 16

//USEfuul values
#define DEVID_ADXL366 0xAD
#define ADXL366_DEVID_MST 0x1D
#define ADXL366_PART_ID 0xF7
#define ADXL366_REV_ID 0x05


//bitmask definitions
#define ADXL366_FILTER_ODR_MASK      0x07   // bits [2:0]
#define ADXL366_FILTER_RANGE_MASK    0xC0   // bits [5:4]
#define ADXL366_POWER_MEASURE_MODE   0x03   // bits [1:0]
#define ADXL366_POWER_NOISE_MODE   0x30   // bits [5:4]

//power modes
#define ADXL366_POWER_MEASURE_STANDBY 0x00
#define ADXL366_POWER_MEASURE_MEAS    0x02

typedef struct {
    uint8_t dataRate;
    uint8_t measurementMode;
    uint8_t noiseMode;
    uint8_t range;
} ADXL366_config;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} ADXL366_accel_data;

class ADXL366: public SPIDevice {
    private:
        ADXL366_config& config;
        ADXL366_accel_data& accelData;

        // Moving-average filter (circular buffer + running sum)
        int16_t xBuf[ADXL366_MA_FILTER_SIZE];
        int16_t yBuf[ADXL366_MA_FILTER_SIZE];
        int16_t zBuf[ADXL366_MA_FILTER_SIZE];
        int32_t xSum;
        int32_t ySum;
        int32_t zSum;
        uint8_t filterIdx;    // next slot to overwrite
        uint8_t sampleCount;  // saturates at ADXL366_MA_FILTER_SIZE

    public:
        ADXL366(SPIDriver& spiDriver, IOPin& spiCS, ADXL366_config& config, ADXL366_accel_data& accelData);
        bool write(uint8_t address, uint8_t numberOfBytes, uint8_t *data);
		bool read(uint8_t address, uint8_t numberOfBytes, uint8_t *rData);
        bool check_ID(uint8_t expected_ID, uint8_t register_address, uint8_t* rData);
        bool check_IDs(uint8_t* rData);
        bool initialize();
        bool configure();



        bool readAccelData();
        int16_t getX() const;
        int16_t getY() const;
        int16_t getZ() const;


};

#endif /* MFE_DRIVERS_INC_DEVICE_DRIVERS_ADXL366_HPP_ */
