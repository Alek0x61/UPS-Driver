#ifndef BUZZER_H
#define BUZZER_H

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef GPIOD
#include <gpiod.h>
#endif
#include "types/result.h"

extern struct gpiod_chip *chip;
extern struct gpiod_line *line;

Result setupAlertService(char pin);
void alert();
void disposeAlertService();

#endif
