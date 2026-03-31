################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/LCD/ILI9341.c \
../App/LCD/XPT2046.c \
../App/LCD/glcdfont.c 

OBJS += \
./App/LCD/ILI9341.o \
./App/LCD/XPT2046.o \
./App/LCD/glcdfont.o 

C_DEPS += \
./App/LCD/ILI9341.d \
./App/LCD/XPT2046.d \
./App/LCD/glcdfont.d 


# Each subdirectory must supply rules for building sources it contributes
App/LCD/%.o App/LCD/%.su App/LCD/%.cyclo: ../App/LCD/%.c App/LCD/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I"C:/Users/mjxin/OneDrive/Documents/School/Senior_Year/Spring_2026/EE 4951/stm32-esp8266-uart/predictive-humidity-controller-features-stm32-esp8266-uart/firmware/stm32/App/LCD" -I../App/UART -I../App/LCD -I../App/sensors -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/RTOS2/Include -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-LCD

clean-App-2f-LCD:
	-$(RM) ./App/LCD/ILI9341.cyclo ./App/LCD/ILI9341.d ./App/LCD/ILI9341.o ./App/LCD/ILI9341.su ./App/LCD/XPT2046.cyclo ./App/LCD/XPT2046.d ./App/LCD/XPT2046.o ./App/LCD/XPT2046.su ./App/LCD/glcdfont.cyclo ./App/LCD/glcdfont.d ./App/LCD/glcdfont.o ./App/LCD/glcdfont.su

.PHONY: clean-App-2f-LCD

