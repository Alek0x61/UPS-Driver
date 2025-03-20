#include "logger.h"

int initLog(const char* filename) {
    file = fopen(filename, "a");
    if (!file) {
        fprintf(stderr, "Failed to open log file: %s\n", filename);
        return NULL;
    }

    return 0;
}

void logMessage(int logLevel, const char* format, ...) {
    va_list args;
    va_start(args, format);    

    time_t raw_time;
    struct tm *time_info;
    char time_buffer[80];

    time(&raw_time);
    time_info = localtime(&raw_time);

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);
    
    fprintf(file, "[%s] ", time_buffer);

    switch (logLevel) {
        case LOG_ERROR_CODE:
            fprintf(file, "[ERROR] ");
            break;
        default:
            fprintf(file, "[INFO] ");
            break;
    }

    vfprintf(file, format, args);
    fprintf(file, "\n");

    va_end(args);
}

void disposeLogger() {
    fclose(file);
}