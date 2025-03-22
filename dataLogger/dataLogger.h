#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "../result.h"
#include <errno.h>

int logMessages(float current, float power, float voltage, double delta_time, float soc);
Result createLogFile(const char* dataLoggerPath);

#endif
