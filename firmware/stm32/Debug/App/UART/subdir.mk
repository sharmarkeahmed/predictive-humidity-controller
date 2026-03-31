################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/UART/esp8266_uart.c 

OBJS += \
./App/UART/esp8266_uart.o 

C_DEPS += \
./App/UART/esp8266_uart.d 


# Each subdirectory must supply rules for building sources it contributes
App/UART/%.o App/UART/%.su App/UART/%.cyclo: ../App/UART/%.c App/UART/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I"C:/Users/mjxin/OneDrive/Documents/School/Senior_Year/Spring_2026/EE 4951/stm32-esp8266-uart/predictive-humidity-controller-features-stm32-esp8266-uart/firmware/stm32/App/LCD" -I../App/UART -I../App/LCD -I../App/sensors -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/RTOS2/Include -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-UART

clean-App-2f-UART:
	-$(RM) ./App/UART/esp8266_uart.cyclo ./App/UART/esp8266_uart.d ./App/UART/esp8266_uart.o ./App/UART/esp8266_uart.su

.PHONY: clean-App-2f-UART

