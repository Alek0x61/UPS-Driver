#ifndef BATTERYSOC_H
#define BATTERYSOC_H

#include <stdio.h>    
#include <stdlib.h>   
#include <stdint.h>  
#include <string.h>  
#include <errno.h>
#include <sys/time.h> 
#include <unistd.h>
#include <math.h>

#include "types/result.h"
#include "types/battery_state.h"
#include "electrical_data.h"
#include "logger.h"
#include "data_logger.h"
#include "../globalConfig.h"

BatteryState getState();
float trimSoc(float soc);
double calculateDeltaTime(struct timeval* previous_time);
float updateStateOfCharge(float soc, float current, float time_hours);
int calculateChargingSoC(float *soc_mem_ref);
int calculateDischargingSoC(float *soc_mem_ref);

#endif