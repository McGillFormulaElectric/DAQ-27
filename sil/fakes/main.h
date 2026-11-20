#pragma once
#include "fake_hal.hpp"

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;
extern FDCAN_HandleTypeDef hfdcan1;

#define ADC_CS1_GPIO_Port (&sil::gpio1)
#define ADC_CS2_GPIO_Port (&sil::gpio2)
#define ADC_CS1_Pin 1u
#define ADC_CS2_Pin 2u
