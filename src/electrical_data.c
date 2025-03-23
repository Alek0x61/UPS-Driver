#include "../include/electrical_data.h"

#define REG_BUS_VOLTAGE      0x02
#define REG_POWER            0x03
#define REG_CURRENT          0x04

// Used to convert raw ADC data into real-world units (eg. volts...)
#define POWER_LSB            0.003048
#define CURRENT_LSB          0.0001524
#define VOLTAGE_LSB          0.004

#define MIN_VOLTAGE          3.420
#define MAX_VOLTAGE          4.000
#define MAX_POWER            9.0

#define CALIBRATION_RETRIES  3

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

float tryReadPower(float* power) {
    int16_t powerRaw;
    int result = tryReadRegister(REG_POWER, &powerRaw);
    *power = (float)convertToValidUnit(powerRaw, POWER_LSB);
    return result;
}

float tryReadCurrent(float* current) {
    int16_t currentRaw;
    int result = tryReadRegister(REG_CURRENT, &currentRaw);
    *current = (float)convertToValidUnit(currentRaw, CURRENT_LSB);
    return result;
}

float tryReadVoltage(float* voltage) {
    int16_t voltageRaw;
    int result = tryReadRegister(REG_BUS_VOLTAGE, &voltageRaw);
    voltageRaw = (voltageRaw >> 3);
    *voltage = convertToValidUnit(voltageRaw, VOLTAGE_LSB);
    return result;
}

float dischargeCalibration() {
    float busVoltage = 0;
    for (char i = 0; i < CALIBRATION_RETRIES; i++) {
        tryReadVoltage(&busVoltage);
        sleep(2);
    }
    return (busVoltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE);
}

float chargeCalibration() {
    float power = 0;
    for (char i = 0; i < CALIBRATION_RETRIES; i++) {
        tryReadPower(&power);
        sleep(1);
    }
    return 1 - (power / MAX_POWER);
}