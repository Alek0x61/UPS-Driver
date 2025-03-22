#include "dataLogger.h"

char path[30];

int logMessages(float current, float power, float voltage, double delta_time, float soc) {
    printf("Battery Voltage: %.3fV\n", voltage);
    printf("delta_time: %.6f seconds\n", delta_time);
    printf("Current: %.3fA\n", current);
    printf("power: %.3fW\n", power);
    printf("Estimated SoC: %.3f%%\n", soc);
    printf("\n");

    FILE *file = fopen(path, "a");
    if (file == NULL) {
        perror("Failed to open CSV file");
        return -1;
    }

    fprintf(file, "%.6f,%.3f,%.3f,%.3f,%.3f\n", delta_time, voltage, current, power, soc);
    fclose(file);
    return 0;
}

Result createLogFile(const char* dataLoggerPath) {
    Result res;
    res.status = 0;

    strcpy(path, dataLoggerPath);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        file = fopen(path, "w");
        if (file != NULL) {
            fprintf(file, "Time(s),Voltage(V),Current(A),Power(W),SoC(%%)\n");
            fclose(file);
        }
        else {
            snprintf(res.message, sizeof(res.message), "Failed to create the data logger file: %s", strerror(errno));
            res.status = -1;
        }
            
    } else {
        fclose(file);
    }

    return res;
}