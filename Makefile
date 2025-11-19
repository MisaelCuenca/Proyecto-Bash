CC = gcc
CFLAGS = -Wall -g

TARGET = CuencaN-bash

all: $(TARGET)

$(TARGET): CuencaN-bash.c
	$(CC) $(CFLAGS) -o $(TARGET) CuencaN-bash.c

clean:
	rm $(TARGET)
