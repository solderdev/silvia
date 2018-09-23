#ifndef SHOT_H_
#define SHOT_H_

typedef enum : uint8_t {
  SHOT_OFF = 0,
  SHOT_INIT_FILL,
  SHOT_RAMP,
  SHOT_PAUSE,
  SHOT_100PERCENT,
  SHOT_MANUAL,
  
} SHOT_State_t;

#endif /* SHOT_H_ */
