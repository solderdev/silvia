
static float kP = 0.0f;
static float kI = 0.0f;
static float kD = 0.0f;
static uint32_t ts = 0;  // [ms]

void setup_pid(float p, float i, float d, uint32_t ts_ms)
{
  // P, I and D parameters
  kP = p;
  kI = i;
  kD = d;

  // sampling time
  ts = ts_ms;
  
}


void service_pid(void)
{
  static uint32_t last_update = 0;
  uint32_t now = millis();
  
  if (ts != 0 && now - last_update > ts)
  {
    last_update = now;

    // TODO
  }
}

