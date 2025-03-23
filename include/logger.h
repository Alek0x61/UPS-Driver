#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

static FILE* file = NULL;

#define LOG_INFO_CODE    1
#define LOG_ERROR_CODE   2

int initLog(const char* filename);
void logMessage(int logLevel, const char* format, ...);
void disposeLogger();

#define LOG_ERROR(...) logMessage(LOG_ERROR_CODE, __VA_ARGS__)
#define LOG_INFO(...)  logMessage(LOG_INFO_CODE, __VA_ARGS__)

#endif