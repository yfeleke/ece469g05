################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../apps/example/hello_world/hello_world.c 

OBJS += \
./apps/example/hello_world/hello_world.o 

C_DEPS += \
./apps/example/hello_world/hello_world.d 


# Each subdirectory must supply rules for building sources it contributes
apps/example/hello_world/%.o: ../apps/example/hello_world/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

