#include "buzzer.h"

struct gpiod_chip *chip;
struct gpiod_line *line;

Result setupAlertService(char pin) {
    Result res;

    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        snprintf(res.message, sizeof(res.message), "Failed to open gpiochip0: %s", strerror(errno));
        res.status = -1;
        return res;
    }

    line = gpiod_chip_get_line(chip, pin);
    if (!line) {
        snprintf(res.message, sizeof(res.message), "Failed to get GPIO line: %s", strerror(errno));
        res.status = -1;
        gpiod_chip_close(chip);
        return res;
    }

    if (gpiod_line_request_output(line, "buzzer", 0) == -1) {
        snprintf(res.message, sizeof(res.message), "Failed to set GPIO as output: %s", strerror(errno));
        res.status = -1;
        gpiod_chip_close(chip);
        return res;
    }

    res.status = 0;

    return res;
}

void alert() {
    gpiod_line_set_value(line, 1);
    sleep(1);
    gpiod_line_set_value(line, 0);
}

void disposeAlertService() {
    gpiod_line_release(line);
    gpiod_chip_close(chip);
}