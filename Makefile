TOOLCHAIN ?= arm-linux-gnueabi
CC = $(TOOLCHAIN)-gcc

all:
	$(CC) main.c rfm69.c spi.c -o rfm69-gateway

clean:
	rm rfm69-gateway

.PHONY: all
