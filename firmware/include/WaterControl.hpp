#pragma once

class SSR;
class SSRPump;
class Shot;
class Preheat;
class PIDHeater;


typedef enum {
  WATERCTRL_OFF = 0,
  WATERCTRL_WATER,
  WATERCTRL_WATER_VALVE,
  WATERCTRL_SHOT,
  WATERCTRL_PREHEAT,
  WATERCTRL_STEAM
} WATERCTRL_State_t;


class WaterControl
{
public:
  WaterControl();
  WaterControl(WaterControl const&) = delete;
  void operator=(WaterControl const&)  = delete;
  static WaterControl* getInstance();
  void enable();
  void disable();
  void startPump(uint8_t percent, bool valve);
  void overridePump(uint8_t percent, uint16_t time_ms);
  void overridePumpCheck(void);
  void startShot();
  uint32_t getShotTime();
  void startPreheat();
  void startSteam(uint8_t pump_percent = 0, bool new_state_valve = false);
  void stop(uint8_t new_pump_percent = 0, bool new_state_valve = false, WATERCTRL_State_t new_state = WATERCTRL_OFF);
  PIDHeater *getBoilerPID() {return pid_boiler_;};

private:
  SSRPump *pump_;
  SSR *valve_;
  friend class Shot;
  Shot *shot_;
  friend class Preheat;
  Preheat *preheat_;
  friend class PIDHeater;
  PIDHeater *pid_boiler_;

  WATERCTRL_State_t state_;
  uint8_t pump_override_percent_;  // override pump with this value if positive
  uint16_t pump_override_total_ms_;  // for how many milli-secs the override should be in place
  uint16_t pump_override_active_ms_;  // for how long the override is already active
};
