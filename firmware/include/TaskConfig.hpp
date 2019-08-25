#pragma once

class TaskConfig
{
public:
  static constexpr uint32_t PIDHeater_stacksize = 4000u;  // words
  static constexpr uint32_t PIDHeater_priority = 5;

  static constexpr uint32_t Shot_stacksize = 4000u;  // words
  static constexpr uint32_t Shot_priority = 3;

  static constexpr uint32_t SensorsHandler_stacksize = 1000u;  // words
  static constexpr uint32_t SensorsHandler_priority = 4;

  static constexpr uint32_t WiFi_http_stacksize = 5000u;  // words
  static constexpr uint32_t WiFi_http_priority = 2;

  static constexpr uint32_t WiFi_influx_stacksize = 5000u;  // words
  static constexpr uint32_t WiFi_influx_priority = 2;

  static constexpr uint32_t WiFi_ota_stacksize = 5000u;  // words
  static constexpr uint32_t WiFi_ota_priority = 3;
};
