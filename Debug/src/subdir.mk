################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/comms.c \
../src/cr_startup_lpc175x_6x.c \
../src/crp.c \
../src/encoder.c \
../src/homing.c \
../src/limit_switches.c \
../src/main.c \
../src/motor.c \
../src/pot.c 

C_DEPS += \
./src/comms.d \
./src/cr_startup_lpc175x_6x.d \
./src/crp.d \
./src/encoder.d \
./src/homing.d \
./src/limit_switches.d \
./src/main.d \
./src/motor.d \
./src/pot.d 

OBJS += \
./src/comms.o \
./src/cr_startup_lpc175x_6x.o \
./src/crp.o \
./src/encoder.o \
./src/homing.o \
./src/limit_switches.o \
./src/main.o \
./src/motor.o \
./src/pot.o 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__CODE_RED -DCORE_M3 -D__USE_CMSIS=CMSISv2p00_LPC17xx -D__LPC17XX__ -D__REDLIB__ -I"C:\Users\danie\Documents\MCUXpressoIDE_25.6.136\lpc1769_drivers\tp-final\inc" -I"C:\Users\danie\Documents\MCUXpressoIDE_25.6.136\lpc1769_drivers\CMSISv2p00_LPC17xx\inc" -I"C:\Users\danie\Documents\MCUXpressoIDE_25.6.136\lpc1769_drivers\CMSISv2p00_LPC17xx\Drivers\inc" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m3 -mthumb -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src

clean-src:
	-$(RM) ./src/comms.d ./src/comms.o ./src/cr_startup_lpc175x_6x.d ./src/cr_startup_lpc175x_6x.o ./src/crp.d ./src/crp.o ./src/encoder.d ./src/encoder.o ./src/homing.d ./src/homing.o ./src/limit_switches.d ./src/limit_switches.o ./src/main.d ./src/main.o ./src/motor.d ./src/motor.o ./src/pot.d ./src/pot.o

.PHONY: clean-src

