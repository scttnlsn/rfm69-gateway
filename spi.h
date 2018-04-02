#ifndef __SPI_H__
#define __SPI_H__

#include <linux/spi/spidev.h>

typedef struct {
  int num;
  unsigned char mode;
  unsigned char bits_per_word;
  unsigned int speed;
  int fd;
} spi_device_t;

int spi_init(spi_device_t *device);
int spi_transfer(spi_device_t *device, unsigned char *data, int length);

#endif
