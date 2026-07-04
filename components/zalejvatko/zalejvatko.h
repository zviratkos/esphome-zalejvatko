#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text/text.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <vector>
#include <string>
#include <deque>

namespace esphome {
namespace zalejvatko {

static const uint8_t MAX_CHANNELS = 16;
static const uint8_t MAX_SCHEDULE_SLOTS = 6;  // max poctu casu za den na kanal
static const uint8_t MAX_SCHEDULE_STRLEN = 48;  // "08:00,14:00,18:00,..." vejde se

// jeden naplanovany cas v ramci dne
struct ScheduleSlot {
  int8_t hour = -1;    // -1 = nepouzity slot
  int8_t minute = -1;
};

// perzistentni konfigurace jednoho kanalu (uklada se do flash)
struct ChannelConfig {
  bool enabled{false};
  float dose_ml{20.0f};
  char schedule[MAX_SCHEDULE_STRLEN]{0};   // napr. "08:00,14:00,18:00"
  uint32_t last_watered_epoch{0};          // unix timestamp posledniho zalevani
};

// prikaz ve fronte na zalevani (bud naplanovany, nebo rucni)
struct WateringCommand {
  uint8_t channel;
  float dose_ml;
};

class ZalejvatkoChannelSwitch;
class ZalejvatkoChannelDoseNumber;
class ZalejvatkoChannelScheduleText;
class ZalejvatkoChannelWaterNowButton;
class ZalejvatkoChannelLastWateredTextSensor;

class ZalejvatkoComponent : public PollingComponent {
 public:
  ZalejvatkoComponent() : PollingComponent(1000) {}

  // ---- konfigurace z YAML (volano z __init__.py) ----
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
  void set_valve_enable_pin(GPIOPin *pin) { this->valve_enable_pin_ = pin; }
  void set_valve_signal_pin(GPIOPin *pin) { this->valve_signal_pin_ = pin; }
  void set_address_pin(uint8_t index, GPIOPin *pin) { this->address_pins_[index] = pin; }
  void set_pump_pin(GPIOPin *pin) { this->pump_pin_ = pin; }
  void set_ml_per_sec(float ml_per_sec) { this->ml_per_sec_ = ml_per_sec; }

  // ---- registrace entit jednotlivych kanalu ----
  void register_enable_switch(uint8_t channel, ZalejvatkoChannelSwitch *sw);
  void register_dose_number(uint8_t channel, ZalejvatkoChannelDoseNumber *num);
  void register_schedule_text(uint8_t channel, ZalejvatkoChannelScheduleText *txt);
  void register_water_now_button(uint8_t channel, ZalejvatkoChannelWaterNowButton *btn);
  void register_last_watered_sensor(uint8_t channel, ZalejvatkoChannelLastWateredTextSensor *sens);

  // ---- volano z entit pri zmene stavu z HA / UI ----
  void set_channel_enabled(uint8_t channel, bool enabled);
  void set_channel_dose(uint8_t channel, float dose_ml);
  void set_channel_schedule(uint8_t channel, const std::string &schedule_csv);
  void water_now(uint8_t channel);

  const ChannelConfig &get_channel_config(uint8_t channel) const { return this->channels_[channel]; }

  // ---- esphome lifecycle ----
  void setup() override;
  void update() override;  // kontrola rozvrhu, bezi kazdou sekundu
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

 protected:
  void load_channel_(uint8_t channel);
  void save_channel_(uint8_t channel);
  void parse_schedule_(const char *csv, ScheduleSlot slots[MAX_SCHEDULE_SLOTS]);
  void check_schedule_();
  void process_queue_();
  void check_running_watering_();
  void start_watering_(uint8_t channel, float dose_ml);
  void stop_watering_();
  void publish_last_watered_(uint8_t channel);
  std::string format_epoch_(uint32_t epoch);

  time::RealTimeClock *time_{nullptr};

  GPIOPin *valve_enable_pin_{nullptr};
  GPIOPin *valve_signal_pin_{nullptr};
  GPIOPin *address_pins_[4]{nullptr, nullptr, nullptr, nullptr};
  GPIOPin *pump_pin_{nullptr};
  float ml_per_sec_{4.0f};

  void address_mux_(uint8_t channel);
  void set_valve_(bool on);

  ChannelConfig channels_[MAX_CHANNELS];
  ESPPreferenceObject channel_prefs_[MAX_CHANNELS];

  ZalejvatkoChannelSwitch *enable_switches_[MAX_CHANNELS]{nullptr};
  ZalejvatkoChannelDoseNumber *dose_numbers_[MAX_CHANNELS]{nullptr};
  ZalejvatkoChannelScheduleText *schedule_texts_[MAX_CHANNELS]{nullptr};
  ZalejvatkoChannelWaterNowButton *water_now_buttons_[MAX_CHANNELS]{nullptr};
  ZalejvatkoChannelLastWateredTextSensor *last_watered_sensors_[MAX_CHANNELS]{nullptr};

  std::deque<WateringCommand> queue_;
  bool watering_in_progress_{false};
  uint8_t watering_channel_{0};
  uint32_t watering_end_ms_{0};
};

// ---------------------------------------------------------------------
// Jednoduche "front-end" entity, ktere jen delegujou stav na hub
// ---------------------------------------------------------------------

class ZalejvatkoChannelSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(ZalejvatkoComponent *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void setup() override { this->publish_state(this->parent_->get_channel_config(this->channel_).enabled); }

 protected:
  void write_state(bool state) override {
    this->parent_->set_channel_enabled(this->channel_, state);
    this->publish_state(state);
  }
  ZalejvatkoComponent *parent_{nullptr};
  uint8_t channel_{0};
};

class ZalejvatkoChannelDoseNumber : public number::Number, public Component {
 public:
  void set_parent(ZalejvatkoComponent *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void setup() override { this->publish_state(this->parent_->get_channel_config(this->channel_).dose_ml); }

 protected:
  void control(float value) override {
    this->parent_->set_channel_dose(this->channel_, value);
    this->publish_state(value);
  }
  ZalejvatkoComponent *parent_{nullptr};
  uint8_t channel_{0};
};

class ZalejvatkoChannelScheduleText : public text::Text, public Component {
 public:
  void set_parent(ZalejvatkoComponent *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void setup() override { this->publish_state(this->parent_->get_channel_config(this->channel_).schedule); }

 protected:
  void control(const std::string &value) override {
    this->parent_->set_channel_schedule(this->channel_, value);
    this->publish_state(value);
  }
  ZalejvatkoComponent *parent_{nullptr};
  uint8_t channel_{0};
};

class ZalejvatkoChannelWaterNowButton : public button::Button, public Component {
 public:
  void set_parent(ZalejvatkoComponent *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

 protected:
  void press_action() override { this->parent_->water_now(this->channel_); }
  ZalejvatkoComponent *parent_{nullptr};
  uint8_t channel_{0};
};

// jen zobrazuje stav, hub do ni zapisuje pri kazdem zalevani - neni to "controllable" entita
class ZalejvatkoChannelLastWateredTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(ZalejvatkoComponent *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

 protected:
  ZalejvatkoComponent *parent_{nullptr};
  uint8_t channel_{0};
};

}  // namespace zalejvatko
}  // namespace esphome
