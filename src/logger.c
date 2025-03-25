#include "../include/logger.h"

int initLog(const char* filename) {
    file = fopen(filename, "a");
    if (!file) {
        perror("Failed to open log file");
        return -1;
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

    if (logLevel == LOG_ERROR_CODE) {
        fprintf(stderr, format, args); 
    }

    vfprintf(file, format, args);
    fprintf(file, "\n");
    fflush(file); 
    
    va_end(args);
}

void disposeLogger() {
    fclose(file);
}