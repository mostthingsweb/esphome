#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

#ifndef ESPHOME_TEMPERBRIDGE_H
#define ESPHOME_TEMPERBRIDGE_H

namespace esphome {
namespace temperbridge {

enum class SimpleCommand {
  PRESET_FLAT,
  PRESET_MODE1,
  PRESET_MODE2,
  PRESET_MODE3,
  PRESET_MODE4,
  SAVE_PRESET_MODE1,
  SAVE_PRESET_MODE2,
  SAVE_PRESET_MODE3,
  SAVE_PRESET_MODE4,
  STOP,
  MASSAGE_PRESET_MODE1,
  MASSAGE_PRESET_MODE2,
  MASSAGE_PRESET_MODE3,
  MASSAGE_PRESET_MODE4,
};

enum class PositionCommand {
  RAISE_HEAD,
  RAISE_LEGS,
  LOWER_HEAD,
  LOWER_LEGS,
};

enum class MassageTarget {
  HEAD,
  LEGS,
  LUMBAR,
};

enum class MassageCommandMode {
  BUILTIN,
  CUSTOM,
};

struct TemperPacket {
  uint32_t cmd;
  uint16_t channel;
  uint8_t crc;
} PACKED;

class TemperBridgeComponent : public Component,
                              public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                    spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void loop() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

  void set_sdn_pin(GPIOPin *pin) { this->sdn_pin_ = pin; }

  void execute_simple_command(SimpleCommand cmd);

  void start_positioning(PositionCommand cmd);

  void set_channel(uint16_t channel);

  void set_massage_level(MassageTarget target, uint8_t level);

  void si446x_get_int_status(Si446xGetIntStatusResp *ret, bool clear_pending);

 protected:
  void si446x_raw_command_(const uint8_t *tx_data, size_t tx_data_bytes, uint8_t *resp, size_t resp_bytes);
  void si446x_execute_command_(uint8_t command, const uint8_t *args, size_t arg_bytes, uint8_t *data, size_t data_bytes);
  Si446xChipInfoResp si446x_part_info_();
  void si446x_configuration_init_(const uint8_t *data);
  void si446x_fifo_info_(Si446xFifoInfoResp *ret, bool clear_rx, bool clear_tx);
  void si446x_start_tx_();
  void si446x_set_freq_control_properties_(uint8_t freq_control_inte, uint32_t freq_control_frac);
  void si446x_set_property_(Si446xSetPropertyArgs *args, uint8_t *data);
  void si446x_get_freq_control_properties_(uint8_t *freq_control_inte, uint32_t *freq_control_frac);
  void si446x_get_property_(Si446xGetPropertyArgs *args, uint8_t *props);

  void transmit_command_(uint32_t command);

  void read_irq_pend_frr();

  void tune_channel_(uint16_t channel);

  bool initialized_ = false;

  uint16_t channel_;
  InternalGPIOPin *interrupt_pin_;
  GPIOPin *sdn_pin_;

  uint8_t massage_leg_intensity_ = 0;
  uint8_t massage_head_intensity_ = 0;
  uint8_t massage_lumbar_intensity_ = 0;
  MassageCommandMode massage_command_mode_ = MassageCommandMode::CUSTOM;
};

template<typename... Ts> class ExecuteSimpleCommandAction : public Action<Ts...>, public Parented<TemperBridgeComponent> {
 public:
  TEMPLATABLE_VALUE(SimpleCommand, cmd);

  void play(Ts... x) override { this->parent_->execute_simple_command(this->cmd_.value(x...)); }
};

template<typename... Ts> class PositionCommandAction : public Action<Ts...>, public Parented<TemperBridgeComponent> {
 public:
  TEMPLATABLE_VALUE(PositionCommand, cmd);

  void play(Ts... x) override { this->parent_->start_positioning(this->cmd_.value(x...)); }
};

template<typename... Ts> class SetChannelAction : public Action<Ts...>, public Parented<TemperBridgeComponent> {
 public:
  TEMPLATABLE_VALUE(uint16_t, channel)

  void play(Ts... x) override {
    auto channel = this->channel_.value(x...);
    ESP_LOGI("temperbridge", "channel: %d", channel);
    this->parent_->set_channel(channel);
  }
};

template<typename... Ts> class SetMassageIntensityAction : public Action<Ts...>, public Parented<TemperBridgeComponent> {
 public:
  TEMPLATABLE_VALUE(MassageTarget, target)
  TEMPLATABLE_VALUE(uint8_t, level)

  void play(Ts... x) override {
    auto target = this->target_.value(x...);
    auto level = this->level_.value(x...);
    this->parent_->set_massage_level(target, level);
  }
};

}  // namespace temperbridge
}  // namespace esphome

#endif  // ESPHOME_TEMPERBRIDGE_H
