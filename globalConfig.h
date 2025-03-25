#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#define MESSAGE_LOGGER_PATH     "/var/lib/battery.log"
#define DATA_LOGGER_PATH        "/var/lib/battery_data.csv"
#define SHM_BACKUP              "/var/lib/battery_shm"
#define DATA_LOGGER_ENABLED     1
#define INFO_LOGGER_ENABLED     1
#define ALERT_ENABLED           1

#define MIN_VOLTAGE             3.300
#define MAX_VOLTAGE             4.000
#define MAX_POWER               9.0

#define ALERT_PIN               21

#define LOW_BATTERY_ALERT       0.005
#define BATTERY_CAPACITY        1 // Ah

#define DATA_SIZE               sizeof(float)
#define RW_PERMISSION           0600

#define REG_CALIBRATION         0x05
#define CALIBRATION_VALUE       26868

#define REG_BUS_VOLTAGE      0x02
#define REG_POWER            0x03
#define REG_CURRENT          0x04

// Used to convert raw ADC data into real-world units (eg. volts...)
#define POWER_LSB            0.003048
#define CURRENT_LSB          0.0001524
#define VOLTAGE_LSB          0.004

#define MAX_RETRIES  3

#endif
