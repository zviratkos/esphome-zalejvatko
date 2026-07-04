#include "zalejvatko.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace zalejvatko {

static const char *const TAG = "zalejvatko";

void ZalejvatkoComponent::setup() {
  // adresni piny muxu (BCD adresace 0-15)
  for (uint8_t i = 0; i < 4; i++) {
    if (this->address_pins_[i] != nullptr) {
      this->address_pins_[i]->setup();
      this->address_pins_[i]->digital_write(false);
    }
  }
  if (this->valve_enable_pin_ != nullptr) {
    this->valve_enable_pin_->setup();
    this->valve_enable_pin_->digital_write(false);  // aktivni v LOW, viz puvodni firmware
  }
  if (this->valve_signal_pin_ != nullptr) {
    this->valve_signal_pin_->setup();
    this->valve_signal_pin_->digital_write(false);
  }
  if (this->pump_pin_ != nullptr) {
    this->pump_pin_->setup();
    this->pump_pin_->digital_write(false);
  }

  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++)
    this->load_channel_(ch);

  ESP_LOGCONFIG(TAG, "Zalejvatko hub inicializovan, %u kanalu", MAX_CHANNELS);
}

void ZalejvatkoComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Zalejvatko:");
  ESP_LOGCONFIG(TAG, "  ml/s kalibrace cerpadla: %.2f", this->ml_per_sec_);
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    const auto &c = this->channels_[ch];
    if (c.enabled || c.schedule[0] != 0) {
      ESP_LOGCONFIG(TAG, "  kanal %u: enabled=%s dose=%.0fml schedule=%s", ch, YESNO(c.enabled), c.dose_ml,
                    c.schedule);
    }
  }
}

// ---------------------------------------------------------------------
// perzistence
// ---------------------------------------------------------------------
void ZalejvatkoComponent::load_channel_(uint8_t channel) {
  // unikatni "hash" pro kazdy kanal, aby se neprepisovaly navzajem ve flash
  uint32_t hash = fnv1_hash("zalejvatko_channel_" + std::to_string(channel));
  this->channel_prefs_[channel] = global_preferences->make_preference<ChannelConfig>(hash);
  ChannelConfig loaded;
  if (this->channel_prefs_[channel].load(&loaded)) {
    this->channels_[channel] = loaded;
  } else {
    // vychozi hodnoty, kdyz jeste nic ulozeno neni
    this->channels_[channel] = ChannelConfig{};
    this->save_channel_(channel);
  }
}

void ZalejvatkoComponent::save_channel_(uint8_t channel) {
  this->channel_prefs_[channel].save(&this->channels_[channel]);
}

// ---------------------------------------------------------------------
// registrace entit
// ---------------------------------------------------------------------
void ZalejvatkoComponent::register_enable_switch(uint8_t channel, ZalejvatkoChannelSwitch *sw) {
  this->enable_switches_[channel] = sw;
}
void ZalejvatkoComponent::register_dose_number(uint8_t channel, ZalejvatkoChannelDoseNumber *num) {
  this->dose_numbers_[channel] = num;
}
void ZalejvatkoComponent::register_schedule_text(uint8_t channel, ZalejvatkoChannelScheduleText *txt) {
  this->schedule_texts_[channel] = txt;
}
void ZalejvatkoComponent::register_water_now_button(uint8_t channel, ZalejvatkoChannelWaterNowButton *btn) {
  this->water_now_buttons_[channel] = btn;
}

// ---------------------------------------------------------------------
// zapis z entit -> hub
// ---------------------------------------------------------------------
void ZalejvatkoComponent::set_channel_enabled(uint8_t channel, bool enabled) {
  this->channels_[channel].enabled = enabled;
  this->save_channel_(channel);
}

void ZalejvatkoComponent::set_channel_dose(uint8_t channel, float dose_ml) {
  this->channels_[channel].dose_ml = dose_ml;
  this->save_channel_(channel);
}

void ZalejvatkoComponent::set_channel_schedule(uint8_t channel, const std::string &schedule_csv) {
  strncpy(this->channels_[channel].schedule, schedule_csv.c_str(), MAX_SCHEDULE_STRLEN - 1);
  this->channels_[channel].schedule[MAX_SCHEDULE_STRLEN - 1] = 0;
  this->save_channel_(channel);
}

void ZalejvatkoComponent::water_now(uint8_t channel) {
  if (!this->channels_[channel].enabled) {
    ESP_LOGW(TAG, "Kanal %u je zakazany, rucni zalevani ignorovano", channel);
    return;
  }
  ESP_LOGI(TAG, "Rucni pozadavek na zalevani, kanal %u", channel);
  this->queue_.push_back({channel, this->channels_[channel].dose_ml});
}

// ---------------------------------------------------------------------
// rozvrh - parsovani "08:00,14:00,18:00"
// ---------------------------------------------------------------------
void ZalejvatkoComponent::parse_schedule_(const char *csv, ScheduleSlot slots[MAX_SCHEDULE_SLOTS]) {
  for (uint8_t i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
    slots[i].hour = -1;
    slots[i].minute = -1;
  }
  if (csv == nullptr || csv[0] == 0)
    return;

  char buf[MAX_SCHEDULE_STRLEN];
  strncpy(buf, csv, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;

  uint8_t slot_idx = 0;
  char *token = strtok(buf, ",");
  while (token != nullptr && slot_idx < MAX_SCHEDULE_SLOTS) {
    int hour = -1, minute = -1;
    if (sscanf(token, "%d:%d", &hour, &minute) == 2 && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      slots[slot_idx].hour = hour;
      slots[slot_idx].minute = minute;
      slot_idx++;
    } else {
      ESP_LOGW(TAG, "Neplatny format casu v rozvrhu: '%s'", token);
    }
    token = strtok(nullptr, ",");
  }
}

void ZalejvatkoComponent::check_schedule_() {
  if (this->time_ == nullptr)
    return;
  auto now = this->time_->now();
  if (!now.is_valid())
    return;  // cas jeste nebyl synchronizovan (NTP nedostupne)

  time_t midnight = now.timestamp - (now.hour * 3600 + now.minute * 60 + now.second);

  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    auto &cfg = this->channels_[ch];
    if (!cfg.enabled)
      continue;

    ScheduleSlot slots[MAX_SCHEDULE_SLOTS];
    this->parse_schedule_(cfg.schedule, slots);

    for (uint8_t s = 0; s < MAX_SCHEDULE_SLOTS; s++) {
      if (slots[s].hour < 0)
        continue;
      time_t slot_epoch = midnight + slots[s].hour * 3600 + slots[s].minute * 60;
      // spusti, pokud jsme uz za planovanym casem a od te doby jsme se jeste nezalevali
      if (now.timestamp >= slot_epoch && cfg.last_watered_epoch < (uint32_t) slot_epoch) {
        ESP_LOGI(TAG, "Naplanovane zalevani, kanal %u, cas %02d:%02d", ch, slots[s].hour, slots[s].minute);
        this->queue_.push_back({ch, cfg.dose_ml});
        // last_watered_epoch se aktualizuje az pri realnem spusteni (start_watering_),
        // tohle jen brani opakovanemu zarazeni do fronty ve stejnem loopu
        cfg.last_watered_epoch = now.timestamp;
      }
    }
  }
}

// ---------------------------------------------------------------------
// hardware - mux / cerpadlo
// ---------------------------------------------------------------------
void ZalejvatkoComponent::address_mux_(uint8_t channel) {
  channel &= 0xF;
  for (uint8_t bit = 0; bit < 4; bit++) {
    if (this->address_pins_[bit] != nullptr)
      this->address_pins_[bit]->digital_write((channel >> bit) & 0x1);
  }
}

void ZalejvatkoComponent::set_valve_(bool on) {
  if (this->valve_signal_pin_ != nullptr)
    this->valve_signal_pin_->digital_write(on);
}

void ZalejvatkoComponent::start_watering_(uint8_t channel, float dose_ml) {
  float duration_s = dose_ml / this->ml_per_sec_;
  this->watering_in_progress_ = true;
  this->watering_channel_ = channel;
  this->watering_end_ms_ = millis() + (uint32_t) (duration_s * 1000.0f);

  this->channels_[channel].last_watered_epoch =
      (this->time_ != nullptr && this->time_->now().is_valid()) ? this->time_->now().timestamp : 0;
  this->save_channel_(channel);

  ESP_LOGI(TAG, "Zalevam kanal %u, %.0f ml (%.1f s)", channel, dose_ml, duration_s);
  this->address_mux_(channel);
  this->set_valve_(true);
  if (this->pump_pin_ != nullptr)
    this->pump_pin_->digital_write(true);
}

void ZalejvatkoComponent::stop_watering_() {
  ESP_LOGI(TAG, "Konec zalevani, kanal %u", this->watering_channel_);
  this->set_valve_(false);
  if (this->pump_pin_ != nullptr)
    this->pump_pin_->digital_write(false);
  this->watering_in_progress_ = false;
}

void ZalejvatkoComponent::process_queue_() {
  if (this->watering_in_progress_ || this->queue_.empty())
    return;
  WateringCommand cmd = this->queue_.front();
  this->queue_.pop_front();
  this->start_watering_(cmd.channel, cmd.dose_ml);
}

void ZalejvatkoComponent::check_running_watering_() {
  if (!this->watering_in_progress_)
    return;
  if ((int32_t) (millis() - this->watering_end_ms_) >= 0)
    this->stop_watering_();
}

void ZalejvatkoComponent::update() {
  this->check_schedule_();
  this->process_queue_();
  this->check_running_watering_();
}

}  // namespace zalejvatko
}  // namespace esphome
