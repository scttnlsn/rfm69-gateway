#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rfm69.h"
#include "spi.h"

#define FREQUENCY RFM69_FREQ_915MHZ
#define NODEID 1
#define NETWORKID 100
#define TXPOWER 31

// must be 16 bits
#define CRYPTPASS "ABCDEFGHIJKLMNOP"

int main(int argc, char* argv[]) {
  spi_device_t spi_device;
  spi_device.num = 0;
  spi_device.mode = SPI_MODE_0;
  spi_device.speed = 2000000;
  spi_device.bits_per_word = 8;

  int ret = spi_init(&spi_device);
  if (ret < 0) {
    return -1;
  }

  if (rfm69_initialize(&spi_device, FREQUENCY, NETWORKID) < 0) {
    fprintf(stderr, "error: unable to initialize RFM69\n\r");
    exit(1);
  }

  rfm69_encrypt(CRYPTPASS);
  rfm69_set_power(TXPOWER);
  rfm69_set_address(NODEID);

  printf("listening...\r\n");

  while(1) {
    rfm69_packet_t packet;
    rfm69_receive(&packet);

    if (packet.length > 0) {
      int rssi = rfm69_read_rssi(0);
      printf("received: from=%d, length=%d, rssi=%d, data=%s\r\n", packet.sender, packet.length, rssi, packet.data);
    }
  }
}
