#include "i2cService.h"

#define I2C_DEV              "/dev/i2c-1"
#define INA219_ADDR          0x43 

#define REG_CALIBRATION      0x05
#define CALIBRATION_VALUE    26868

Result configureI2C(int* fd) {
    Result res;

    *fd = open(I2C_DEV, O_RDWR);
    if (*fd < 0) {
        snprintf(res.message, sizeof(res.message), "Failed to open I2C device: %s", strerror(errno));
        res.status = -1;
        return res;
    }

    if (ioctl(*fd, I2C_SLAVE, INA219_ADDR) < 0) {
        snprintf(res.message, sizeof(res.message), "Failed to open I2C device: %s", strerror(errno));
        res.status = -1;
        close(*fd);
        return res;
    }

    res.status = 0;
    return res;
}