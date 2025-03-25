#include "../include/electrical_data.h"

int fd;

void configureElectricalData(int p_fd) {
    fd = p_fd;
}

int tryReadRegister(uint8_t reg, int16_t* result) {
    uint8_t buf[2];

    if (write(fd, &reg, 1) != 1) {
        return errno;
    }

    if (read(fd, buf, 2) != 2) {
        return errno;
    }

    uint16_t raw_value = (buf[0] << 8) | buf[1];

    if (raw_value > 32767) {  
        raw_value -= 65535;
    }

    *result = (int16_t)raw_value;
    return 0;
}

float convertToValidUnit(int16_t rawValue, float lsb) {
    return rawValue * lsb;
}

int tryReadPower(float* power) {
    int attempts = 0;
    int16_t powerRaw;
    int result;
    while (attempts < MAX_RETRIES) {
        result = tryReadRegister(REG_POWER, &powerRaw);
        if (result == 0) {
            *power = (float)convertToValidUnit(powerRaw, POWER_LSB);
            return 0;
        }
        attempts++;
        sleep(1);
    }
    return result;
}

int tryReadCurrent(float* current) {
    int attempts = 0;
    int16_t currentRaw;
    int result;
    while (attempts < MAX_RETRIES) {
        result = tryReadRegister(REG_CURRENT, &currentRaw);
        if (result == 0) {
            *current = (float)convertToValidUnit(currentRaw, CURRENT_LSB);
            return 0;
        }
        attempts++;
        sleep(1);
    }
    return result;
}

int tryReadVoltage(float* voltage) {
    int attempts = 0;
    int16_t voltageRaw;
    int result;
    while (attempts < MAX_RETRIES) {
        result = tryReadRegister(REG_BUS_VOLTAGE, &voltageRaw);
        if (result == 0) {
            voltageRaw = (voltageRaw >> 3);
            *voltage = (float)convertToValidUnit(voltageRaw, VOLTAGE_LSB);
            return 0;
        }
        attempts++;
        sleep(1);
    }
    return result;
}

int dischargeCalibration(float* calibration) {
    float busVoltage = 0;
    int result = tryReadVoltage(&busVoltage);
    *calibration = (busVoltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE);
    return result;
}

int chargeCalibration(float* calibration) {
    float power = 0;
    int result = tryReadVoltage(&power);
    *calibration = 1 - (power / MAX_POWER);
    return result;
}