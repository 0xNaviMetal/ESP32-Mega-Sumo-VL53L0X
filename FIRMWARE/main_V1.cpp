/* ---------------------------------------------------------
      HYBRID SUMO – OPTIMIZED VERSION (FAST 15–40 cm)
   --------------------------------------------------------- */

#include <Wire.h>
#include <Adafruit_VL53L0X.h>

/* ---------- PINS ---------- */
#define SDA_PIN 21
#define SCL_PIN 22

const uint8_t LED_PIN = 13;

const uint8_t XSHUT_LEFT   = 15;
const uint8_t XSHUT_CENTER = 2;
const uint8_t XSHUT_RIGHT  = 4;

#define ADDR_LEFT   0x30
#define ADDR_CENTER 0x39
#define ADDR_RIGHT  0x32

const uint8_t M_A_IN1_PIN = 12;
const uint8_t M_A_IN2_PIN = 14;
const uint8_t M_B_IN1_PIN = 27;
const uint8_t M_B_IN2_PIN = 26;

const uint8_t CH_MA_IN1 = 0;
const uint8_t CH_MA_IN2 = 1;
const uint8_t CH_MB_IN1 = 2;
const uint8_t CH_MB_IN2 = 3;

const uint8_t LINE_LEFT   = 32;
const uint8_t LINE_CENTER = 33;
const uint8_t LINE_RIGHT  = 25;

/* ---------- THRESHOLDS (TUNE HERE) ---------- */
uint16_t ATTACK_THRESHOLD = 700;   // LOWER = more aggressive
uint16_t SIDE_THRESHOLD   = 750;   // side detection
uint16_t MAX_RANGE        = 1200;

/* ---------- SMOOTHING (TUNE HERE) ---------- */
float EMA_CENTER = 0.70;
float EMA_SIDE   = 0.55;

/* ---------- Timings ---------- */
uint16_t LOOP_MS = 18;   // faster = better detection

/* ---------- Speeds ---------- */
uint8_t SPD_SEARCH   = 130;
uint8_t SPD_SWEEP    = 180;
uint8_t SPD_ALIGN    = 170;
uint8_t SPD_ATTACK   = 240;
uint8_t SPD_TURN     = 200;
uint8_t SPD_BACK     = 240;

/* ---------- Globals ---------- */
Adafruit_VL53L0X vlxL, vlxC, vlxR;
float emaL=-1, emaC=-1, emaR=-1;

unsigned long lastLoop = 0;
unsigned long lastSweep = 0;

/* ---------- State Machine ---------- */
enum State { SEARCH, ALIGN, ATTACK, RECOVER };
State state = SEARCH;

/* ==========================================================
                      MOTOR CONTROL
   ========================================================== */
void pwmSetup() {
  ledcSetup(CH_MA_IN1, 2000, 8);
  ledcSetup(CH_MA_IN2, 2000, 8);
  ledcSetup(CH_MB_IN1, 2000, 8);
  ledcSetup(CH_MB_IN2, 2000, 8);

  ledcAttachPin(M_A_IN1_PIN, CH_MA_IN1);
  ledcAttachPin(M_A_IN2_PIN, CH_MA_IN2);

  ledcAttachPin(M_B_IN1_PIN, CH_MB_IN1);
  ledcAttachPin(M_B_IN2_PIN, CH_MB_IN2);
}

inline void motorA(int pwm, bool f){
  pwm = constrain(pwm,0,255);
  ledcWrite(f?CH_MA_IN1:CH_MA_IN2, pwm);
  ledcWrite(f?CH_MA_IN2:CH_MA_IN1, 0);
}

inline void motorB(int pwm, bool f){
  pwm = constrain(pwm,0,255);
  ledcWrite(f?CH_MB_IN1:CH_MB_IN2, pwm);
  ledcWrite(f?CH_MB_IN2:CH_MB_IN1, 0);
}

inline void forward(uint8_t s){ motorA(s,1); motorB(s,1); }
inline void reverse_(uint8_t s){ motorA(s,0); motorB(s,0); }
inline void spinL(uint8_t s){ motorA(s,0); motorB(s,1); }
inline void spinR(uint8_t s){ motorA(s,1); motorB(s,0); }
inline void stopMotors(){ motorA(0,1); motorB(0,1); }

/* ==========================================================
                      SENSOR READ (FAST)
   ========================================================== */
uint16_t readVLX(Adafruit_VL53L0X &dv, float &ema, float a) {
  VL53L0X_RangingMeasurementData_t m;
  dv.rangingTest(&m, false);

  uint16_t d = 
      (m.RangeStatus==4) ? 
      (MAX_RANGE+1) : constrain(m.RangeMilliMeter,1,MAX_RANGE+1);

  if(ema < 0) ema = d;
  else ema = ema*(1-a) + d*a;

  return d;
}

/* ==========================================================
                      INIT VLX
   ========================================================== */
void initVLX(Adafruit_VL53L0X *dev, uint8_t pin, uint8_t addr){
  digitalWrite(pin,LOW); delay(5);
  digitalWrite(pin,HIGH); delay(10);

  dev->begin();
  dev->setAddress(addr);

  // High-speed mode
  dev->setMeasurementTimingBudgetMicroSeconds(20000);
}

/* ==========================================================
                          SETUP
   ========================================================== */
void setup(){
  Serial.begin(115200);

  Wire.begin(SDA_PIN,SCL_PIN);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); delay(80);
  digitalWrite(LED_PIN, LOW);  delay(80);
  digitalWrite(LED_PIN, HIGH); delay(80);
  digitalWrite(LED_PIN, HIGH); delay(80);
  digitalWrite(LED_PIN, LOW);  delay(80);
  digitalWrite(LED_PIN, HIGH); delay(80);
  
  pinMode(XSHUT_LEFT,OUTPUT);
  pinMode(XSHUT_CENTER,OUTPUT);
  pinMode(XSHUT_RIGHT,OUTPUT);

  initVLX(&vlxL,XSHUT_LEFT,ADDR_LEFT);
  initVLX(&vlxC,XSHUT_CENTER,ADDR_CENTER);
  initVLX(&vlxR,XSHUT_RIGHT,ADDR_RIGHT);

  pwmSetup();

  pinMode(LINE_LEFT,INPUT);
  pinMode(LINE_CENTER,INPUT);
  pinMode(LINE_RIGHT,INPUT);

  stopMotors();
}

/* ==========================================================
                     TARGET SELECTION
   ========================================================== */
String target(uint16_t L,uint16_t C,uint16_t R){
  if(C <= ATTACK_THRESHOLD) return "C";
  if(L <= SIDE_THRESHOLD)   return "L";
  if(R <= SIDE_THRESHOLD)   return "R";
  return "NONE";
}

/* ==========================================================
                          MAIN LOOP
   ========================================================== */
void loop(){
  unsigned long now = millis();
  if(now-lastLoop < LOOP_MS) return;
  lastLoop = now;

 if(!digitalRead(LINE_LEFT) || !digitalRead(LINE_CENTER) || !digitalRead(LINE_RIGHT)){
    reverse_(SPD_BACK);
    delay(120);

    unsigned long spinStart = millis();
    const unsigned long maxSpinWait = 500; // ms max to wait for sensors to clear
    while (millis() - spinStart < maxSpinWait) {
      spinR(SPD_TURN); // keep spinning right
      if (digitalRead(LINE_LEFT) && digitalRead(LINE_CENTER) && digitalRead(LINE_RIGHT)) break;
      delay(10);
    }
    const unsigned long extraSpinMs = 400;
    spinR(SPD_TURN);
    delay(extraSpinMs);

    stopMotors();
    state = RECOVER;
    return;
  }

  uint16_t dL = readVLX(vlxL,emaL,EMA_SIDE);
  uint16_t dC = readVLX(vlxC,emaC,EMA_CENTER);
  uint16_t dR = readVLX(vlxR,emaR,EMA_SIDE);

  String tgt = target(dL,dC,dR);

  /* ===================================================
                      STATE MACHINE
     =================================================== */
  switch(state){

    /* ---------------- SEARCH ---------------- */
    case SEARCH:
      forward(SPD_SEARCH);

      // Spiral: slow turning to scan big area
      if(now - lastSweep > 950){
        spinL(SPD_SWEEP);
        delay(160);
        lastSweep = now;
      }

      if(tgt!="NONE") state=ALIGN;
      break;

    /* ---------------- ALIGN ---------------- */
    case ALIGN:
      if(tgt=="C"){
        state=ATTACK;
      }
      else if(tgt=="L"){
        spinL(SPD_ALIGN);
      }
      else if(tgt=="R"){
        spinR(SPD_ALIGN);
      }
      else state=SEARCH;
      break;

    /* ---------------- ATTACK ---------------- */
    case ATTACK:
      if(dC < ATTACK_THRESHOLD)
        forward(SPD_ATTACK);
      else
        forward(SPD_ATTACK-40);

      if(dC > ATTACK_THRESHOLD + 150){
        stopMotors();
        state = SEARCH;
      }
      break;

    /* ---------------- RECOVER ---------------- */
    case RECOVER:
      state = SEARCH;
      break;
  }
}
