/**
 * Robothon Task Board Firmware - Hardened Scale Driver
 */

#pragma once

#include <hal/TaskBoardDriver.hpp>
#include <hal/HardwareLowLevelController.hpp>
#include <sensor/Sensor.hpp>
#include <sensor/AnalogFilteredSensor.hpp>
#include <sensor/CounterSensor.hpp>
#include <sensor/TriggeredSensor.hpp>
#include <task/TaskStepEqual.hpp>
#include <task/TaskStepEqualToRandom.hpp>
#include <task/SimultaneousConditionTask.hpp>
#include <task/SequentialTask.hpp>
#include <util/Timing.hpp>

#include <esp_mac.h>

struct TaskBoardDriver_v1 : public TaskBoardDriver
{
    const char* TAG = "TaskBoardDriver_v1";

    TaskBoardDriver_v1(m5::M5Unified& m5_unified)
        : pb_hub_controller_(new PbHubController()),
          hardware_low_level_controller_(*pb_hub_controller_, m5_unified),
          scales_present_(false),       // 1st in private declaration
          latest_weight_(0.0f),         // 2nd in private declaration
          scale_poll_counter_(0)        // 3rd in private declaration
    {
        // Unique ID Generation
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char mac_str[18];
        sprintf(mac_str, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        unique_id_ = mac_str;
        char ssid_with_mac[36];
        snprintf(ssid_with_mac, sizeof(ssid_with_mac), "Task Board v2023 Bolt Addon %01X%02X", (mac[4] & 0x0F), mac[5]);
        unique_ssid_ = ssid_with_mac;

        // 1. Force explicit I2C Bus recovery configuration
        ESP_LOGI(TAG, "Initializing External I2C Bus at 100kHz for stability...");
        
        // Skip calling m5_unified.Ex_I2C.begin() to avoid driver allocation collisions!
        
        i2c_master_bus_handle_t idf_bus_handle;
        i2c_master_bus_config_t bus_cfg = {}; // Clear memory profile
        bus_cfg.i2c_port = I2C_NUM_0;          // External Peripherals Port
        #if CONFIG_M5STACK_STICK_S3
        bus_cfg.sda_io_num = GPIO_NUM_9;
        bus_cfg.scl_io_num = GPIO_NUM_10;
        #else
        bus_cfg.sda_io_num = GPIO_NUM_32;
        bus_cfg.scl_io_num = GPIO_NUM_33;
        #endif

        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt = 7;
        bus_cfg.flags.enable_internal_pullup = 1; // Safeguard if external pull-ups are missing

        // Allocate the standard driver context
        esp_err_t bus_err = i2c_new_master_bus(&bus_cfg, &idf_bus_handle);
        if (bus_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize standard I2C master bus! Error: %s", esp_err_to_name(bus_err));
        }

        // Initialize our custom controller using our safe IDF Master Bus context
        if (!pb_hub_controller_->begin(idf_bus_handle)) {
            ESP_LOGE(TAG, "CRITICAL ERROR: Failed to register I2C devices to Master Bus!");
        }
        
        // The check_status probing validation loop can now safely execute 
        while (!hardware_low_level_controller_.pb_hub_controller.check_status())
        {
            ESP_LOGI(TAG, "Waiting for PbHubController to start");
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // 2. Hardened Scales Initialization Probe Loop using the integrated controller
        ESP_LOGI(TAG, "Probing M5 Scales hardware module via Hub Controller...");
        
        // Probe check using an initial weight test read attempt
        for (int retry = 0; retry < 5; ++retry) {
            latest_weight_ = hardware_low_level_controller_.pb_hub_controller.read_scale_weight_g();
            if (latest_weight_ != -999.0f) { // If it returns anything other than a failed bus flag
                scales_present_ = true;
                break;
            }
            ESP_LOGW(TAG, "Scale not responding on bus. Retry %d/5...", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (scales_present_) {
            ESP_LOGI(TAG, "Scales hardware confirmed present. Calibrating raw offsets...");
            
            hardware_low_level_controller_.pb_hub_controller.tare_scale(); 
            vTaskDelay(pdMS_TO_TICKS(300)); // Wait period for ADC conversion loop
            hardware_low_level_controller_.pb_hub_controller.set_scale_led_color(0x001000); // Brighter Green feedback on boot success
            
            latest_weight_ = hardware_low_level_controller_.pb_hub_controller.read_scale_weight_g();
            ESP_LOGI(TAG, "Initial test weight reading: %.2f g", latest_weight_);
        } else {
            ESP_LOGW(TAG, "M5 Scales hardware module not responsive. Defaulting to fallback state.");
            latest_weight_ = 0.0f;
        }

        vTaskDelay(pdMS_TO_TICKS(100));

        // Define Sensors 
        Sensor* scale = new Sensor("SCALE", [&]()
                        {
                            return SensorMeasurement(latest_weight_);
                        });

                                Sensor* scale_free = new Sensor("SCALE_FREE", [&]()
                        {
                            bool is_free = latest_weight_ < 30;
                            return SensorMeasurement(is_free);
                        });

        Sensor* scale_pushed = new Sensor("SCALE_PUSHED", [&]()
                        {
                            bool is_pushed = 250 < latest_weight_ && latest_weight_ < 500;
                            return SensorMeasurement(is_pushed);
                        });

        Sensor* bolt_at_start = new Sensor("BOLT_START", [&]()
                        {
                            #if CONFIG_PIN_PROFILE_EXTERNAL
                            bool value = hardware_low_level_controller_.pb_hub_controller.is_bolt_at_start();
                            return SensorMeasurement(!value);
                            #else
                            return SensorMeasurement(!hardware_low_level_controller_.pb_hub_controller.read_digital_IO0(PbHubController::Channel::CHANNEL_2));
                            #endif
                        });

        Sensor* bolt_at_end = new Sensor("BOLT_END", [&]()
                        {
                            #if CONFIG_PIN_PROFILE_EXTERNAL
                            bool value = hardware_low_level_controller_.pb_hub_controller.is_bolt_at_end();
                            return SensorMeasurement(!value);
                            #else
                            return SensorMeasurement(!hardware_low_level_controller_.pb_hub_controller.read_digital_IO1(PbHubController::Channel::CHANNEL_2));
                            #endif
                        });

        Sensor* blue_button = new Sensor("BLUE_BUTTON", [&]()
                        {
                            bool value = hardware_low_level_controller_.pb_hub_controller.read_digital_IO0(PbHubController::Channel::CHANNEL_0);
                            return SensorMeasurement(!value);
                        });

        Sensor* red_button = new Sensor("RED_BUTTON", [&]()
                        {
                            bool value = hardware_low_level_controller_.pb_hub_controller.read_digital_IO1(PbHubController::Channel::CHANNEL_0);
                            return SensorMeasurement(!value);
                        });

        Sensor* fader = new AnalogFilteredSensor("FADER", 10, [&]()
                        {
                            uint16_t value = hardware_low_level_controller_.pb_hub_controller.read_analog_IO0(PbHubController::Channel::CHANNEL_5);
                            return SensorMeasurement(static_cast<float>(value) / 4095.0f);
                        });

        Sensor* door_angle = new AnalogFilteredSensor("DOOR_ANGLE", 10, [&]()
                        {
                            uint16_t value = hardware_low_level_controller_.pb_hub_controller.read_analog_IO0(PbHubController::Channel::CHANNEL_4);
                            return SensorMeasurement(static_cast<float>(value) / 4095.0f);
                        });

        Sensor* light_right = new Sensor("LIGHT_RIGHT", [&]()
                        {
                            #if CONFIG_PIN_PROFILE_EXTERNAL
                            return SensorMeasurement(hardware_low_level_controller_.pb_hub_controller.read_digital_IO0(PbHubController::Channel::CHANNEL_2));
                            #else
                            return SensorMeasurement(hardware_low_level_controller_.pb_hub_controller.read_digital_IO1(PbHubController::Channel::CHANNEL_1));
                            #endif
                        });

        Sensor* light_left = new Sensor("LIGHT_LEFT", [&]()
                        {
                            return SensorMeasurement(hardware_low_level_controller_.pb_hub_controller.read_digital_IO0(PbHubController::Channel::CHANNEL_1));
                        });

        Sensor* free_cable = new Sensor("FREE_CABLE", [=]()
                        {
                            return SensorMeasurement(!light_right->read().get_boolean() && !light_left->read().get_boolean());
                        });

        Sensor* attached_cable = new Sensor("ATTACHED_CABLE", [=]()
                        {
                            return SensorMeasurement(light_right->read().get_boolean() && light_left->read().get_boolean());
                        });

        Sensor* probe_plugged_bool = new Sensor("PROBE_PLUGGED", [&]()
                        {
                            bool value = hardware_low_level_controller_.pb_hub_controller.read_digital_IO1(PbHubController::Channel::CHANNEL_3);
                            return SensorMeasurement(!value);
                        });

        Sensor* probe_goal_bool = new Sensor("PROBE_GOAL_BOOL", [&]()
                        {
                            bool value = hardware_low_level_controller_.pb_hub_controller.read_digital_IO0(PbHubController::Channel::CHANNEL_3);
                            return SensorMeasurement(!value);
                        });

        Sensor* probe_goal_analog = new Sensor("PROBE_GOAL_ANALOG", [&]()
                        {
                            uint16_t value = hardware_low_level_controller_.pb_hub_controller.read_analog_IO0(PbHubController::Channel::CHANNEL_3);
                            return SensorMeasurement(static_cast<float>(value) / 4095.0f);
                        });

        Sensor* probe_goal = new Sensor("PROBE_GOAL", [=]()
                        {
                            return SensorMeasurement(probe_goal_analog->read().get_analog() < 0.1);
                        });

        Sensor* on_board_button_a = new Sensor("ON_BOARD_BUTTON_A", [&]()
                        {
                            return SensorMeasurement(hardware_low_level_controller_.m5_unified.BtnA.isPressed());
                        });

        Sensor* on_board_button_b = new Sensor("ON_BOARD_BUTTON_B", [&]()
                        {
                            return SensorMeasurement(hardware_low_level_controller_.m5_unified.BtnB.isPressed());
                        });

        Sensor* on_board_button_c = new Sensor("ON_BOARD_BUTTON_C", [&]()
                        {
                            return SensorMeasurement(hardware_low_level_controller_.m5_unified.BtnC.isPressed());
                        });

        Sensor* on_board_button_pwr = new Sensor("ON_BOARD_BUTTON_PWR", [&]()
                        {
                            return SensorMeasurement(hardware_low_level_controller_.m5_unified.BtnPWR.isPressed());
                        });

        Sensor* accelerometer = new Sensor("ACCELEROMETER", [&]()
                        {
                            SensorMeasurement::Vector3 values;
                            hardware_low_level_controller_.m5_unified.Imu.getAccel(&values.x, &values.y, &values.z);
                            return SensorMeasurement(values);
                        });

        Sensor* magnetometer = new Sensor("MAGNETOMETER", [&]()
                        {
                            SensorMeasurement::Vector3 values;
                            hardware_low_level_controller_.m5_unified.Imu.getMag(&values.x, &values.y, &values.z);
                            return SensorMeasurement(values);
                        });

        Sensor* gyroscope = new Sensor("GYROSCOPE", [&]()
                        {
                            SensorMeasurement::Vector3 values;
                            hardware_low_level_controller_.m5_unified.Imu.getGyro(&values.x, &values.y, &values.z);
                            return SensorMeasurement(values);
                        });

        Sensor* temperature = new Sensor("TEMPERATURE", [&]()
                        {
                            float value = 0.0;
                            hardware_low_level_controller_.m5_unified.Imu.getTemp(&value);
                            return SensorMeasurement(value);
                        });

        Sensor* door_status = new Sensor("DOOR_OPEN", [=]()
                        {
                            bool is_open = true;
                            if (door_angle->read().get_type() == SensorMeasurement::Type::ANALOG)
                            {
                                if (door_angle->read().get_analog() > 0.7) { is_open = false; }
                            }
                            return SensorMeasurement(is_open);
                        });

        Sensor* blue_button_counter = new CounterSensor("BLUE_BUTTON_COUNTER", [=]() { return blue_button->read(); });
        Sensor* red_button_counter = new CounterSensor("RED_BUTTON_COUNTER", [=]() { return red_button->read(); });
        Sensor* probe_goal_counter = new CounterSensor("PROBE_GOAL_COUNTER", [=]() { return probe_goal_bool->read(); });

        sensors_.push_back(scale);
        sensors_.push_back(scale_free);
        sensors_.push_back(scale_pushed);
        sensors_.push_back(bolt_at_start);
        sensors_.push_back(bolt_at_end);
        sensors_.push_back(blue_button);
        sensors_.push_back(red_button);
        sensors_.push_back(fader);
        sensors_.push_back(door_angle);
        sensors_.push_back(light_right);
        sensors_.push_back(light_left);
        sensors_.push_back(probe_plugged_bool);
        sensors_.push_back(probe_goal_bool);
        sensors_.push_back(probe_goal_analog);
        sensors_.push_back(on_board_button_a);
        sensors_.push_back(on_board_button_b);
        sensors_.push_back(on_board_button_c);
        sensors_.push_back(on_board_button_pwr);
        sensors_.push_back(accelerometer);
        sensors_.push_back(magnetometer);
        sensors_.push_back(gyroscope);
        sensors_.push_back(temperature);
        sensors_.push_back(door_status);
        sensors_.push_back(free_cable);
        sensors_.push_back(attached_cable);
        sensors_.push_back(probe_goal);
        sensors_.push_back(blue_button_counter);
        sensors_.push_back(red_button_counter);
        sensors_.push_back(probe_goal_counter);

        update();

        // Standard Task Protocols
        std::vector<const TaskStepBase*>* precondition_steps = new std::vector<const TaskStepBase*>
        {
            new TaskStepEqual(*get_sensor_by_name("FADER"), SensorMeasurement(0.0f), 0.1f),
            new TaskStepEqual(*get_sensor_by_name("DOOR_OPEN"), SensorMeasurement(false)),
            new TaskStepEqual(*get_sensor_by_name("PROBE_GOAL"), SensorMeasurement(false)),
            new TaskStepEqual(*get_sensor_by_name("FREE_CABLE"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("PROBE_PLUGGED"), SensorMeasurement(false)),
            new TaskStepEqual(*get_sensor_by_name("BOLT_START"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("SCALE_FREE"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("BLUE_BUTTON"), SensorMeasurement(true)),
        };
        default_precondition_task_ = new SimultaneousConditionTask(*precondition_steps, "Precondition Task");

        std::vector<const TaskStepBase*>* main_steps = new std::vector<const TaskStepBase*>
        {
            new TaskStepEqual(*get_sensor_by_name("BLUE_BUTTON"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("FADER"), SensorMeasurement(0.5f), 0.05f),
            new TaskStepEqualToRandom(*get_sensor_by_name("FADER"), 0.05f),
            new TaskStepEqual(*get_sensor_by_name("PROBE_PLUGGED"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("DOOR_OPEN"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("PROBE_GOAL"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("ATTACHED_CABLE"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("PROBE_PLUGGED"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("BOLT_START"), SensorMeasurement(false)),
            new TaskStepEqual(*get_sensor_by_name("SCALE_PUSHED"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("BOLT_END"), SensorMeasurement(true)),
            new TaskStepEqual(*get_sensor_by_name("RED_BUTTON"), SensorMeasurement(true)),
        };
        default_task_ = new SequentialTask(*main_steps, "Bolt Addon Protocol");
    }

    ~TaskBoardDriver_v1() {}

    Task& get_default_task() override { return *default_task_; }
    Task& get_default_task_precondition() override { return *default_precondition_task_; }
    const std::string& get_unique_id() const override { return unique_id_; }
    const std::string& get_unique_ssid() const override { return unique_ssid_; }
    HardwareLowLevelController& get_hardware_low_level_controller() override { return hardware_low_level_controller_; }
    uint32_t get_sensor_count() const override { return sensors_.size(); }

    SensorReader* get_sensor(const size_t& index) const override {
        return (index < sensors_.size()) ? sensors_[index] : nullptr;
    }

    SensorReader* get_sensor_by_name(const std::string& sensor_name) const override {
        for (auto const& s : sensors_) {
            if (s->name() == sensor_name) return s;
        }
        return nullptr;
    }

    /**
     * @brief High Priority Update Loop
     */
    /**
     * @brief High Priority Update Loop
     */
    void update() override
    {
        hardware_low_level_controller_.m5_unified.update();
        hardware_low_level_controller_.m5_unified.Imu.update();

        #if CONFIG_PIN_PROFILE_INFRARED_Y_CABLE
        hardware_low_level_controller_.pb_hub_controller.write_digital_IO0(PbHubController::Channel::CHANNEL_2, true);
        hardware_low_level_controller_.pb_hub_controller.write_digital_IO1(PbHubController::Channel::CHANNEL_2, true);
        #endif

        hardware_low_level_controller_.pb_hub_controller.write_digital_IO0(PbHubController::Channel::CHANNEL_3, true);
        hardware_low_level_controller_.pb_hub_controller.write_digital_IO1(PbHubController::Channel::CHANNEL_3, true);

        // Throttled scale reading sequence using the shared hub controller
        if (++scale_poll_counter_ % 15 == 0)
        {
            float raw_reading = hardware_low_level_controller_.pb_hub_controller.read_scale_weight_g();
            
            // Check for valid range boundary (ignores the -999.0f failure state)
            if (raw_reading > -500.0f && raw_reading < 20000.0f) { 
                latest_weight_ = raw_reading;
                scales_present_ = true;
            } else {
                scales_present_ = false;
            }
            
            // Apply status LEDs directly via the verified 0x30 register
            if (scales_present_) {
                if (latest_weight_ >= 500.0f) {
                    hardware_low_level_controller_.pb_hub_controller.set_scale_led_color(0x100000); // Gentle Red
                } else if (latest_weight_ >= 250.0f) {
                    hardware_low_level_controller_.pb_hub_controller.set_scale_led_color(0x101000); // Gentle Yellow
                } else {
                    hardware_low_level_controller_.pb_hub_controller.set_scale_led_color(0x001000); // Gentle Green
                }
            }
        }

        for (auto& item : sensors_)
        {
            item->update();
        }
    }

    void tare_scale() 
    {
        if (scales_present_) {
            hardware_low_level_controller_.pb_hub_controller.tare_scale();
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }

private:
    PbHubController* pb_hub_controller_;
    HardwareLowLevelController hardware_low_level_controller_;
    std::vector<Sensor*> sensors_;
    std::string unique_id_ = "TaskBoard_v1";
    std::string unique_ssid_ = "Robothon Task Board";

    Task* default_task_;
    Task* default_precondition_task_;

    bool scales_present_;
    float latest_weight_;
    uint32_t scale_poll_counter_;
};