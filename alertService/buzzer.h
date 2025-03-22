#ifndef BUZZER_H
#define BUZZER_H

#include <stdio.h>
#include <unistd.h>
#include "../result.h"
#include <errno.h>
#include <string.h>

#ifdef GPIOD
#include <gpiod.h>
#endif

Result setupAlertService(char pin);
void alert();
void disposeAlertService();

#endif
