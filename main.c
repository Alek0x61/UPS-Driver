#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "alertService/buzzer.h"
#include "logger/logger.h"

#define LOG_FILE_NAME        "battery.log"

#define I2C_DEV              "/dev/i2c-1"
#define INA219_ADDR          0x43 
#define ALERT_PIN            21

#define REG_BUS_VOLTAGE      0x02
#define REG_POWER            0x03
#define REG_CURRENT          0x04

#define MIN_VOLTAGE_CUTOFF   3.400
#define MIN_VOLTAGE_WARNING  3.480
#define MAX_VOLTAGE          4.000
#define MAX_POWER            9.0

// Used to convert raw ADC data into real-world units (eg. volts...)
#define POWER_LSB            0.003048
#define CURRENT_LSB          0.0001524
#define VOLTAGE_LSB          0.004

#define CALIBRATION_RETRIES  3

#define BATTERY_CAPACITY     1 // Ah

#define SHM_BACKUP           "battery_shm"
#define DATA_SIZE            sizeof(float)
#define RW_PERMISSION        0600

#define REG_CALIBRATION      0x05
#define CALIBRATION_VALUE    26868

int i2c_fd;
int fd_file;

enum State { 
    Charging = 1,
    Discharging = 0
};

int configureINA219() {
    uint8_t buf[3];
    buf[0] = REG_CALIBRATION;
    buf[1] = (CALIBRATION_VALUE >> 8) & 0xFF;
    buf[2] = CALIBRATION_VALUE & 0xFF;

    if (write(i2c_fd, buf, 3) != 3) {
        LOG_ERROR("%s %s", "Failed to write calibration register:", strerror(errno));
        return -1;
    }

    return 0;
}

int configureI2C() {
    i2c_fd = open(I2C_DEV, O_RDWR);
    if (i2c_fd < 0) {
        LOG_ERROR("%s %s", "Failed to open I2C device:", strerror(errno));
        return -1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, INA219_ADDR) < 0) {
        LOG_ERROR("%s %s", "Failed to open I2C device:", strerror(errno));
        close(i2c_fd);
        return -1;
    }

    return 0;
}

int configureAlertService() {
    Result res = setupAlertService(ALERT_PIN);
    if (res.status == -1) {
        LOG_ERROR(res.message);
        return -1;
    }

    return 0;
}

int tryReadRegister(uint8_t reg, int16_t* result) {
    uint8_t buf[2];

    if (write(i2c_fd, &reg, 1) != 1) {
        LOG_ERROR("%s %s", "Failed to set register address:", strerror(errno));
        return -1;
    }

    if (read(i2c_fd, buf, 2) != 2) {
        LOG_ERROR("%s %s", "Failed to read register:", strerror(errno));
        return -1;
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

float getPower() {
    int16_t powerRaw;
    if (tryReadRegister(REG_POWER, &powerRaw) == -1)
        return NAN;

    return (float)convertToValidUnit(powerRaw, POWER_LSB);
}

float getCurrent() {
    int16_t currentRaw;
    if (tryReadRegister(REG_CURRENT, &currentRaw) == -1)
        return NAN;

    return convertToValidUnit(currentRaw, CURRENT_LSB);
}

float getVoltage() {
    int16_t busVoltageRaw;
    if (tryReadRegister(REG_BUS_VOLTAGE, &busVoltageRaw) == -1)
        return NAN;

    busVoltageRaw = (busVoltageRaw >> 3);
    return convertToValidUnit(busVoltageRaw, VOLTAGE_LSB);
}

float trimSoc(float soc) {
    if (soc < 0)
        return 0;
    if (soc > 1)
        return 1;
    return soc;
}

float dischargeCalibration() {
    float busVoltage = 0;
    for (char i = 0; i < CALIBRATION_RETRIES; i++) {
        busVoltage = getVoltage();
        sleep(2);
    }

    float soc = (busVoltage - MIN_VOLTAGE_WARNING) / (MAX_VOLTAGE - MIN_VOLTAGE_WARNING);
    return soc;
}

float chargeCalibration() {
    float power = 0;
    for (char i = 0; i < CALIBRATION_RETRIES; i++) {
        power = getPower();
        sleep(1);
    }
    return 1 - (power / MAX_POWER);
}

int getState(float current) {
    if (current > 0) {
        return Charging;
    }

    return Discharging;
}

double calculateDeltaTime(struct timeval previous_time) {
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    double delta_time = (double)(current_time.tv_sec - previous_time.tv_sec) +
                (double)(current_time.tv_usec - previous_time.tv_usec) / 1000000.0;
    
    previous_time = current_time; 

    if (delta_time >= 6) {
        delta_time = 5.005;
    }

    return delta_time;
}

float updateStateOfCharge(float soc, float current, float time_hours) {
    return soc + (current / BATTERY_CAPACITY) * time_hours;
}

void syncMemory(float *soc_mem_ref) {
    msync(soc_mem_ref, DATA_SIZE, MS_SYNC);
    fsync(fd_file);
}

int calculateChargingSoC(float *soc_mem_ref) {
    if (*soc_mem_ref == 0) {
        *soc_mem_ref = chargeCalibration();
        if (isnan(*soc_mem_ref)) {
            LOG_ERROR("Failed to calibrate SoC during charge");
            return -1; 
        }
    }

    struct timeval previous_time;
    gettimeofday(&previous_time, NULL);

    while (1) {
        float current = getCurrent();

        if (isnan(current)) {
            LOG_ERROR("Failed to get valid electrical measurements");
            return NULL; 
        }

        double delta_time = calculateDeltaTime(previous_time);
        double time_hours = delta_time / 3600.00;

        *soc_mem_ref = updateStateOfCharge(*soc_mem_ref, current, time_hours);

        if (current == 0) {
            return 0;
        }

        *soc_mem_ref = trimSoc(*soc_mem_ref);

        syncMemory(soc_mem_ref);

        sleep(5);
    }
}

int calculateDischargingSoC(float *soc_mem_ref) {
    if (*soc_mem_ref == 0) {
        *soc_mem_ref = dischargeCalibration();
        if (isnan(*soc_mem_ref)) {
            LOG_ERROR("Failed to calibrate SoC during discharge");
            return -1; 
        }
    }

    struct timeval previous_time;
    gettimeofday(&previous_time, NULL);

    while (1) {
        float voltage = getVoltage();
        float current = getCurrent();

        if (isnan(voltage) || isnan(current)) {
            LOG_ERROR("Failed to get valid electrical measurements");
            return NULL; 
        }

        double delta_time = calculateDeltaTime(previous_time);
        double time_hours = (delta_time - 0.2) / 3600.00;

        *soc_mem_ref = updateStateOfCharge(*soc_mem_ref, current, time_hours);

        if (voltage < MIN_VOLTAGE_CUTOFF) {
            return 0;
        }

        if (voltage < MIN_VOLTAGE_WARNING) {
            alert();
        }

        *soc_mem_ref = trimSoc(*soc_mem_ref);

        syncMemory(soc_mem_ref);

        sleep(5);
    }
}


int main() {
    int returnCode = 0;

    if (initLog(LOG_FILE_NAME) == -1) {
        return -1;
    }
    if (configureI2C() == -1) {
        disposeLogger();
        return -1;
    }
    if (configureINA219() == -1) {
        disposeLogger();
        return -1;
    }
    if (configureAlertService() == -1) {
        disposeLogger();
        return -1;       
    }

    fd_file = open(SHM_BACKUP, O_CREAT | O_RDWR, RW_PERMISSION);
    if (fd_file == -1) {
        LOG_ERROR("%s %s", "Failed to open shared memory file:", strerror(errno));
        return -1;
    }

    ftruncate(fd_file, DATA_SIZE);

    float *soc_mem_ref = mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_file, 0);
    if (soc_mem_ref == MAP_FAILED) {
        LOG_ERROR("%s %s", "mmap failed:", strerror(errno));
        close(fd_file);
        return -1;
    }

    while (1) {
        float initial_power = getPower();
        if (isnan(initial_power)) {
            returnCode = -1;
            break;
        }
        if (initial_power < 0.1 && *soc_mem_ref == 100.0) {
            continue;
        }

        float initial_current = getCurrent();
        if (isnan(initial_current)) {
            returnCode = -1;
            break;
        }

        int state = getState(initial_current);

        if (state == Charging) {
            if (calculateChargingSoC(soc_mem_ref) == -1) {
                returnCode = -1;
                break;
            }
            while (*soc_mem_ref < 100) {
                *(soc_mem_ref) += 1;
                syncMemory(soc_mem_ref);

                sleep(1);
            }
        }
        else {
            if (calculateDischargingSoC(soc_mem_ref) == -1) {
                returnCode = -1;
                break;
            }
            while (*soc_mem_ref > 0) {
                *(soc_mem_ref) -= 1; 
                syncMemory(soc_mem_ref);
 
                sleep(1);
            }
            system("sudo shutdown -h now");
        }
        sleep(5);
    }

    close(i2c_fd);
    close(fd_file);
    munmap(soc_mem_ref, DATA_SIZE);
    shm_unlink(SHM_BACKUP); 
    disposeAlertService();
    disposeLogger();

    return returnCode;
}
