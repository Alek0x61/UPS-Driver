CC = gcc
CFLAGS = -D GPIOD
LIBS = -lgpiod

SRCS = main.c src/battery_soc.c src/buzzer.c src/logger.c src/electrical_data.c src/i2c_service.c src/data_logger.c include/types/result.h include/types/battery_state.h globalConfig.h
TARGET = main

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(LIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) -d

clean:
	rm -f $(TARGET)

.PHONY: run clean
 