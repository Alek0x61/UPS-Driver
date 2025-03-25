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
#include <sys/time.h> 

#include "../globalConfig.h"

void configureElectricalData(int p_fd);
int tryReadRegister(uint8_t reg, int16_t* result);
float convertToValidUnit(int16_t rawValue, float lsb);
int tryReadPower(float* power);
int tryReadCurrent(float* current);
int tryReadVoltage(float* voltage);
int dischargeCalibration(float* calibration);
int chargeCalibration(float* calibration);

#endif
