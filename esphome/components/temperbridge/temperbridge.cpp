#include <cstring>
#include <cinttypes>

#include "esphome/core/helpers.h"
#include "si446x.h"
#include "radio_config_Si4463.h"

#include "temperbridge.h"

namespace esphome {
namespace temperbridge {

static const char *const TAG = "temperbridge";

const uint8_t SI4463_RADIO_CONFIGURATION_DATA_ARRAY[] = RADIO_CONFIGURATION_DATA_ARRAY;

void TemperBridgeComponent::setup() {
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
  this->interrupt_pin_->setup();

  // this->store_.pin = this->interrupt_pin_->to_isr();

  this->sdn_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sdn_pin_->setup();

  this->spi_setup();

  Component::setup();

  this->sdn_pin_->digital_write(true);
  delay(10);
  this->sdn_pin_->digital_write(false);
  delay(20);

  const Si446xChipInfoResp resp = si446x_part_info_();
  ESP_LOGCONFIG(TAG, "part %x", resp.part);
  ESP_LOGCONFIG(TAG, "rev %x", resp.chiprev);
  // https://community.silabs.com/s/article/using-part-info-command-to-identify-ezradio-pro-part-number?language=en_US
  ESP_LOGCONFIG(TAG, "romid %x", resp.romid);
  ESP_LOGCONFIG(TAG, "prbuild %x", resp.prbuild);

  si446x_configuration_init_(SI4463_RADIO_CONFIGURATION_DATA_ARRAY);

  Si446xGetIntStatusResp int_status;
  si446x_get_int_status(&int_status, true);
  //int_status.print();

  this->initialized_ = true;

  this->tune_channel_(this->channel_);
}

void TemperBridgeComponent::si446x_raw_command_(const uint8_t *tx_data, size_t tx_data_bytes, uint8_t *resp,
                                                size_t resp_bytes) {
  // Wait for CTS
  while (true) {
    this->enable();
    this->write_byte(SI446X_CMD_READ_CMD_BUFF);
    const uint8_t cts = this->read_byte();
    this->disable();
    if (cts == 0xFF) {
      break;
    }
    delay(20);
  }

  this->enable();
  this->write_array(tx_data, tx_data_bytes);
  this->disable();

  // TODO timeout
  if (resp) {
    while (true) {
      this->enable();
      this->write_byte(SI446X_CMD_READ_CMD_BUFF);
      const uint8_t cts = this->read_byte();
      if (cts != 0xFF) {
        this->disable();
        delay(20);
        continue;
      }

      this->read_array(resp, resp_bytes);
      this->disable();
      break;
    }
  }
}

Si446xChipInfoResp TemperBridgeComponent::si446x_part_info_() {
  static_assert(sizeof(Si446xChipInfoResp) == 8, "size wrong");
  Si446xChipInfoResp ret;

  this->si446x_execute_command_(SI446X_CMD_PART_INFO, nullptr, 0, (uint8_t *) &ret, sizeof(Si446xChipInfoResp));
  ret.part = convert_big_endian(ret.part);
  ret.id = convert_big_endian(ret.id);

  return ret;
}

void TemperBridgeComponent::si446x_execute_command_(uint8_t command, const uint8_t *args, size_t arg_bytes,
                                                    uint8_t *data, size_t data_bytes) {
  uint8_t tx_data[arg_bytes + 1];
  tx_data[0] = command;
  if (arg_bytes > 0) {
    assert(args != nullptr);
    memcpy(tx_data + 1, args, arg_bytes);
  }

  this->si446x_raw_command_(tx_data, arg_bytes + 1, data, data_bytes);
}

void TemperBridgeComponent::si446x_configuration_init_(const uint8_t *data) {
  while (*data != 0) {
    const size_t size_bytes = *data++;
    assert(size_bytes <= 16);

    uint8_t command[size_bytes];
    memcpy(command, data, size_bytes);
    ESP_LOGI(TAG, "Processing command %x with # bytes: %d", command[0], size_bytes);
    data += size_bytes;
    si446x_raw_command_(command, size_bytes, nullptr, 0);
  }
}

void TemperBridgeComponent::si446x_get_int_status(Si446xGetIntStatusResp *ret, bool clear_pending) {
  static_assert(sizeof(Si446xGetIntStatusResp) == 8, "wrong size");
  if (clear_pending) {
    si446x_execute_command_(SI446X_CMD_GET_INT_STATUS, nullptr, 0, (uint8_t *) ret, sizeof(Si446xGetIntStatusResp));
  } else {
    const uint8_t args[] = {
        static_cast<uint8_t>(0xFF),
        static_cast<uint8_t>(0xFF),
        static_cast<uint8_t>(0x7F),
    };

    si446x_execute_command_(SI446X_CMD_GET_INT_STATUS, args, sizeof(args), (uint8_t *) ret,
                            sizeof(Si446xGetIntStatusResp));
  }
}

void TemperBridgeComponent::read_irq_pend_frr() {
  this->enable();
  this->write_byte(SI446X_CMD_FRR_A_READ);
  const uint8_t a = this->read_byte();
  const uint8_t b = this->read_byte();
  const uint8_t c = this->read_byte();
  const uint8_t d = this->read_byte();
  ESP_LOGI(TAG, "a: %x, b: %x, c: %x, d: %x", a, b, c, d);

  this->disable();
}

enum class TemperCommand : uint32_t {
  HEAD_UP = 0X96530005,
  HEAD_DOWN = 0X96540005,
  FLAT = 0X965C0400,
  LEG_UP = 0X96510100,
  LEG_DOWN = 0X96520100,
  MEM_1 = 0X965C0000,
  MEM_2 = 0X965C0100,
  MEM_3 = 0X965C0200,
  MEM_4 = 0X965C0300,
  SET_MEM_1 = 0x965B0000,
  SET_MEM_2 = 0x965B0100,
  SET_MEM_3 = 0x965B0200,
  SET_MEM_4 = 0x965B0300,
  STOP = 0X96860000,
  MASSAGE_MODE_1 = 0X968D0078,
  MASSAGE_MODE_2 = 0X968D0178,
  MASSAGE_MODE_3 = 0X968D0278,
  MASSAGE_MODE_4 = 0X968D0378
};

#define TEMPER_CMD_BROADCAST_CH 0x96000000

// CRC table generated from http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
// The parameters were reversed using http://reveng.sourceforge.net/
// Poly: 0x8D, initial: 0xFF
const uint8_t TEMPER_CRC_TABLE[] = {
    0x00, 0x8D, 0x97, 0x1A, 0xA3, 0x2E, 0x34, 0xB9, 0xCB, 0x46, 0x5C, 0xD1, 0x68, 0xE5, 0xFF, 0x72, 0x1B, 0x96, 0x8C,
    0x01, 0xB8, 0x35, 0x2F, 0xA2, 0xD0, 0x5D, 0x47, 0xCA, 0x73, 0xFE, 0xE4, 0x69, 0x36, 0xBB, 0xA1, 0x2C, 0x95, 0x18,
    0x02, 0x8F, 0xFD, 0x70, 0x6A, 0xE7, 0x5E, 0xD3, 0xC9, 0x44, 0x2D, 0xA0, 0xBA, 0x37, 0x8E, 0x03, 0x19, 0x94, 0xE6,
    0x6B, 0x71, 0xFC, 0x45, 0xC8, 0xD2, 0x5F, 0x6C, 0xE1, 0xFB, 0x76, 0xCF, 0x42, 0x58, 0xD5, 0xA7, 0x2A, 0x30, 0xBD,
    0x04, 0x89, 0x93, 0x1E, 0x77, 0xFA, 0xE0, 0x6D, 0xD4, 0x59, 0x43, 0xCE, 0xBC, 0x31, 0x2B, 0xA6, 0x1F, 0x92, 0x88,
    0x05, 0x5A, 0xD7, 0xCD, 0x40, 0xF9, 0x74, 0x6E, 0xE3, 0x91, 0x1C, 0x06, 0x8B, 0x32, 0xBF, 0xA5, 0x28, 0x41, 0xCC,
    0xD6, 0x5B, 0xE2, 0x6F, 0x75, 0xF8, 0x8A, 0x07, 0x1D, 0x90, 0x29, 0xA4, 0xBE, 0x33, 0xD8, 0x55, 0x4F, 0xC2, 0x7B,
    0xF6, 0xEC, 0x61, 0x13, 0x9E, 0x84, 0x09, 0xB0, 0x3D, 0x27, 0xAA, 0xC3, 0x4E, 0x54, 0xD9, 0x60, 0xED, 0xF7, 0x7A,
    0x08, 0x85, 0x9F, 0x12, 0xAB, 0x26, 0x3C, 0xB1, 0xEE, 0x63, 0x79, 0xF4, 0x4D, 0xC0, 0xDA, 0x57, 0x25, 0xA8, 0xB2,
    0x3F, 0x86, 0x0B, 0x11, 0x9C, 0xF5, 0x78, 0x62, 0xEF, 0x56, 0xDB, 0xC1, 0x4C, 0x3E, 0xB3, 0xA9, 0x24, 0x9D, 0x10,
    0x0A, 0x87, 0xB4, 0x39, 0x23, 0xAE, 0x17, 0x9A, 0x80, 0x0D, 0x7F, 0xF2, 0xE8, 0x65, 0xDC, 0x51, 0x4B, 0xC6, 0xAF,
    0x22, 0x38, 0xB5, 0x0C, 0x81, 0x9B, 0x16, 0x64, 0xE9, 0xF3, 0x7E, 0xC7, 0x4A, 0x50, 0xDD, 0x82, 0x0F, 0x15, 0x98,
    0x21, 0xAC, 0xB6, 0x3B, 0x49, 0xC4, 0xDE, 0x53, 0xEA, 0x67, 0x7D, 0xF0, 0x99, 0x14, 0x0E, 0x83, 0x3A, 0xB7, 0xAD,
    0x20, 0x52, 0xDF, 0xC5, 0x48, 0xF1, 0x7C, 0x66, 0xEB,
};

uint8_t temper_crc(const uint8_t data[], size_t len) {
  // Compute CRC using table lookup method
  uint8_t ret = 0xFF;
  for (size_t i = 0; i < len; i++) {
    const uint8_t byte = data[i] ^ ret;
    ret = TEMPER_CRC_TABLE[byte];
  }

  return ret;
}

void TemperBridgeComponent::start_positioning(PositionCommand cmd) {
  TemperCommand command;
  switch (cmd) {
    case PositionCommand::LOWER_HEAD:
      command = TemperCommand::HEAD_DOWN;
      break;
    case PositionCommand::LOWER_LEGS:
      command = TemperCommand::LEG_DOWN;
      break;
    case PositionCommand::RAISE_HEAD:
      command = TemperCommand::HEAD_UP;
      break;
    case PositionCommand::RAISE_LEGS:
      command = TemperCommand::LEG_UP;
      break;
  }

  for (int i = 0; i < 5; i++) {
    this->transmit_command_(static_cast<uint32_t>(command));
  }
}

void TemperBridgeComponent::execute_simple_command(SimpleCommand cmd) {
  TemperCommand command;
  switch (cmd) {
    case SimpleCommand::PRESET_FLAT:
      command = TemperCommand::FLAT;
      break;
    case SimpleCommand::PRESET_MODE1:
      command = TemperCommand::MEM_1;
      break;
    case SimpleCommand::PRESET_MODE2:
      command = TemperCommand::MEM_2;
      break;
    case SimpleCommand::PRESET_MODE3:
      command = TemperCommand::MEM_3;
      break;
    case SimpleCommand::PRESET_MODE4:
      command = TemperCommand::MEM_4;
      break;
    case SimpleCommand::SAVE_PRESET_MODE1:
      command = TemperCommand::SET_MEM_1;
      break;
    case SimpleCommand::SAVE_PRESET_MODE2:
      command = TemperCommand::SET_MEM_2;
      break;
    case SimpleCommand::SAVE_PRESET_MODE3:
      command = TemperCommand::SET_MEM_3;
      break;
    case SimpleCommand::SAVE_PRESET_MODE4:
      command = TemperCommand::SET_MEM_4;
      break;
    case SimpleCommand::STOP:
      command = TemperCommand::STOP;
      this->massage_head_intensity_ = 0;
      this->massage_leg_intensity_ = 0;
      this->massage_lumbar_intensity_ = 0;
      this->massage_command_mode_ = MassageCommandMode::CUSTOM;
      break;
    case SimpleCommand::MASSAGE_PRESET_MODE1:
      command = TemperCommand::MASSAGE_MODE_1;
      break;
    case SimpleCommand::MASSAGE_PRESET_MODE2:
      command = TemperCommand::MASSAGE_MODE_2;
      break;
    case SimpleCommand::MASSAGE_PRESET_MODE3:
      command = TemperCommand::MASSAGE_MODE_3;
      break;
    case SimpleCommand::MASSAGE_PRESET_MODE4:
      command = TemperCommand::MASSAGE_MODE_4;
      break;
  }

  if (cmd == SimpleCommand::MASSAGE_PRESET_MODE1 || cmd == SimpleCommand::MASSAGE_PRESET_MODE2 ||
      cmd == SimpleCommand::MASSAGE_PRESET_MODE3 || cmd == SimpleCommand::MASSAGE_PRESET_MODE4) {
    this->massage_head_intensity_ = 5;
    this->massage_leg_intensity_ = 5;
    this->massage_lumbar_intensity_ = 5;
    this->massage_command_mode_ = MassageCommandMode::BUILTIN;
  }

  for (int i = 0; i < 3; i++) {
    this->transmit_command_(static_cast<uint32_t>(command));
  }
}

void TemperBridgeComponent::transmit_command_(uint32_t command) {
  const uint32_t tx_start = micros();

  // This command doesn't need to wait for CTS
  uint8_t packet_bytes[9] = {0};
  static_assert(sizeof(TemperPacket) == 7, "wrong size");
  packet_bytes[0] = SI446X_CMD_WRITE_TX_FIFO;
  packet_bytes[1] = sizeof(TemperPacket);

  TemperPacket packet = {.cmd = convert_big_endian(command), .channel = convert_big_endian(this->channel_)};
  packet.crc = temper_crc((uint8_t *) &packet, 6);
  memcpy(packet_bytes + 2, &packet, sizeof(TemperPacket));

  this->enable();
  this->write_array(packet_bytes, sizeof(packet_bytes));
  this->disable();

  this->si446x_start_tx_();

  const uint32_t start_time = millis();
  while (this->interrupt_pin_->digital_read()) {
    if (millis() - start_time > 50) {
      break;
    }
  }

  Si446xGetIntStatusResp int_status;
  si446x_get_int_status(&int_status, true);
  //int_status.print();

  uint32_t tx_diff = micros() - tx_start;
  ESP_LOGI(TAG, "took %u us to TX one packet", tx_diff);

  while (tx_diff < 100000) {
    delay(10);
    tx_diff = micros() - tx_start;
  }

  ESP_LOGI(TAG, "after delay: took %u us to TX one packet", tx_diff);
}

void TemperBridgeComponent::loop() {
  if (!this->interrupt_pin_->digital_read()) {
    Si446xGetIntStatusResp int_status;
    si446x_get_int_status(&int_status, true);
    int_status.print();
  }
}

void TemperBridgeComponent::set_channel(uint16_t channel) {
  this->channel_ = channel;
  if (this->initialized_) {
    this->tune_channel_(channel);
  }
}

void TemperBridgeComponent::si446x_fifo_info_(Si446xFifoInfoResp *ret, bool clear_rx, bool clear_tx) {
  static_assert(sizeof(Si446xFifoInfoResp) == 2, "");
  uint8_t const arg = (clear_tx ? (1 << 0) : 0) | (clear_rx ? (1 << 1) : 0);
  si446x_execute_command_(SI446X_CMD_FIFO_INFO, &arg, 1, (uint8_t *) ret, sizeof(Si446xFifoInfoResp));
}

void TemperBridgeComponent::si446x_start_tx_() {
  uint8_t tx_args[] = {
      0x0,  // channel
      0,    // condition
  };
  si446x_execute_command_(SI446X_CMD_START_TX, tx_args, sizeof(tx_args), nullptr, 0);
}

inline float temper_frequency_for_channel(uint16_t channel) {
  // compute fc (from original Si4432 implementation)
  const uint16_t fc = channel > 8862 ? ((2 * channel) + 10658) : channel + 19520;
  return 10.0f * (19 + 24 + (fc / 64000.0f));
}

// TODO these methods should go in the si446x file
void temper_calculate_freq_control(uint16_t channel, uint8_t *freq_control_inte, uint32_t *freq_control_frac) {
  assert(channel >= 1);
  assert(channel <= 10111);
  assert(freq_control_inte != nullptr);
  assert(freq_control_frac != nullptr);

  // TODO compiler warnings
  // oscillator frequency in Hz
  const float freq_xo = 0x01C9C380;

  const float freq_mhz = temper_frequency_for_channel(channel);
  const float freq_hz = pow10f(6) * freq_mhz;

  const float N = freq_hz / ((2 * freq_xo) / 8);

  float integ;
  float frac = modff(N, &integ);

  integ--;
  frac++;
  assert(frac >= 1 && frac <= 2);

  *freq_control_frac = frac * powf(2, 19);
  *freq_control_inte = integ;
}

void TemperBridgeComponent::si446x_set_freq_control_properties_(uint8_t freq_control_inte, uint32_t freq_control_frac) {
  Si446xSetPropertyArgs args = {
      .group = 0x40,  // TODO don't hardcode
      .num_props = 4,
      .start_prop = 0x00  // TODO don't hardcode
  };

  uint8_t data[] = {freq_control_inte, static_cast<uint8_t>((freq_control_frac & 0xFFFF00) >> 16),
                    static_cast<uint8_t>((freq_control_frac & 0xFF00) >> 8),
                    static_cast<uint8_t>(freq_control_frac & 0xFF)};
  si446x_set_property_(&args, data);
}

void TemperBridgeComponent::si446x_set_property_(Si446xSetPropertyArgs *args, uint8_t *data) {
  static_assert(sizeof(Si446xSetPropertyArgs) == 3, "wrong size");
  uint8_t cts;
  uint8_t full_args[sizeof(Si446xSetPropertyArgs) + args->num_props];
  memcpy(full_args, (uint8_t *) args, sizeof(Si446xSetPropertyArgs));
  memcpy(full_args + sizeof(Si446xSetPropertyArgs), data, args->num_props);

  si446x_execute_command_(SI446X_CMD_SET_PROPERTY, full_args, sizeof(full_args), &cts, 1);
}

void TemperBridgeComponent::si446x_get_property_(Si446xGetPropertyArgs *args, uint8_t *props) {
  static_assert(sizeof(Si446xGetPropertyArgs) == 3, "wrong size");
  uint8_t resp[args->num_props];
  si446x_execute_command_(SI446X_CMD_GET_PROPERTY, (uint8_t *) args, sizeof(Si446xGetPropertyArgs), (uint8_t *) resp,
                          args->num_props);
  memcpy(props, resp, args->num_props);
}

void TemperBridgeComponent::si446x_get_freq_control_properties_(uint8_t *freq_control_inte,
                                                                uint32_t *freq_control_frac) {
  Si446xGetPropertyArgs args = {
      .group = 0x40,  // TODO don't hardcode
      .num_props = 4,
      .start_prop = 0x00  // TODO don't hardcode
  };

  uint8_t freq_props[4] = {0};
  si446x_get_property_(&args, freq_props);

  *freq_control_inte = freq_props[0];
  *freq_control_frac = freq_props[3] | (freq_props[2] << 8) | (freq_props[1] << 16);
}

void TemperBridgeComponent::tune_channel_(uint16_t channel) {
  uint8_t calc_inte;
  uint32_t calc_frac;
  temper_calculate_freq_control(channel, &calc_inte, &calc_frac);

  ESP_LOGI(TAG, "calculated frac: %08" PRIx32, calc_frac);
  ESP_LOGI(TAG, "calculated inte: %x", calc_inte);

  si446x_set_freq_control_properties_(calc_inte, calc_frac);

  delay(50);

  uint32_t read_frac;
  uint8_t read_inte;
  si446x_get_freq_control_properties_(&read_inte, &read_frac);
  ESP_LOGI(TAG, "read frac: %08" PRIx32, read_frac);
  ESP_LOGI(TAG, "read inte: %x", read_inte);
}

// This one seems to be used when making adjustments to built-in modes
#define TEMPER_MASSAGE_MAGIC_1 0x968E0000
#define TEMPER_MASSAGE_MAGIC_2 0x96850000

#define TEMPER_MASSAGE_TYPE_HEAD 0X00000000
#define TEMPER_MASSAGE_TYPE_LUMBAR 0X00000100
#define TEMPER_MASSAGE_TYPE_LEG 0X00000200

#define TEMPER_MASSAGE_LEVEL_STEP 0x18

void TemperBridgeComponent::set_massage_level(MassageTarget target, uint8_t level) {
  uint32_t command =
      this->massage_command_mode_ == MassageCommandMode::BUILTIN ? TEMPER_MASSAGE_MAGIC_1 : TEMPER_MASSAGE_MAGIC_2;

  switch (target) {
    case MassageTarget::HEAD:
      if (this->massage_head_intensity_ == level) {
        return;
      }
      this->massage_head_intensity_ = level;
      command |= TEMPER_MASSAGE_TYPE_HEAD;
      break;
    case MassageTarget::LEGS:
      if (this->massage_leg_intensity_ == level) {
        return;
      }
      this->massage_leg_intensity_ = level;
      command |= TEMPER_MASSAGE_TYPE_LEG;
      break;
    case MassageTarget::LUMBAR:
      if (this->massage_lumbar_intensity_ == level) {
        return;
      }
      this->massage_lumbar_intensity_ = level;
      command |= TEMPER_MASSAGE_TYPE_LUMBAR;
      break;
  }

  command |= TEMPER_MASSAGE_LEVEL_STEP * level;

  // TODO: De-dupe
  for (int i = 0; i < 3; i++) {
    this->transmit_command_(static_cast<uint32_t>(command));
  }
}

}  // namespace temperbridge
}  // namespace esphome