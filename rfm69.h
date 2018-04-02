#ifndef __RFM69_H__
#define __RFM69_H__

#include "spi.h"

#define RFM69_MAX_DATA_LEN 61
#define RFM69_BROADCAST_ADDR 255

typedef enum {
  RFM69_MODE_SLEEP,
  RFM69_MODE_STANDBY,
  RFM69_MODE_SYNTH,
  RFM69_MODE_RX,
  RFM69_MODE_TX
} rfm69_mode_t;

typedef enum {
  RFM69_FREQ_315MHZ,
  RFM69_FREQ_433MHZ,
  RFM69_FREQ_868MHZ,
  RFM69_FREQ_915MHZ
} rfm69_freq_t;

typedef struct {
  char sender;
  char ctlbyte;
  char length;
  char data[RFM69_MAX_DATA_LEN + 2];
} rfm69_packet_t;

int rfm69_initialize(spi_device_t *spi_device, rfm69_freq_t freq, char network_id);
void rfm69_encrypt(const char* key);
void rfm69_set_power(char power);
void rfm69_set_mode(char mode);
void rfm69_set_address(char address);
void rfm69_sleep(void);
int rfm69_read_rssi(char force_trigger);
void rfm69_receive(rfm69_packet_t *packet);
void rfm69_send(char to_address, const void* buffer, char size, char request_ack);

#endif
