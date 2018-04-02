#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rfm69.h"
#include "rfm69registers.h"
#include "spi.h"

spi_device_t *_spi_device;
char _mode;
char _address;
char _power;

void rfm69_write_reg(char address, char value);
char rfm69_read_reg(char address);

// default register values
// most of these are from https://github.com/LowPowerLab/RFM69
const char defaults[][2] = {
  { REG_OPMODE, RF_OPMODE_SEQUENCER_ON | RF_OPMODE_LISTEN_OFF | RF_OPMODE_STANDBY },
  { REG_DATAMODUL, RF_DATAMODUL_DATAMODE_PACKET | RF_DATAMODUL_MODULATIONTYPE_FSK | RF_DATAMODUL_MODULATIONSHAPING_00 },

  // 4.8 kbps
  { REG_BITRATEMSB, RF_BITRATEMSB_55555 },
  { REG_BITRATELSB, RF_BITRATELSB_55555 },

  // 5 kHz
  { REG_FDEVMSB, RF_FDEVMSB_50000 },
  { REG_FDEVLSB, RF_FDEVLSB_50000 },

  // 10.4 kHz
  { REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_16 | RF_RXBW_EXP_2 },

  // DIO0 IRQ
  // { REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01 },

  // -114 dBm
  { REG_RSSITHRESH, 220 },

  { REG_PREAMBLELSB, RF_PREAMBLESIZE_LSB_VALUE },
  { REG_SYNCCONFIG, RF_SYNC_ON | RF_SYNC_FIFOFILL_AUTO | RF_SYNC_SIZE_2 | RF_SYNC_TOL_0 },
  { REG_SYNCVALUE1, 0x2D },
  { REG_PACKETCONFIG1, RF_PACKET1_FORMAT_VARIABLE | RF_PACKET1_DCFREE_OFF | RF_PACKET1_CRC_ON | RF_PACKET1_CRCAUTOCLEAR_ON | RF_PACKET1_ADRSFILTERING_OFF },
  { REG_PAYLOADLENGTH, 66 },
  { REG_FIFOTHRESH, RF_FIFOTHRESH_TXSTART_FIFONOTEMPTY | RF_FIFOTHRESH_VALUE },
  { REG_PACKETCONFIG2, RF_PACKET2_RXRESTARTDELAY_2BITS | RF_PACKET2_AUTORXRESTART_ON | RF_PACKET2_AES_OFF },
  { REG_TESTDAGC, RF_DAGC_IMPROVED_LOWBETA0 },
  { 255, 0 }
};

int rfm69_initialize(spi_device_t *spi_device, rfm69_freq_t freq, char network_id) {
  _spi_device = spi_device;

  // sync
  do {
    rfm69_write_reg(REG_SYNCVALUE1, 0xAA);
  } while (rfm69_read_reg(REG_SYNCVALUE1) != 0xAA);

  do {
    rfm69_write_reg(REG_SYNCVALUE1, 0x55);
  } while (rfm69_read_reg(REG_SYNCVALUE1) != 0x55);

  // write default register values
  for (int i = 0; defaults[i][0] != 255; i++) {
    rfm69_write_reg(defaults[i][0], defaults[i][1]);
  }

  rfm69_write_reg(REG_SYNCVALUE2, network_id);

  // set carrier frequency
  switch (freq) {
    case RFM69_FREQ_315MHZ:
      rfm69_write_reg(REG_FRFMSB, RF_FRFMSB_315);
      rfm69_write_reg(REG_FRFMID, RF_FRFMID_315);
      rfm69_write_reg(REG_FRFLSB, RF_FRFLSB_315);
      break;

    case RFM69_FREQ_433MHZ:
      rfm69_write_reg(REG_FRFMSB, RF_FRFMSB_433);
      rfm69_write_reg(REG_FRFMID, RF_FRFMID_433);
      rfm69_write_reg(REG_FRFLSB, RF_FRFLSB_433);
      break;

    case RFM69_FREQ_868MHZ:
      rfm69_write_reg(REG_FRFMSB, RF_FRFMSB_868);
      rfm69_write_reg(REG_FRFMID, RF_FRFMID_868);
      rfm69_write_reg(REG_FRFLSB, RF_FRFLSB_868);
      break;

    case RFM69_FREQ_915MHZ:
      rfm69_write_reg(REG_FRFMSB, RF_FRFMSB_915);
      rfm69_write_reg(REG_FRFMID, RF_FRFMID_915);
      rfm69_write_reg(REG_FRFLSB, RF_FRFLSB_915);
      break;

    default:
      perror("error: rfm69: invalid frequency");
      return -1;
  }

  // reset encryption key since it persists between resets
  rfm69_encrypt(0);

  rfm69_set_mode(RFM69_MODE_STANDBY);
  while (!(rfm69_read_reg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY));

  return 0;
}

void rfm69_encrypt(const char *key) {
  char data[17];

  rfm69_set_mode(RFM69_MODE_STANDBY);

  if (key != 0) {
    data[0] = REG_AESKEY1 | 0x80;
    memcpy(data + 1, key, 16);

    spi_transfer(_spi_device, data, 17);
    usleep(5);
  }

  rfm69_write_reg(REG_PACKETCONFIG2, (rfm69_read_reg(REG_PACKETCONFIG2) & 0xFE) | (key ? 1 : 0));
}

void rfm69_set_address(char address) {
  _address = address;

  // only receive packets matching this address
  rfm69_write_reg(REG_NODEADRS, _address);
}

void rfm69_set_power(char power) {
  _power = power;
  if (_power > 31) {
    _power = 31;
  }

  rfm69_write_reg(REG_PALEVEL, (rfm69_read_reg(REG_PALEVEL) & 0xE0) | _power);
}

void rfm69_set_mode(char mode) {
  if (mode == _mode) {
    return;
  }

  char opmode = rfm69_read_reg(REG_OPMODE) & 0xE3;
  char new_mode;

  switch (mode) {
    case RFM69_MODE_TX:
      new_mode = opmode | RF_OPMODE_TRANSMITTER;
      break;

    case RFM69_MODE_RX:
      new_mode = opmode | RF_OPMODE_RECEIVER;
      break;

    case RFM69_MODE_SYNTH:
      new_mode = opmode | RF_OPMODE_SYNTHESIZER;
      break;

    case RFM69_MODE_STANDBY:
      new_mode = opmode | RF_OPMODE_STANDBY;
      break;

    case RFM69_MODE_SLEEP:
      new_mode = opmode | RF_OPMODE_SLEEP;
      break;

    default:
      return;
  }

  rfm69_write_reg(REG_OPMODE, new_mode);

  if (_mode == RFM69_MODE_SLEEP) {
    while (!(rfm69_read_reg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY));
  }

  _mode = mode;
}

void rfm69_sleep(void) {
  rfm69_set_mode(RFM69_MODE_SLEEP);
}

int rfm69_read_rssi(char force_trigger) {
  int rssi = 0;

  if (force_trigger == 1) {
    // RSSI trigger not needed if DAGC is in continuous mode
    rfm69_write_reg(REG_RSSICONFIG, RF_RSSI_START);

    while (!(rfm69_read_reg(REG_RSSICONFIG) & RF_RSSI_DONE));
  }

  rssi = -rfm69_read_reg(REG_RSSIVALUE);
  rssi >>= 1;
  rssi += 20;
  return rssi;
}

void rfm69_receive(rfm69_packet_t *packet) {
  memset(packet, 0, sizeof(packet));

  unsigned long timeout = 10000;
  char data[67];

  // until data is received or timeout is reached
  while (1) {
    if (rfm69_read_reg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY) {
      // avoid RX deadlocks
      rfm69_write_reg(REG_PACKETCONFIG2, (rfm69_read_reg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART);
    }

    rfm69_set_mode(RFM69_MODE_RX);

    while (!(rfm69_read_reg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)) {
      timeout--;

      if (timeout == 0) {
        rfm69_set_mode(RFM69_MODE_STANDBY);
        return;
      }
    }

    rfm69_set_mode(RFM69_MODE_STANDBY);

    memset(data, 0, sizeof(data));
    data[0] = REG_FIFO & 0x7F;
    spi_transfer(_spi_device, data, 3);
    usleep(5);

    char payload_length = data[1];
    if (payload_length > 66) {
      payload_length = 66;
    }

    char target_id = data[2];
    if (target_id == _address || target_id == RFM69_BROADCAST_ADDR) {
      memset(data, 0, sizeof(data));
      data[0] = REG_FIFO & 0x7F;
      spi_transfer(_spi_device, data, payload_length);

      packet->sender = data[1];
      packet->ctlbyte = data[2];
      packet->length = payload_length - 3;
      memcpy(packet->data, data + 3, packet->length);
      packet->data[packet->length] = '\0';

      return;
    }
  }
}

void rfm69_send(char to_address, const void* buffer, char size, char request_ack) {
  char data[63];

  // avoid RX deadlocks
  rfm69_write_reg(REG_PACKETCONFIG2, (rfm69_read_reg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART);

  // turn off receiver to prevent reception while filling fifo
  rfm69_set_mode(RFM69_MODE_STANDBY);

  while (!(rfm69_read_reg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY));

  if (size > RFM69_MAX_DATA_LEN) {
    size = RFM69_MAX_DATA_LEN;
  }

  memset(data, 0, sizeof(data));
  data[0] = REG_FIFO | 0x80;
  data[1] = size + 3;
  data[2] = to_address;
  data[3] = _address;

  if (request_ack) {
    data[4] = 0x40;
  }

  for (int i = 0; i < size; i++) {
    data[i + 5] = ((char*) buffer)[i];
  }

  spi_transfer(_spi_device, data, size + 5);

  rfm69_set_mode(RFM69_MODE_TX);
  while (!(rfm69_read_reg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT));

  rfm69_set_mode(RFM69_MODE_STANDBY);
}

void rfm69_write_reg(char addr, char value) {
  char data[2];
  data[0] = addr | 0x80;
  data[1] = value;

  spi_transfer(_spi_device, data, 2);
  usleep(5);
}

char rfm69_read_reg(char addr) {
  char data[2];
  data[0] = addr & 0x7F;
  data[1] = 0;

  spi_transfer(_spi_device, data, 2);
  usleep(5);

  return data[1];
}
