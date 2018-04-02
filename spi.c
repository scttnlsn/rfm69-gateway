#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "spi.h"

int spi_init(spi_device_t *device) {
  int ret = 0;

  if (device->num == 0) {
    device->fd = open("/dev/spidev0.0", O_RDWR);
  } else if (device->num == 1) {
    device->fd = open("/dev/spidev0.1", O_RDWR);
  } else {
    perror("error: spi: invalid device");
    return -1;
  }

  if (device->fd < 0) {
    perror("error: spi: could not open device");
    return -1;
  }

  ret = ioctl(device->fd, SPI_IOC_WR_MODE, &device->mode);
  if (ret < 0) {
    perror("error: spi: could not set WR mode");
    return -1;
  }

  ret = ioctl(device->fd, SPI_IOC_RD_MODE, &device->mode);
  if (ret < 0) {
    perror("error: spi: could not set RD mode");
    return -1;
  }

  ret = ioctl(device->fd, SPI_IOC_WR_BITS_PER_WORD, &device->bits_per_word);
  if (ret < 0) {
    perror("error: spi: could not set WR bits per word");
    return -1;
  }

  ret = ioctl(device->fd, SPI_IOC_RD_BITS_PER_WORD, &device->bits_per_word);
  if (ret < 0) {
    perror("error: spi: could not set RD bits per word");
    return -1;
  }

  ret = ioctl(device->fd, SPI_IOC_WR_MAX_SPEED_HZ, &device->speed);
  if (ret < 0) {
    perror("error: spi: could not set WR speed");
    return -1;
  }

  ret = ioctl(device->fd, SPI_IOC_RD_MAX_SPEED_HZ, &device->speed);
  if (ret < 0) {
    perror("error: spi: could not set RD speed");
    return -1;
  }

  return ret;
}

int spi_transfer(spi_device_t *device, unsigned char *data, int length) {
  int ret = -1;
  struct spi_ioc_transfer spi[length];

  // transfer single byte at a time
  for (int i = 0; i < length; i++) {
    memset(&spi[i], 0, sizeof (spi[i]));

    spi[i].tx_buf = (unsigned long)(data + i);
    spi[i].rx_buf = (unsigned long)(data + i);
    spi[i].len = sizeof(*(data + i)) ;
    spi[i].delay_usecs = 0;
    spi[i].speed_hz = device->speed;
    spi[i].bits_per_word = device->bits_per_word;
    spi[i].cs_change = 0;
  }

  ret = ioctl(device->fd, SPI_IOC_MESSAGE(length), &spi) ;
  if (ret < 0) {
    perror("error: spi: could not transfer");
    return ret;
  }

  return ret;
}
