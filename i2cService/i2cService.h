#ifndef I2CSERVICE_H
#define I2CSERVICE_H

#include <errno.h>
#include <string.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include "../result.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

Result configureI2C(int* fd);

#endif
