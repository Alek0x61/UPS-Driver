CC = gcc
CFLAGS = -D GPIOD
LIBS = -lgpiod

SRCS = main.c alertService/buzzer.c logger/logger.c electricalDataService/electricalData.c i2cService/i2cService.c dataLogger/dataLogger.c result.h
TARGET = main

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(LIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) -d

clean:
	rm -f $(TARGET)

.PHONY: run clean
 