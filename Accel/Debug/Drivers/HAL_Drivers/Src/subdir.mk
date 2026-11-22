################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Drivers/HAL_Drivers/Src/CANDriver.cpp \
../Drivers/HAL_Drivers/Src/CANMessage.cpp \
../Drivers/HAL_Drivers/Src/IOPin.cpp \
../Drivers/HAL_Drivers/Src/SPIDevice.cpp \
../Drivers/HAL_Drivers/Src/SPIDriver.cpp 

OBJS += \
./Drivers/HAL_Drivers/Src/CANDriver.o \
./Drivers/HAL_Drivers/Src/CANMessage.o \
./Drivers/HAL_Drivers/Src/IOPin.o \
./Drivers/HAL_Drivers/Src/SPIDevice.o \
./Drivers/HAL_Drivers/Src/SPIDriver.o 

CPP_DEPS += \
./Drivers/HAL_Drivers/Src/CANDriver.d \
./Drivers/HAL_Drivers/Src/CANMessage.d \
./Drivers/HAL_Drivers/Src/IOPin.d \
./Drivers/HAL_Drivers/Src/SPIDevice.d \
./Drivers/HAL_Drivers/Src/SPIDriver.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/HAL_Drivers/Src/%.o Drivers/HAL_Drivers/Src/%.su Drivers/HAL_Drivers/Src/%.cyclo: ../Drivers/HAL_Drivers/Src/%.cpp Drivers/HAL_Drivers/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/Device_Drivers/Src" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/Device_Drivers/Inc" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/HAL_Drivers/Inc" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/HAL_Drivers/Src" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/Device_Drivers" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/HAL_Drivers" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-HAL_Drivers-2f-Src

clean-Drivers-2f-HAL_Drivers-2f-Src:
	-$(RM) ./Drivers/HAL_Drivers/Src/CANDriver.cyclo ./Drivers/HAL_Drivers/Src/CANDriver.d ./Drivers/HAL_Drivers/Src/CANDriver.o ./Drivers/HAL_Drivers/Src/CANDriver.su ./Drivers/HAL_Drivers/Src/CANMessage.cyclo ./Drivers/HAL_Drivers/Src/CANMessage.d ./Drivers/HAL_Drivers/Src/CANMessage.o ./Drivers/HAL_Drivers/Src/CANMessage.su ./Drivers/HAL_Drivers/Src/IOPin.cyclo ./Drivers/HAL_Drivers/Src/IOPin.d ./Drivers/HAL_Drivers/Src/IOPin.o ./Drivers/HAL_Drivers/Src/IOPin.su ./Drivers/HAL_Drivers/Src/SPIDevice.cyclo ./Drivers/HAL_Drivers/Src/SPIDevice.d ./Drivers/HAL_Drivers/Src/SPIDevice.o ./Drivers/HAL_Drivers/Src/SPIDevice.su ./Drivers/HAL_Drivers/Src/SPIDriver.cyclo ./Drivers/HAL_Drivers/Src/SPIDriver.d ./Drivers/HAL_Drivers/Src/SPIDriver.o ./Drivers/HAL_Drivers/Src/SPIDriver.su

.PHONY: clean-Drivers-2f-HAL_Drivers-2f-Src

