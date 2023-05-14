#include "si446x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace temperbridge {

static const char *const TAG = "temperbridge";

void Si446xGetIntStatusResp::print() {
  if (this->int_pend != 0) {
    ESP_LOGI(TAG, "interrupt pend:");
    if (this->int_pend & (1 << 2))
      ESP_LOGI(TAG, "\tCHIP_INT_PEND");
    if (this->int_pend & (1 << 1))
      ESP_LOGI(TAG, "\tMODEM_INT_PEND");
    if (this->int_pend & (1 << 0))
      ESP_LOGI(TAG, "\tPH_INT_PEND");
  }

  if (this->ph_pend != 0) {
    ESP_LOGI(TAG, "packet pend:");
    if (this->ph_pend & (1 << 7))
      ESP_LOGI(TAG, "\tFILTER_MATCH_PEND");
    if (this->ph_pend & (1 << 6))
      ESP_LOGI(TAG, "\tFILTER_MISS_PEND");
    if (this->ph_pend & (1 << 5))
      ESP_LOGI(TAG, "\tPACKET_SENT_PEND");
    if (this->ph_pend & (1 << 4))
      ESP_LOGI(TAG, "\tPACKET_RX_PEND");
    if (this->ph_pend & (1 << 3))
      ESP_LOGI(TAG, "\tCRC_ERROR_PEND");
    if (this->ph_pend & (1 << 2))
      ESP_LOGI(TAG, "\tALT_CRC_ERROR_PEND");
    if (this->ph_pend & (1 << 1))
      ESP_LOGI(TAG, "\tTX_FIFO_ALMOST_EMPTY_PEND");
    if (this->ph_pend & (1 << 0))
      ESP_LOGI(TAG, "\tRX_FIFO_ALMOST_FULL_PEND");
  }

  if (this->modem_pend != 0) {
    ESP_LOGI(TAG, "modem pend:");
    if (this->modem_pend & (1 << 7))
      ESP_LOGI(TAG, "\tRSSI_LATCH_PEND");
    if (this->modem_pend & (1 << 6))
      ESP_LOGI(TAG, "\tPOSTAMBLE_DETECT_PEND");
    if (this->modem_pend & (1 << 5))
      ESP_LOGI(TAG, "\tINVALID_SYNC_PEND");
    if (this->modem_pend & (1 << 4))
      ESP_LOGI(TAG, "\tRSSI_JUMP_PEND");
    if (this->modem_pend & (1 << 3))
      ESP_LOGI(TAG, "\tRSSI_PEND");
    if (this->modem_pend & (1 << 2))
      ESP_LOGI(TAG, "\tINVALID_PREAMBLE_PEND");
    if (this->modem_pend & (1 << 1))
      ESP_LOGI(TAG, "\tPREAMBLE_DETECT_PEND");
    if (this->modem_pend & (1 << 0))
      ESP_LOGI(TAG, "\tSYNC_DETECT_PEND");
  }

  if (this->chip_pend != 0) {
    ESP_LOGI(TAG, "chip pend:");
    if (this->chip_pend & (1 << 6))
      ESP_LOGI(TAG, "\tCAL_PEND");
    if (this->chip_pend & (1 << 5))
      ESP_LOGI(TAG, "\tFIFO_UNDERFLOW_OVERFLOW_ERROR_PEND");
    if (this->chip_pend & (1 << 4))
      ESP_LOGI(TAG, "\tSTATE_CHANGE_PEND");
    if (this->chip_pend & (1 << 3))
      ESP_LOGI(TAG, "\t>>> CMD_ERROR_PEND <<<");
    if (this->chip_pend & (1 << 2))
      ESP_LOGI(TAG, "\tCHIP_READY_PEND");
    if (this->chip_pend & (1 << 1))
      ESP_LOGI(TAG, "\tLOW_BATT_PEND");
    if (this->chip_pend & (1 << 0))
      ESP_LOGI(TAG, "\tWUT_PEND");
  }
}

}  // namespace temperbridge
}  // namespace esphome