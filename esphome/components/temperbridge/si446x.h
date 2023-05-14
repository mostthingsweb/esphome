#ifndef TEMPERF_BRIDGE_ALEXA_SI446X_H
#define TEMPERF_BRIDGE_ALEXA_SI446X_H

#include <cstdint>

#define SI446X_CMD_PART_INFO 0x01
#define SI446X_CMD_SET_PROPERTY 0x11
#define SI446X_CMD_GET_PROPERTY 0x12
#define SI446X_CMD_FIFO_INFO 0x15
#define SI446X_CMD_GET_INT_STATUS 0x20
#define SI446X_CMD_START_TX 0x31
#define SI446X_CMD_WRITE_TX_FIFO 0x66
#define SI446X_CMD_READ_CMD_BUFF 0x44
#define SI446X_CMD_FRR_A_READ 0x50

namespace esphome {
namespace temperbridge {

struct Si446xChipInfoResp {
  uint8_t chiprev;
  uint16_t part;
  uint8_t prbuild;
  uint16_t id;
  uint8_t customer;
  uint8_t romid;
} __attribute__((packed));

struct Si446xGetIntStatusResp {
  uint8_t int_pend;
  uint8_t int_status;
  uint8_t ph_pend;
  uint8_t ph_status;
  uint8_t modem_pend;
  uint8_t modem_status;
  uint8_t chip_pend;
  uint8_t chip_status;

  void print();
} __attribute__((packed));

struct Si446xFifoInfoResp {
  uint8_t rx_fifo_count;
  uint8_t tx_fifo_space;
} __attribute__((packed));

struct Si446xGetPropertyArgs {
  uint8_t group;
  uint8_t num_props;
  uint8_t start_prop;
} __attribute__((packed));

struct Si446xSetPropertyArgs {
  uint8_t group;
  uint8_t num_props;
  uint8_t start_prop;
} __attribute__((packed));

}  // namespace temperbridge
}  // namespace esphome

#endif  // TEMPERF_BRIDGE_ALEXA_SI446X_H
