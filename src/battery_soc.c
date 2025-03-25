#include "../include/battery_soc.h"

BatteryState getState() {
    float power;
    float powerRes = tryReadPower(&power);
    if (powerRes < 0) {
        LOG_ERROR("Failed to get valid power measurement %s", strerror(powerRes));
        return -1;
    }
    if (power < 0.1) {
        return ACPOWER;
    }

    float voltage;
    float voltageRes = tryReadVoltage(&voltage);
    if (voltageRes < 0) {
        LOG_ERROR("Failed to get valid voltage measurement %s", strerror(voltageRes));
        return -1; 
    }
    if (voltage < MIN_VOLTAGE) {
        return DEPLETED;
    }

    float current;
    float currentRes = tryReadCurrent(&current);
    if (currentRes < 0) {
        LOG_ERROR("Failed to get valid current measurement %s", strerror(currentRes));
        return -1; 
    }
    if (current > 0) {
        return CHARGING;
    }

    return DISCHARGING;
}

float trimSoc(float soc) {
    if (soc < 0)
        return 0;
    if (soc > 1)
        return 1;
    return soc;
}

double calculateDeltaTime(struct timeval* previous_time) {
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    double delta_time = (double)(current_time.tv_sec - previous_time->tv_sec) +
                (double)(current_time.tv_usec - previous_time->tv_usec) / 1000000.0;
    
    *previous_time = current_time; 

    if (delta_time > 7) {
        #if INFO_LOGGER_ENABLED
            LOG_INFO("delta time dillation: %.6f seconds", delta_time);
        #endif
        delta_time = 5.005;
    }

    return delta_time;
}

float updateStateOfCharge(float soc, float current, float time_hours) {
    return soc + (current / BATTERY_CAPACITY) * time_hours;
}

int calculateChargingSoC(float *soc_mem_ref) {
    struct timeval previous_time;
    gettimeofday(&previous_time, NULL);

    float calibration = 0;
    int calibResult = chargeCalibration(&calibration);
    if (calibResult < 0) {
        LOG_ERROR("Failed to get valid charge calibration %s", strerror(calibResult));
        return -1;        
    }

    #if INFO_LOGGER_ENABLED
        LOG_INFO("Calibration SoC: %.3f", calibration);
    #endif
    if (fabs((*soc_mem_ref) - calibration) > 0.4) {
        *soc_mem_ref = calibration;
    }

    while (1) {
        float power;
        float powerRes = tryReadPower(&power);
        if (powerRes < 0) {
            LOG_ERROR("Failed to get valid power measurement %s", strerror(powerRes));
        }

        float current;
        float currentRes = tryReadCurrent(&current);
        if (currentRes < 0) {
            LOG_ERROR("Failed to get valid current measurement %s", strerror(currentRes));
            return -1; 
        }

        if (power <= 0.05 && current >= 0 && current <= 0.01) { // Charging done
            break;
        }

        if (current < 0) { // Charger was unplugged
            return 0;
        }

        double delta_time = calculateDeltaTime(&previous_time);
        double time_hours = (delta_time - 0.6) / 3600.00;

        *soc_mem_ref = updateStateOfCharge(*soc_mem_ref, current, time_hours);

        #if DATA_LOGGER_ENABLED
            float voltage;
            float voltageRes = tryReadVoltage(&voltage);
            if (voltageRes < 0) {
                LOG_ERROR("Failed to get valid voltage measurement %s", strerror(voltageRes));
            }

            logMessages(current, power, voltage, delta_time, *soc_mem_ref);
        #endif

        *soc_mem_ref = trimSoc(*soc_mem_ref);

        sleep(5);
    }
    while (*soc_mem_ref < 1) {
        *(soc_mem_ref) += 0.005;
        #if INFO_LOGGER_ENABLED
            LOG_INFO("Gracefully increasing SoC : %.3f", *soc_mem_ref);
        #endif

        sleep(1);
    }
    *soc_mem_ref = trimSoc(*soc_mem_ref);
    return 0;
}

int calculateDischargingSoC(float *soc_mem_ref) {
    struct timeval previous_time;
    gettimeofday(&previous_time, NULL);

    float calibration = 0;
    int calibResult = dischargeCalibration(&calibration);
    if (calibResult < 0) {
        LOG_ERROR("Failed to get valid discharge calibration %s", strerror(calibResult));
        return -1;        
    }

    #if INFO_LOGGER_ENABLED
        LOG_INFO("Calibration SoC: %.3f", calibration);
    #endif
    if (fabs((*soc_mem_ref) - calibration) > 0.4) {
        *soc_mem_ref = calibration;
    }

    while (1) {
        float current;
        float currentRes = tryReadCurrent(&current);
        if (currentRes < 0) {
            LOG_ERROR("Failed to get valid current measurement %s", strerror(currentRes));
            return -1; 
        }

        if (current > 0) { // Charger was plugged
            return 0;
        }

        float voltage;
        float voltageRes = tryReadVoltage(&voltage);
        if (voltageRes < 0) {
            LOG_ERROR("Failed to get valid voltage measurement %s", strerror(voltageRes));
            return -1; 
        }

        if (voltage < MIN_VOLTAGE) {
            break;
        }

        double delta_time = calculateDeltaTime(&previous_time);
        double time_hours = (delta_time - 0.7) / 3600.00;

        *soc_mem_ref = updateStateOfCharge(*soc_mem_ref, current, time_hours);

        #if DATA_LOGGER_ENABLED
            float power;
            float powerRes = tryReadPower(&power);
            if (powerRes < 0) {
                LOG_ERROR("Failed to get valid power measurement %s", strerror(powerRes));
            }
            logMessages(current, power, voltage, delta_time, *soc_mem_ref);
        #endif

        *soc_mem_ref = trimSoc(*soc_mem_ref);

        sleep(5);
    }
    while (*soc_mem_ref > 0) {
        *(soc_mem_ref) -= 0.005; 
        #if INFO_LOGGER_ENABLED
            LOG_INFO("Gracefully decreasing SoC: %.3f", *soc_mem_ref);
        #endif
        sleep(1);
    }
    *soc_mem_ref = trimSoc(*soc_mem_ref);
    return 0;
}
