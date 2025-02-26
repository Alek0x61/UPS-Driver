#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>

#define I2C_DEV             "/dev/i2c-1"
#define INA219_ADDR         0x43 

#define REG_BUS_VOLTAGE     0x02
#define REG_POWER           0x03
#define REG_CURRENT         0x04

#define MIN_VOLTAGE         3.100
#define MAX_VOLTAGE         4.000

// Used to convert raw ADC data into real-world units (eg. volts...)
#define POWER_LSB           0.003048
#define CURRENT_LSB         0.0001524
#define VOLTAGE_LSB         0.004

#define CALIBRATION_RETRIES 6

#define BATTERY_CAPACITY    1 // Ah

int fd;

int16_t readRegister(uint8_t reg) {
    uint8_t buf[2];

    if (write(fd, &reg, 1) != 1) {
        perror("Failed to set register address");
        return -1;
    }

    if (read(fd, buf, 2) != 2) {
        perror("Failed to read register");
        return -1;
    }

    uint16_t raw_value = (buf[0] << 8) | buf[1];

    if (raw_value > 32767) {  
        raw_value -= 65535;
    }

    return (int16_t)raw_value;
}

float convertToValidUnit(int16_t rawValue, float lsb) {
    return rawValue * lsb;
}

float getPower() {
    int16_t powerRaw = readRegister(REG_POWER);
    return (float)convertToValidUnit(powerRaw, POWER_LSB);
}

float getCurrent() {
    int16_t currentRaw = readRegister(REG_CURRENT);
    return convertToValidUnit(currentRaw, CURRENT_LSB);
}

float getVoltage() {
    int16_t busVoltageRaw = readRegister(REG_BUS_VOLTAGE);
    busVoltageRaw = (busVoltageRaw >> 3);
    return convertToValidUnit(busVoltageRaw, VOLTAGE_LSB);
}

float trimSoc(float soc) {
    if (soc < 0)
        return 0;
    if (soc > 100)
        return 100;
    return soc;
}

float dischargeCalibration() {
    float busVoltage = 0;
    for (char i = 0; i < CALIBRATION_RETRIES; i++) {
        busVoltage = getVoltage();
        sleep(2);
    }

    float soc = ((busVoltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE)) * 100;
    soc = trimSoc(soc);
    return soc;
}

float chargeCalibration() {
    float power = 0;
    for (char i = 0; i < CALIBRATION_RETRIES; i++) {
        power = getPower();
        sleep(2);
    }

    return 100.0 - ((power / 9.5) * 100.0);
}

float calibrateStartingPercentage() {
    float current = getCurrent();

    if (current > 0) {
        return chargeCalibration();
    }

    return dischargeCalibration();
}

void logDebug(double dt, float busVoltage, float current, float power, float integrated_charge, float soc) {
    FILE *file = fopen("battery_data.csv", "a");
    if (file == NULL) {
        perror("Failed to open CSV file");
        return;
    }

    fprintf(file, "%.6f,%.3f,%.3f,%.3f,%.4f,%.1f\n", dt, busVoltage, current, power, integrated_charge, soc);
    fclose(file);
}

void createLogFile() {
    FILE *file = fopen("battery_data.csv", "r");
    if (file == NULL) {
        file = fopen("battery_data.csv", "w");
        if (file != NULL) {
            fprintf(file, "Time(s),Voltage(V),Current(A),Power(W),Charge(Ah),SoC(%%)\n");
            fclose(file);
        }
    } else {
        fclose(file);
    }
}

int main() {
    fd = open(I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open I2C device");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, INA219_ADDR) < 0) {
        perror("Failed to set I2C address");
        close(fd);
        return 1;
    }

    float initial_soc = calibrateStartingPercentage();

    createLogFile();

    float initial_charge = BATTERY_CAPACITY * initial_soc / 100.0;

    float integrated_charge = 0.0;

    struct timeval previous_time, current_time;
    gettimeofday(&previous_time, NULL);
    
    while (1) {
        gettimeofday(&current_time, NULL);

        double dt = (double)(current_time.tv_sec - previous_time.tv_sec) +
                    (double)(current_time.tv_usec - previous_time.tv_usec) / 1000000.0;
        
        previous_time = current_time; 

        float busVoltage = getVoltage();
        float current = getCurrent();
        float power = getPower();

        integrated_charge += current * (dt / 3600.0);

        float soc = (initial_charge + integrated_charge) / BATTERY_CAPACITY * 100;
        soc = trimSoc(soc);

        logDebug(dt, busVoltage, current, power, integrated_charge, soc);
        sleep(5);
    }

    close(fd);
    return 0;
}
