#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

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

#endif
