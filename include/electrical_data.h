#ifndef ELECTRICALDATA_H
#define ELECTRICALDATA_H

#include <stdint.h> 
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>  
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>

void configureElectricalData(int p_fd);
int tryReadRegister(uint8_t reg, int16_t* result);
float convertToValidUnit(int16_t rawValue, float lsb);
float tryReadPower(float* power);
float tryReadCurrent(float* current);
float tryReadVoltage(float* voltage);
float dischargeCalibration();
float chargeCalibration();

#endif
