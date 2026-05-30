#pragma once

#include <Wire.h>
#include <driver/gpio.h>

struct PbHubController {
  static constexpr uint8_t DEFAULT_I2C_ADDR = 0x61;
  static constexpr uint8_t SCALE_I2C_ADDR = 0x26;

#if defined(ARDUINO_M5STACK_STICKS3)
  static constexpr gpio_num_t PIN_BOLT_START = GPIO_NUM_0;
  static constexpr gpio_num_t PIN_BOLT_END = GPIO_NUM_1;
#elif defined(ARDUINO_M5STACK_STICKC_PLUS2)
  static constexpr gpio_num_t PIN_BOLT_START = GPIO_NUM_26;
  static constexpr gpio_num_t PIN_BOLT_END = GPIO_NUM_25;
#else
  static constexpr gpio_num_t PIN_BOLT_START = GPIO_NUM_0;
  static constexpr gpio_num_t PIN_BOLT_END = GPIO_NUM_1;
#endif


  enum class Operation : uint8_t {
    WRITE_IO0 = 0x00,
    WRITE_IO1 = 0x01,
    PWM_IO0 = 0x02,
    PWM_IO1 = 0x03,
    READ_IO0 = 0x04,
    READ_IO1 = 0x05,
    ANALOG_READ_IO0 = 0x06,
  };

  enum class Channel : uint8_t {
    CHANNEL_0 = 0x40,
    CHANNEL_1 = 0x50,
    CHANNEL_2 = 0x60,
    CHANNEL_3 = 0x70,
    CHANNEL_4 = 0x80,
    CHANNEL_5 = 0xA0,
  };

  PbHubController() noexcept
    : i2c_addr_(DEFAULT_I2C_ADDR) {}

  void begin() {
    Wire.begin();
    init_gpio_inputs();
  }

  bool check_status() {
    Wire.beginTransmission(i2c_addr_);
    return Wire.endTransmission() == 0;
  }

  // ── GPIO bolt sensors ─────────────────────────────────────────────────────
  // Pins are pulled up, short-to-GND = active, gpio_get_level returns 0 when active
  bool is_bolt_at_start() const {
    return gpio_get_level(PIN_BOLT_START) == 0;
  }
  bool is_bolt_at_end() const {
    return gpio_get_level(PIN_BOLT_END) == 0;
  }

  // ── Scale ─────────────────────────────────────────────────────────────────
  float read_scale_weight_g() {
    Wire.beginTransmission(SCALE_I2C_ADDR);
    Wire.write(0x10);
    if (Wire.endTransmission(true) != 0) return -999.0f;
    delay(5);
    if (Wire.requestFrom(SCALE_I2C_ADDR, (uint8_t)4) != 4) return -999.0f;
    uint8_t data[4];
    for (int i = 0; i < 4; i++) data[i] = Wire.read();
    float weight;
    memcpy(&weight, data, 4);
    return weight;
  }

  void tare_scale() {
    Wire.beginTransmission(SCALE_I2C_ADDR);
    Wire.write(0x50);
    Wire.write((uint8_t)1);
    Wire.endTransmission();
  }

  void set_scale_led_color(uint32_t rgb_24bit) {
    uint8_t buf[3] = {
      (uint8_t)((rgb_24bit >> 16) & 0xFF),
      (uint8_t)((rgb_24bit >> 8) & 0xFF),
      (uint8_t)(rgb_24bit & 0xFF)
    };
    Wire.beginTransmission(SCALE_I2C_ADDR);
    Wire.write(0x30);
    Wire.write(buf, 3);
    Wire.endTransmission();
  }

  // ── PbHub digital/analog ──────────────────────────────────────────────────
  bool read_digital_IO0(const Channel channel) {
    uint8_t value = 0;
    read_operation(channel, Operation::READ_IO0, &value, sizeof(value));
    return value != 0;
  }

  bool read_digital_IO1(const Channel channel) {
    uint8_t value = 0;
    read_operation(channel, Operation::READ_IO1, &value, sizeof(value));
    return value != 0;
  }

  uint16_t read_analog_IO0(const Channel channel) {
    uint16_t value = 0;
    read_operation(channel, Operation::ANALOG_READ_IO0,
                   reinterpret_cast<uint8_t*>(&value), sizeof(value));
    return value;
  }

  void write_digital_IO0(const Channel channel, const bool value) {
    uint8_t data = value ? 1 : 0;
    write_operation(channel, Operation::WRITE_IO0, &data, sizeof(data));
  }

  void write_digital_IO1(const Channel channel, const bool value) {
    uint8_t data = value ? 1 : 0;
    write_operation(channel, Operation::WRITE_IO1, &data, sizeof(data));
  }

  void write_PWM_IO0(const Channel channel, const uint8_t value) {
    uint8_t data = value;
    write_operation(channel, Operation::PWM_IO0, &data, sizeof(data));
  }

  void write_PWM_IO1(const Channel channel, const uint8_t value) {
    uint8_t data = value;
    write_operation(channel, Operation::PWM_IO1, &data, sizeof(data));
  }

protected:

  virtual bool read_operation(const Channel channel, const Operation operation,
                              uint8_t* data, const size_t length) {
    uint8_t reg = static_cast<uint8_t>(channel) + static_cast<uint8_t>(operation);

    Wire.beginTransmission(i2c_addr_);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0) return false;
    delay(5);

    if (Wire.requestFrom(i2c_addr_, (uint8_t)length) != length) return false;
    for (size_t i = 0; i < length; i++) data[i] = Wire.read();
    return true;
  }

  virtual bool write_operation(const Channel channel, const Operation operation,
                               const uint8_t* data, const size_t length) {
    uint8_t reg = static_cast<uint8_t>(channel) + static_cast<uint8_t>(operation);
    Wire.beginTransmission(i2c_addr_);
    Wire.write(reg);
    Wire.write(data, length);
    return Wire.endTransmission() == 0;
  }

private:
  const uint8_t i2c_addr_ = DEFAULT_I2C_ADDR;

  void init_gpio_inputs() {
    gpio_config_t cfg = {};
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;

    cfg.pin_bit_mask = (1ULL << PIN_BOLT_START);
    gpio_config(&cfg);

    cfg.pin_bit_mask = (1ULL << PIN_BOLT_END);
    gpio_config(&cfg);
  }
};