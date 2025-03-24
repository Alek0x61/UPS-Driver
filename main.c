#include <stdio.h> 
#include <stdlib.h>
#include <stdint.h> 
#include <string.h>
#include <errno.h>
#include <fcntl.h> 
#include <unistd.h>      
#include <sys/mman.h>

#include "include/battery_soc.h"
#include "include/i2c_service.h"
#include "globalConfig.h"
#if ALERT_ENABLED
    #include "include/buzzer.h"
#endif
#include "include/logger.h"
#include "include/data_logger.h"

typedef struct {
    int i2cFd;
    int fileFd;
    int status;
} SetupResult;

void cleanup(int i2cFd, int fd_file, float *soc_mem_ref) {
    if (i2cFd != -1) {
        close(i2cFd);
    }
    if (fd_file != -1) {
        close(fd_file);
    }
    if (soc_mem_ref != MAP_FAILED && soc_mem_ref != NULL) {
        munmap(soc_mem_ref, DATA_SIZE);
    }
    shm_unlink(SHM_BACKUP);
    #if ALERT_ENABLED
        disposeAlertService();
    #endif
    disposeLogger();
}

Result configureINA219(int i2cFd) {
    Result res;
    res.status = 0;

    uint8_t buf[3];
    buf[0] = REG_CALIBRATION;
    buf[1] = (CALIBRATION_VALUE >> 8) & 0xFF;
    buf[2] = CALIBRATION_VALUE & 0xFF;

    if (write(i2cFd, buf, 3) != 3) {
        snprintf(res.message, sizeof(res.message), "Failed to write calibration register: %s", strerror(errno));
        res.status = -1;
    }

    return res;
}

SetupResult setup() {
    SetupResult res;
    res.i2cFd = -1;
    res.fileFd = -1;
    res.status = -1;

    if (initLog(MESSAGE_LOGGER_PATH) == -1) {
        return res;
    }

    Result resI2C = configureI2C(&res.i2cFd);
    if (resI2C.status == -1) {
        cleanup(res.i2cFd, res.fileFd, NULL);
        LOG_ERROR(resI2C.message);
        return res;
    }

    Result resIn1219 = configureINA219(res.i2cFd);
    if (resIn1219.status == -1) {
        cleanup(res.i2cFd, res.fileFd, NULL);
        LOG_ERROR(resI2C.message);
        return res;
    }

    #if ALERT_ENABLED
        Result resAlertService = setupAlertService(ALERT_PIN);
        if (resAlertService.status == -1) {
            cleanup(res.i2cFd, res.fileFd, NULL);
            LOG_ERROR(resAlertService.message);
            return res;
        }
    #endif

    #if DATA_LOGGER_ENABLED
        Result resCreateLog = createLogFile(DATA_LOGGER_PATH);
        if (resCreateLog.status == -1) {
            cleanup(res.i2cFd, res.fileFd, NULL);
            LOG_ERROR(resCreateLog.message);
            return res;
        }
    #endif

    res.fileFd = open(SHM_BACKUP, O_CREAT | O_RDWR, RW_PERMISSION);
    if (res.fileFd == -1) {
        LOG_ERROR("%s %s", "Failed to open shared memory file:", strerror(errno));
        cleanup(res.i2cFd, res.fileFd, NULL);
        return res;
    }

    ftruncate(res.fileFd, DATA_SIZE);

    configureElectricalData(res.i2cFd);
    
    return res;
}

int main() {
    SetupResult res = setup();

    float *soc_mem_ref = mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, res.fileFd, 0);
    if (soc_mem_ref == MAP_FAILED) {
        LOG_ERROR("%s %s", "mmap failed:", strerror(errno));
        cleanup(res.i2cFd, res.fileFd, soc_mem_ref);
        return -1;
    }

    #if INFO_LOGGER_ENABLED
        LOG_INFO("Initial SoC: %.3f", *soc_mem_ref);
    #endif

    while(1) {
        BatteryState state = getState();
        switch (state) {
            case CHARGING:
                #if INFO_LOGGER_ENABLED
                    LOG_INFO("CHARGING\n");
                #endif
                calculateChargingSoC(soc_mem_ref);
                break;
            case DISCHARGING:
                #if INFO_LOGGER_ENABLED
                    LOG_INFO("DISCHARGING\n");
                #endif
                calculateDischargingSoC(soc_mem_ref);
                system("sudo shutdown -h now");
                break;
            case ACPOWER:
                #if INFO_LOGGER_ENABLED
                    LOG_INFO("ACPOWER\n");
                #endif
                break;
            default: //DEPLETED
                #if INFO_LOGGER_ENABLED
                    LOG_INFO("DEPLETED\n");
                #endif
                #if ALERT_ENABLED 
                    alert();
                #endif          
                system("sudo shutdown -h now");
                break;
        }
        sleep(1);
    }

    cleanup(res.i2cFd, res.fileFd, soc_mem_ref);

    return 0;
}
