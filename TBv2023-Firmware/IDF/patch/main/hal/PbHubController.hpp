#pragma once

#include <cstring>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct PbHubController
{
    static constexpr uint8_t  DEFAULT_I2C_ADDR = 0x61;
    static constexpr uint8_t  SCALE_I2C_ADDR   = 0x26;
    static constexpr uint32_t I2C_TIMEOUT_MS   = 100;

    #if CONFIG_M5STACK_STICK_S3
    static constexpr gpio_num_t PIN_BOLT_START = GPIO_NUM_0;
    static constexpr gpio_num_t PIN_BOLT_END   = GPIO_NUM_1;
    #else
    static constexpr gpio_num_t PIN_BOLT_START = GPIO_NUM_26;
    static constexpr gpio_num_t PIN_BOLT_END   = GPIO_NUM_25;
    #endif

    enum class Operation : uint8_t
    {
        WRITE_IO0       = 0x00,
        WRITE_IO1       = 0x01,
        PWM_IO0         = 0x02,
        PWM_IO1         = 0x03,
        READ_IO0        = 0x04,
        READ_IO1        = 0x05,
        ANALOG_READ_IO0 = 0x06,
    };

    enum class Channel : uint8_t
    {
        CHANNEL_0 = 0x40,
        CHANNEL_1 = 0x50,
        CHANNEL_2 = 0x60,
        CHANNEL_3 = 0x70,
        CHANNEL_4 = 0x80,
        CHANNEL_5 = 0xA0,
    };

    PbHubController() noexcept : i2c_addr_(DEFAULT_I2C_ADDR) {}

    ~PbHubController() 
    {
        if (dev_hub_handle_)   { i2c_master_bus_rm_device(dev_hub_handle_); }
        if (dev_scale_handle_) { i2c_master_bus_rm_device(dev_scale_handle_); }
    }

    /**
     * @brief Initialize GPIO inputs and register I2C slave devices to your Master Bus
     */
   bool begin(i2c_master_bus_handle_t bus_handle)
    {
        init_gpio_inputs();
        bus_handle_ = bus_handle; // Store the bus handle for probing later

        // 1. Configure and add PbHub Device
        i2c_device_config_t hub_cfg = {}; // Zero-initialize all fields first
        hub_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        hub_cfg.device_address = i2c_addr_;
        hub_cfg.scl_speed_hz = 100000;
        
        if (i2c_master_bus_add_device(bus_handle, &hub_cfg, &dev_hub_handle_) != ESP_OK) {
            return false;
        }

        // 2. Configure and add Scale Device
        i2c_device_config_t scale_cfg = {}; // Zero-initialize all fields first
        scale_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        scale_cfg.device_address = SCALE_I2C_ADDR;
        scale_cfg.scl_speed_hz = 100000;

        if (i2c_master_bus_add_device(bus_handle, &scale_cfg, &dev_scale_handle_) != ESP_OK) {
            return false;
        }

        return true;
    }

    /**
     * @brief Check if the device responds over I2C (replaces Wire.endTransmission logic)
     */
    bool check_status()
    {
        if (!bus_handle_) return false;
        // Correct signature: probe(bus_handle, 7bit_address, timeout_ms)
        return (i2c_master_probe(bus_handle_, i2c_addr_, I2C_TIMEOUT_MS) == ESP_OK);
    }

    // ── GPIO bolt sensors ─────────────────────────────────────────────────────
    bool is_bolt_at_start() const { return gpio_get_level(PIN_BOLT_START) == 0; }
    bool is_bolt_at_end()   const { return gpio_get_level(PIN_BOLT_END)   == 0; }

    // ── Scale ─────────────────────────────────────────────────────────────────
    float read_scale_weight_g()
    {
        if (!dev_scale_handle_) return -999.0f;

        uint8_t reg = 0x10;
        esp_err_t err = i2c_master_transmit(dev_scale_handle_, &reg, 1, I2C_TIMEOUT_MS);
        if (err != ESP_OK) return -999.0f;

        // delay(5) -> Convert to FreeRTOS ticks
        vTaskDelay(pdMS_TO_TICKS(5));

        uint8_t data[4] = {0};
        err = i2c_master_receive(dev_scale_handle_, data, 4, I2C_TIMEOUT_MS);
        if (err != ESP_OK) return -999.0f;

        float weight;
        std::memcpy(&weight, data, 4);
        return weight;
    }

    void tare_scale()
    {
        if (!dev_scale_handle_) return;
        uint8_t buf[2] = {0x50, 0x01};
        i2c_master_transmit(dev_scale_handle_, buf, sizeof(buf), I2C_TIMEOUT_MS);
    }

    void set_scale_led_color(uint32_t rgb_24bit)
    {
        if (!dev_scale_handle_) return;
        uint8_t buf[4] = {
            0x30,
            static_cast<uint8_t>((rgb_24bit >> 16) & 0xFF),
            static_cast<uint8_t>((rgb_24bit >> 8)  & 0xFF),
            static_cast<uint8_t>(rgb_24bit         & 0xFF)
        };
        i2c_master_transmit(dev_scale_handle_, buf, sizeof(buf), I2C_TIMEOUT_MS);
    }

    // ── PbHub digital/analog ──────────────────────────────────────────────────
    bool read_digital_IO0(const Channel channel)
    {
        uint8_t value = 0;
        read_operation(channel, Operation::READ_IO0, &value, sizeof(value));
        return value != 0;
    }

    bool read_digital_IO1(const Channel channel)
    {
        uint8_t value = 0;
        read_operation(channel, Operation::READ_IO1, &value, sizeof(value));
        return value != 0;
    }

    uint16_t read_analog_IO0(const Channel channel)
    {
        uint16_t value = 0;
        read_operation(channel, Operation::ANALOG_READ_IO0,
                       reinterpret_cast<uint8_t*>(&value), sizeof(value));
        return value;
    }

    void write_digital_IO0(const Channel channel, const bool value)
    {
        uint8_t data = value ? 1 : 0;
        write_operation(channel, Operation::WRITE_IO0, &data, sizeof(data));
    }

    void write_digital_IO1(const Channel channel, const bool value)
    {
        uint8_t data = value ? 1 : 0;
        write_operation(channel, Operation::WRITE_IO1, &data, sizeof(data));
    }

    void write_PWM_IO0(const Channel channel, const uint8_t value)
    {
        uint8_t data = value;
        write_operation(channel, Operation::PWM_IO0, &data, sizeof(data));
    }

    void write_PWM_IO1(const Channel channel, const uint8_t value)
    {
        uint8_t data = value;
        write_operation(channel, Operation::PWM_IO1, &data, sizeof(data));
    }

protected:

    virtual bool read_operation(const Channel channel, const Operation operation,
                                uint8_t* data, const size_t length)
    {
        if (!dev_hub_handle_) return false;

        uint8_t reg = static_cast<uint8_t>(channel) + static_cast<uint8_t>(operation);
        
        // Transmit the target register
        esp_err_t err = i2c_master_transmit(dev_hub_handle_, &reg, 1, I2C_TIMEOUT_MS);
        if (err != ESP_OK) return false;

        vTaskDelay(pdMS_TO_TICKS(5));

        // Request the incoming bytes
        err = i2c_master_receive(dev_hub_handle_, data, length, I2C_TIMEOUT_MS);
        return (err == ESP_OK);
    }

    virtual bool write_operation(const Channel channel, const Operation operation,
                                 const uint8_t* data, const size_t length)
    {
        if (!dev_hub_handle_) return false;

        uint8_t reg = static_cast<uint8_t>(channel) + static_cast<uint8_t>(operation);
        
        // Dynamic allocations are avoided by creating a single combined buffer stack array
        uint8_t buf[16]; 
        if (length + 1 > sizeof(buf)) return false; 

        buf[0] = reg;
        std::memcpy(&buf[1], data, length);

        esp_err_t err = i2c_master_transmit(dev_hub_handle_, buf, length + 1, I2C_TIMEOUT_MS);
        return (err == ESP_OK);
    }

private:
    const uint8_t i2c_addr_;
    i2c_master_bus_handle_t bus_handle_       = nullptr;
    i2c_master_dev_handle_t dev_hub_handle_   = nullptr;
    i2c_master_dev_handle_t dev_scale_handle_ = nullptr;

    void init_gpio_inputs()
    {
        gpio_config_t cfg = {};
        cfg.mode         = GPIO_MODE_INPUT;
        cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type    = GPIO_INTR_DISABLE;

        cfg.pin_bit_mask = (1ULL << PIN_BOLT_START);
        gpio_config(&cfg);

        cfg.pin_bit_mask = (1ULL << PIN_BOLT_END);
        gpio_config(&cfg);
    }
};
