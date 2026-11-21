################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Drivers/Device_Drivers/Src/ADXL366.cpp 

OBJS += \
./Drivers/Device_Drivers/Src/ADXL366.o 

CPP_DEPS += \
./Drivers/Device_Drivers/Src/ADXL366.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Device_Drivers/Src/%.o Drivers/Device_Drivers/Src/%.su Drivers/Device_Drivers/Src/%.cyclo: ../Drivers/Device_Drivers/Src/%.cpp Drivers/Device_Drivers/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/Device_Drivers/Src" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/Device_Drivers/Inc" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/HAL_Drivers/Inc" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/HAL_Drivers/Src" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/Device_Drivers" -I"C:/Users/aniru/OneDrive - McGill University/mfe/MFE26-Accelerometer/Drivers/HAL_Drivers" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Device_Drivers-2f-Src

clean-Drivers-2f-Device_Drivers-2f-Src:
	-$(RM) ./Drivers/Device_Drivers/Src/ADXL366.cyclo ./Drivers/Device_Drivers/Src/ADXL366.d ./Drivers/Device_Drivers/Src/ADXL366.o ./Drivers/Device_Drivers/Src/ADXL366.su

.PHONY: clean-Drivers-2f-Device_Drivers-2f-Src

