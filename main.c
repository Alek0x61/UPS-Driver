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
#include "electricalDataService/electricalData.h"
#include "i2cService/i2cService.h"
#include "dataLogger/dataLogger.h"

#define MESSAGE_LOGGER_PATH     "/var/lib/battery.log"
#define DATA_LOGGER_PATH        "/var/lib/battery_data.csv"
#define SHM_BACKUP              "/var/lib/battery_shm"
#define DATA_LOGGER_ENABLED     1
#define INFO_LOGGER_ENABLED     1
#define ALERT_ENABLED           1

#define ALERT_PIN               21
#define MIN_VOLTAGE_CUTOFF      3.300
#define LOW_BATTERY_ALERT       0.005
#define BATTERY_CAPACITY        1 // Ah

#define DATA_SIZE               sizeof(float)
#define RW_PERMISSION           0600

#define REG_CALIBRATION         0x05
#define CALIBRATION_VALUE       26868

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

float trimSoc(float soc) {
    if (soc < 0)
        return 0;
    if (soc > 1)
        return 1;
    return soc;
}

int getState(float current) {
    if (current > 0) {
        return Charging;
    }
    return Discharging;
}

double calculateDeltaTime(struct timeval* previous_time) {
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    double delta_time = (double)(current_time.tv_sec - previous_time->tv_sec) +
                (double)(current_time.tv_usec - previous_time->tv_usec) / 1000000.0;
    
    *previous_time = current_time; 

    if (delta_time > 7) {
        #if INFO_LOGGER_ENABLED
            LOG_INFO("delta time dillation: %.6f seconds\n", delta_time);
        #endif
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
    float calibration = chargeCalibration();
    if (isnan(calibration)) {
        LOG_ERROR("Failed to calibrate SoC during charge");
        return -1; 
    }

    if (abs(*soc_mem_ref - calibration) > 0) {
        *soc_mem_ref = calibration;
    }

    struct timeval previous_time;
    gettimeofday(&previous_time, NULL);

    while (1) {
        float current;
        float currentRes = tryReadCurrent(&current);
        if (currentRes < 0) {
            LOG_ERROR("Failed to get valid current measurement %s", strerror(currentRes));
            return -1; 
        }

        double delta_time = calculateDeltaTime(&previous_time);
        double time_hours = (delta_time - 0.8) / 3600.00;

        *soc_mem_ref = updateStateOfCharge(*soc_mem_ref, current, time_hours);

        #if DATA_LOGGER_ENABLED
            float voltage;
            float power;

            float voltageRes = tryReadVoltage(&voltage);
            if (voltageRes < 0) {
                LOG_ERROR("Failed to get valid voltage measurement %s", strerror(voltageRes));
            }

            float powerRes = tryReadPower(&power);
            if (powerRes < 0) {
                LOG_ERROR("Failed to get valid power measurement %s", strerror(powerRes));
            }
            logMessages(current, power, voltage, delta_time, *soc_mem_ref);
        #endif

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
        float voltage;
        float current;

        float voltageRes = tryReadVoltage(&voltage);
        if (voltageRes < 0) {
            LOG_ERROR("Failed to get valid voltage measurement %s", strerror(voltageRes));
            return -1; 
        }

        float currentRes = tryReadCurrent(&current);
        if (currentRes < 0) {
            LOG_ERROR("Failed to get valid current measurement %s", strerror(currentRes));
            return -1; 
        }

        double delta_time = calculateDeltaTime(&previous_time);
        double time_hours = (delta_time - 0.8) / 3600.00;

        *soc_mem_ref = updateStateOfCharge(*soc_mem_ref, current, time_hours);

        if (voltage < MIN_VOLTAGE_CUTOFF) {
            return 0;
        }

        #if ALERT_ENABLED 
            if (*soc_mem_ref <= LOW_BATTERY_ALERT) {
                alert();
            }
        #endif

        #if DATA_LOGGER_ENABLED
            float power;
            float powerRes = tryReadPower(&power);
            if (powerRes < 0) {
                LOG_ERROR("Failed to get valid power measurement %s", strerror(powerRes));
            }
            logMessages(current, power, voltage, delta_time, *soc_mem_ref);
        #endif

        *soc_mem_ref = trimSoc(*soc_mem_ref);

        syncMemory(soc_mem_ref);

        sleep(5);
    }
}

void cleanup(int i2c_fd, int fd_file, float *soc_mem_ref) {
    if (i2c_fd != -1) {
        close(i2c_fd);
    }
    if (fd_file != -1) {
        close(fd_file);
    }
    if (soc_mem_ref != MAP_FAILED && soc_mem_ref != NULL) {
        munmap(soc_mem_ref, DATA_SIZE);
    }
    shm_unlink(SHM_BACKUP);
    disposeAlertService();
    disposeLogger();
}

int isOnChargerFullyCharged() {
    float power;
    float powerRes = tryReadPower(&power);
    if (powerRes < 0) {
        return -1;
    }
    if (power < 0.1) {
        return 1;
    }
    return 0;
}

int isBatteryDepleted() {
    float voltage;
    tryReadVoltage(&voltage);
    return voltage < MIN_VOLTAGE_CUTOFF;
}

int main() {
    if (initLog(MESSAGE_LOGGER_PATH) == -1) {
        return -1;
    }

    Result resI2C = configureI2C(&i2c_fd);
    if (resI2C.status == -1) {
        LOG_ERROR(resI2C.message);
        cleanup(i2c_fd, fd_file, NULL);
        return -1;
    }

    if (configureINA219() == -1) {
        cleanup(i2c_fd, fd_file, NULL);
        return -1;
    }

    Result resAlertService = setupAlertService(ALERT_PIN);
    if (resAlertService.status == -1) {
        LOG_ERROR(resAlertService.message);
        cleanup(i2c_fd, fd_file, NULL);
        return -1;
    }

    configureElectricalData(i2c_fd);

    fd_file = open(SHM_BACKUP, O_CREAT | O_RDWR, RW_PERMISSION);
    if (fd_file == -1) {
        LOG_ERROR("%s %s", "Failed to open shared memory file:", strerror(errno));
        cleanup(i2c_fd, fd_file, NULL);
        return -1;
    }

    ftruncate(fd_file, DATA_SIZE);

    float *soc_mem_ref = mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_file, 0);
    if (soc_mem_ref == MAP_FAILED) {
        LOG_ERROR("%s %s", "mmap failed:", strerror(errno));
        cleanup(i2c_fd, fd_file, soc_mem_ref);
        return -1;
    }

    #if DATA_LOGGER_ENABLED
        Result resCreateLog = createLogFile(DATA_LOGGER_PATH);
        if (resCreateLog.status == -1) {
            LOG_ERROR(resCreateLog.message);
        }
    #endif

    #if INFO_LOGGER_ENABLED
        LOG_INFO("Initial SoC: %.3f\n", *soc_mem_ref);
    #endif

    while (1) {
        if (isOnChargerFullyCharged() == 1)
            continue;

        if (isBatteryDepleted() == 1) {
            #if ALERT_ENABLED 
                alert();
            #endif
            system("sudo shutdown -h now");
        }

        float initialCurrent;
        float initialCurrentRes = tryReadCurrent(&initialCurrent);
        if (initialCurrentRes < 0) {
            LOG_ERROR("Failed to get valid current measurement %s", strerror(initialCurrentRes));
            continue;
        }
        int state = getState(initialCurrent);

        if (state == Discharging) {
            if (calculateDischargingSoC(soc_mem_ref) == -1) {
                cleanup(i2c_fd, fd_file, soc_mem_ref);
                return -1;
            }
            while (*soc_mem_ref > 0) {
                *(soc_mem_ref) -= 0.01; 
                LOG_INFO("Initial SoC: %.3f\n", *soc_mem_ref);

                #if ALERT_ENABLED 
                    if (*soc_mem_ref <= LOW_BATTERY_ALERT) {
                        alert();
                    }
                #endif

                syncMemory(soc_mem_ref);
                sleep(1);
            }
            *soc_mem_ref = trimSoc(*soc_mem_ref);
            syncMemory(soc_mem_ref);
            system("sudo shutdown -h now");

            return 0;
        }

        if (calculateChargingSoC(soc_mem_ref) == -1) {
            cleanup(i2c_fd, fd_file, soc_mem_ref);
            return -1;
        }
        while (*soc_mem_ref < 1) {
            *(soc_mem_ref) += 0.01;
            LOG_INFO("Initial SoC: %.3f\n", *soc_mem_ref);

            syncMemory(soc_mem_ref);
            sleep(1);
        }
        *soc_mem_ref = trimSoc(*soc_mem_ref);
        syncMemory(soc_mem_ref);

        sleep(5);
    }

    cleanup(i2c_fd, fd_file, soc_mem_ref);

    return 0;
}