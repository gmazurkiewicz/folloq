// AlphaBot: line follower (default) + hardware test (on demand).
// Sensors: TLC1543 ADC (bit-banged), L298P motor driver. Controlled/monitored from Python over the Router Bridge.
// Debug log goes to the App Lab Serial Monitor.
#include <Arduino_RouterBridge.h>

// ---------------- Pins (from docs/AlphaBot_Pinout.md) ----------------
#define PIN_ENA   5    // left motor PWM
#define PIN_ENB   6    // right motor PWM
#define PIN_IN1   A0   // left  motor
#define PIN_IN2   A1
#define PIN_IN3   A2   // right motor
#define PIN_IN4   A3
#define PIN_ADC_CS    10
#define PIN_ADC_CLK   13
#define PIN_ADC_ADDR  12   // MCU -> TLC1543 address input
#define PIN_ADC_DOUT  11   // TLC1543 data out -> MCU

// ---------------- Config ----------------
#define NUM_SENSORS        5
#define BATTERY_CHANNEL    10      // TA10 (schematic), NOT 5
#define BATTERY_DIVIDER    2.0f
#define ADC_VREF           4.6f    // measured 5V rail; adjust after measuring
#define PWM_CAP_NORMAL     204     // 80% of 255
#define PWM_CAP_BOOST      255
#define STEP_MS            1500
#define PAUSE_MS           500
#define WATCHDOG_MS        2000    // motors stop if Python stops polling
#define LOG_PERIOD_MS      1000
#define SAMPLE_MS          20

// Line follower tuning. Line is DARK: sensor reading is LOW on the line.
#define DEFAULT_SPEED_PCT  35      // base speed in % of 255
#define KP                 0.30f   // steering gain per unit of position error (-2..2)
#define KD                 0.60f   // derivative gain (per sample, SAMPLE_MS)
#define FOUND_THRESHOLD    0.35f   // min normalized strength on any sensor to count as "line seen"
#define CAL_MIN_RANGE      150     // min (max-min) before a sensor's own calibration is trusted
#define DEFAULT_LO         250     // fallback "on line" reading
#define DEFAULT_HI         900     // fallback "off line" reading
#define LOST_SEARCH_MS     2000    // spin toward the last side this long, then stop
#define SEARCH_SPEED_PCT   30

// ---------------- Modes ----------------
enum Mode { M_IDLE, M_FOLLOW, M_TEST };
enum TestState { T_NONE, T_RUNNING, T_DONE, T_ABORTED };

static Mode mode = M_FOLLOW;            // follower runs by default
static TestState testState = T_NONE;
static int  pwmCap = PWM_CAP_NORMAL;
static bool boostEnabled = false;
static int  speedPct = DEFAULT_SPEED_PCT;
static unsigned long lastPollMs = 0;
static bool everPolled = false;

// ---------------- Sensors ----------------
static uint16_t sensors[NUM_SENSORS];
static uint16_t calMin[NUM_SENSORS], calMax[NUM_SENSORS];
static float    strength[NUM_SENSORS];  // 0..1, 1 = on the dark line
static uint16_t batteryRaw = 0;
static float    batteryV = 0;
static float    linePos = 0;            // -2 (IR1, left) .. +2 (IR5, right)
static bool     lineFound = false;

// ---------------- Follower state ----------------
static float lastErr = 0;
static int   lastSide = 0;              // -1 line was left, +1 right, 0 unknown
static unsigned long lostSinceMs = 0;
static bool  lostStopped = false;
static int   cmdL = 0, cmdR = 0;        // last commanded signed PWM

// ---------------- Test steps ----------------
static int  stepIdx = 0;
static bool inPause = false;
static unsigned long stepStartMs = 0;

struct Step { const char* name; int8_t l; int8_t r; bool ramp; bool boostOnly; };
static const Step STEPS[] = {
  {"Left forward",   1,  0, false, false},
  {"Left backward", -1,  0, false, false},
  {"Right forward",  0,  1, false, false},
  {"Right backward", 0, -1, false, false},
  {"Both forward",   1,  1, false, false},
  {"Both backward", -1, -1, false, false},
  {"Spin left",     -1,  1, false, false},
  {"Spin right",     1, -1, false, false},
  {"Ramp forward",   1,  1, true,  false},
  {"Full 100% fwd",  1,  1, false, true},
};
static const int NUM_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);

// ---------------- Motors ----------------
static void setMotor(int enPin, int inA, int inB, int signedPwm) {
  int pwm = abs(signedPwm);
  if (pwm > pwmCap) pwm = pwmCap;
  if (signedPwm > 0)      { digitalWrite(inA, HIGH); digitalWrite(inB, LOW); }
  else if (signedPwm < 0) { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  else                    { digitalWrite(inA, LOW);  digitalWrite(inB, LOW); pwm = 0; }
  analogWrite(enPin, pwm);
}

static void driveLR(int l, int r) {
  if (l > pwmCap) l = pwmCap; if (l < -pwmCap) l = -pwmCap;
  if (r > pwmCap) r = pwmCap; if (r < -pwmCap) r = -pwmCap;
  cmdL = l; cmdR = r;
  setMotor(PIN_ENA, PIN_IN1, PIN_IN2, l);
  setMotor(PIN_ENB, PIN_IN3, PIN_IN4, r);
}

static void motorsStop() {
  cmdL = cmdR = 0;
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
  digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
}

// ---------------- TLC1543 (bit-banged) ----------------
// Sends 4-bit channel address while clocking out the previous result.
static uint16_t tlcTransfer(uint8_t nextChannel) {
  uint16_t value = 0;
  digitalWrite(PIN_ADC_CS, LOW);
  delayMicroseconds(2);
  for (int i = 0; i < 10; i++) {
    if (i < 4) digitalWrite(PIN_ADC_ADDR, (nextChannel >> (3 - i)) & 1);
    value <<= 1;
    if (digitalRead(PIN_ADC_DOUT)) value |= 1;
    digitalWrite(PIN_ADC_CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(PIN_ADC_CLK, LOW);
    delayMicroseconds(2);
  }
  digitalWrite(PIN_ADC_CS, HIGH);
  delayMicroseconds(25);  // conversion time (EOC not wired)
  return value;
}

static uint16_t readAdc(uint8_t channel) {
  tlcTransfer(channel);          // select channel, discard old result
  return tlcTransfer(channel);   // result of requested channel
}

static void calReset() {
  for (int i = 0; i < NUM_SENSORS; i++) { calMin[i] = 1023; calMax[i] = 0; }
  Monitor.println("[CAL] reset");
}

// Reads sensors, updates calibration, computes line strength and position.
static void readAll() {
  float sum = 0, weighted = 0, maxS = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    uint16_t v = readAdc(i);
    sensors[i] = v;
    if (v < calMin[i]) calMin[i] = v;
    if (v > calMax[i]) calMax[i] = v;
    bool trusted = (calMax[i] - calMin[i]) >= CAL_MIN_RANGE;
    float lo = trusted ? calMin[i] : DEFAULT_LO;
    float hi = trusted ? calMax[i] : DEFAULT_HI;
    float s = (hi - v) / (hi - lo);          // dark (low) -> 1
    if (s < 0) s = 0; if (s > 1) s = 1;
    strength[i] = s;
    sum += s;
    weighted += s * (float)(i - 2);
    if (s > maxS) maxS = s;
  }
  batteryRaw = readAdc(BATTERY_CHANNEL);
  batteryV = batteryRaw * ADC_VREF / 1023.0f * BATTERY_DIVIDER;

  lineFound = maxS >= FOUND_THRESHOLD && sum > 0.01f;
  if (lineFound) linePos = weighted / sum;
}

// ---------------- Follower ----------------
static void followTick() {
  if (!lineFound) {
    // Line lost: spin toward the side where it was last seen, then give up
    unsigned long now = millis();
    if (lostSinceMs == 0) { lostSinceMs = now; Monitor.println("[FOLLOW] line lost"); }
    if (lastSide == 0 || lostStopped || now - lostSinceMs > LOST_SEARCH_MS) {
      if (!lostStopped && lastSide != 0) Monitor.println("[FOLLOW] search timeout, stopped");
      lostStopped = true;
      motorsStop();
      return;
    }
    int sp = SEARCH_SPEED_PCT * 255 / 100;
    driveLR(lastSide * sp, -lastSide * sp);
    return;
  }
  if (lostSinceMs != 0) Monitor.println("[FOLLOW] line found");
  lostSinceMs = 0;
  lostStopped = false;

  float err = linePos;                       // + = line on the right
  float d = err - lastErr;
  lastErr = err;
  if (err > 0.3f) lastSide = 1; else if (err < -0.3f) lastSide = -1;

  float steer = KP * err + KD * d;           // fraction of base speed
  float base = speedPct * 255.0f / 100.0f;
  int l = (int)(base * (1.0f + steer));
  int r = (int)(base * (1.0f - steer));
  driveLR(l, r);
}

// ---------------- Test state machine ----------------
static const char* modeName() {
  switch (mode) { case M_FOLLOW: return "follow"; case M_TEST: return "test"; default: return "idle"; }
}
static const char* testName() {
  switch (testState) {
    case T_NONE: return "none"; case T_RUNNING: return "running";
    case T_DONE: return "done"; default: return "aborted";
  }
}

static void startStep(int idx) {
  stepIdx = idx;
  inPause = false;
  stepStartMs = millis();
  Monitor.print("[TEST] step "); Monitor.print(idx + 1); Monitor.print("/"); Monitor.print(NUM_STEPS);
  Monitor.print(" '"); Monitor.print(STEPS[idx].name); Monitor.println("'");
}

static void finishTest(TestState s) {
  motorsStop();
  testState = s;
  mode = M_IDLE;
  Monitor.print("[TEST] finished: "); Monitor.println(testName());
}

static void testTick() {
  unsigned long now = millis();
  unsigned long el = now - stepStartMs;

  if (inPause) {
    if (el >= PAUSE_MS) {
      int next = stepIdx + 1;
      while (next < NUM_STEPS && STEPS[next].boostOnly && !boostEnabled) next++;
      if (next >= NUM_STEPS) finishTest(T_DONE);
      else startStep(next);
    }
    return;
  }

  const Step& s = STEPS[stepIdx];
  if (el >= STEP_MS) {
    motorsStop();
    inPause = true;
    stepStartMs = now;
    return;
  }
  int pwm;
  if (s.ramp) {
    float f = (float)el / STEP_MS;
    f = f < 0.5f ? f * 2 : (1 - f) * 2;
    pwm = (int)(f * pwmCap);
  } else {
    int third = el * 3 / STEP_MS;
    int pct = s.boostOnly ? 100 : (third == 0 ? 40 : (third == 1 ? 60 : 80));
    pwm = pct * 255 / 100;
    if (!s.boostOnly && pwm > PWM_CAP_NORMAL) pwm = PWM_CAP_NORMAL;
  }
  driveLR(s.l * pwm, s.r * pwm);
}

// ---------------- Bridge handlers ----------------
// mode(0 idle,1 follow,2 test),testState(0 none,1 running,2 done,3 aborted),step,pause, s1..s5, n1..n5 (0-100), pos(milli), found,
// cmdL, cmdR, batRaw, batMv, cap, speed
static String getState() {
  lastPollMs = millis();
  everPolled = true;
  String s;
  s.reserve(120);
  s += (int)mode; s += ",";
  s += (int)testState; s += ",";
  s += (mode == M_TEST) ? (stepIdx + 1) : 0; s += ",";
  s += inPause ? 1 : 0; s += ",";
  for (int i = 0; i < NUM_SENSORS; i++) { s += sensors[i]; s += ","; }
  for (int i = 0; i < NUM_SENSORS; i++) { s += (int)(strength[i] * 100); s += ","; }
  s += (int)(linePos * 1000); s += ",";
  s += lineFound ? 1 : 0; s += ",";
  s += cmdL; s += ",";
  s += cmdR; s += ",";
  s += batteryRaw; s += ",";
  s += (int)(batteryV * 1000); s += ",";
  s += pwmCap; s += ",";
  s += speedPct;
  return s;
}

static void applyBoost(bool boost) {
  boostEnabled = boost;
  pwmCap = boost ? PWM_CAP_BOOST : PWM_CAP_NORMAL;
}

static String testStart(bool boost) {
  applyBoost(boost);
  lastPollMs = millis();
  Monitor.print("[TEST] start, boost="); Monitor.print(boost);
  Monitor.print(" cap="); Monitor.println(pwmCap);
  motorsStop();
  mode = M_TEST;
  testState = T_RUNNING;
  startStep(0);
  return "ok";
}

static String followStart(int speed, bool boost) {
  applyBoost(boost);
  if (speed < 10) speed = 10;
  if (speed > (boost ? 100 : 80)) speed = boost ? 100 : 80;
  speedPct = speed;
  lastErr = 0; lostSinceMs = 0; lostStopped = false;
  motorsStop();
  mode = M_FOLLOW;
  Monitor.print("[FOLLOW] start, speed="); Monitor.print(speedPct);
  Monitor.print("% cap="); Monitor.println(pwmCap);
  return "ok";
}

// Stops everything (both test and follower) and goes idle
static String stopAll() {
  Monitor.println("[STOP] idle");
  if (mode == M_TEST) finishTest(T_ABORTED);
  mode = M_IDLE;
  motorsStop();
  applyBoost(false);
  return "ok";
}

static String calibrationReset() { calReset(); return "ok"; }

void setup() {
  pinMode(PIN_ENA, OUTPUT); pinMode(PIN_ENB, OUTPUT);
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  motorsStop();

  pinMode(PIN_ADC_CS, OUTPUT);   digitalWrite(PIN_ADC_CS, HIGH);
  pinMode(PIN_ADC_CLK, OUTPUT);  digitalWrite(PIN_ADC_CLK, LOW);
  pinMode(PIN_ADC_ADDR, OUTPUT); digitalWrite(PIN_ADC_ADDR, LOW);
  pinMode(PIN_ADC_DOUT, INPUT);

  Monitor.begin(115200);
  calReset();
  Bridge.begin();
  Bridge.provide("get_state", getState);
  Bridge.provide("test_start", testStart);
  Bridge.provide("follow_start", followStart);
  Bridge.provide("stop_all", stopAll);
  Bridge.provide("cal_reset", calibrationReset);

  Monitor.println("[BOOT] AlphaBot line follower ready (default mode: follow)");
  Monitor.print("[BOOT] cap normal="); Monitor.print(PWM_CAP_NORMAL);
  Monitor.print(" boost="); Monitor.print(PWM_CAP_BOOST);
  Monitor.print(" speed="); Monitor.print(speedPct);
  Monitor.print("% vref="); Monitor.println(ADC_VREF);
}

void loop() {
  static unsigned long lastSample = 0, lastLog = 0;
  static bool wdgTripped = false;
  unsigned long now = millis();

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    readAll();

    bool alive = everPolled && (now - lastPollMs <= WATCHDOG_MS);
    if (mode != M_IDLE && !alive) {
      // Python side not polling: never drive motors blindly
      if (!wdgTripped) { Monitor.println("[WDG] no poll from Python, motors held off"); wdgTripped = true; }
      motorsStop();
      if (mode == M_TEST) finishTest(T_ABORTED);
    } else {
      if (wdgTripped && alive) { Monitor.println("[WDG] Python polling again"); wdgTripped = false; }
      if (mode == M_FOLLOW) followTick();
      else if (mode == M_TEST) testTick();
    }
  }

  if (now - lastLog >= LOG_PERIOD_MS) {
    lastLog = now;
    Monitor.print("[SENS] raw=");
    for (int i = 0; i < NUM_SENSORS; i++) { Monitor.print(sensors[i]); Monitor.print(' '); }
    Monitor.print("| line="); Monitor.print(lineFound ? "yes" : "no");
    Monitor.print(" pos="); Monitor.print(linePos, 2);
    Monitor.print(" | motors L="); Monitor.print(cmdL); Monitor.print(" R="); Monitor.print(cmdR);
    Monitor.print(" | bat="); Monitor.print(batteryV, 2); Monitor.print("V");
    if (batteryV < 7.0f) Monitor.print(" LOW(<7.0V)");
    Monitor.print(" | mode="); Monitor.print(modeName());
    Monitor.print(" test="); Monitor.println(testName());
  }
}
