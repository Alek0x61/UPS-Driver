CC = gcc

SRCS = main.c
TARGET = main

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) -d

clean:
	rm -f $(TARGET)

.PHONY: run clean
 