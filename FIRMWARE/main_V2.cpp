/* ESP32 Mega-Sumo: 3x VL53L0X + 3x Line Sensors + Motor Control */

#include <Wire.h>
#include <Adafruit_VL53L0X.h>

/* -------- CONFIG: pins -------- */
#define SDA_PIN 21
#define SCL_PIN 22

const int LED_PIN = 13;
const int XSHUT_LEFT   = 15;
const int XSHUT_CENTER = 2;
const int XSHUT_RIGHT  = 4;

bool escapeMode = false;
unsigned long escapeStart = 0;
const unsigned long ESCAPE_TIME = 400;
const unsigned long ESCAPE_REVERSE_TIME = 350;

#define ADDR_LEFT   0x30
#define ADDR_CENTER 0x31
#define ADDR_RIGHT  0x32

// Motors
const int M_A_IN1 = 12;
const int M_A_IN2 = 14;
const int M_B_IN1 = 27;
const int M_B_IN2 = 26;

// Line sensors
const int LINE_LEFT   = 32;
const int LINE_CENTER = 33;
const int LINE_RIGHT  = 25;

/* -------- PARAMETERS -------- */
const uint16_t attackThreshold  = 400;
const uint16_t sideThreshold    = 500;
const uint16_t maxUsefulRange   = 1000;

const float ema_alpha = 0.4;

const unsigned long loopIntervalMs = 30;
const unsigned long searchSweepInterval = 1800;
const unsigned long sweepDurationMs = 450;
const unsigned long microWobblePeriodMs = 300;

const int SPEED_FORWARD = 205;     // normal search speed
const int SPEED_ATTACK  = 255;     // ★ AGGRESSIVE ATTACK SPEED
const int SPEED_TURN = 185;
const int SPEED_REVERSE = 200;

/* -------- PWM CONFIG -------- */
const int PWM_FREQ = 1000;
const int PWM_RES  = 8;

/* -------- END CONFIG -------- */

Adafruit_VL53L0X vlx_left;
Adafruit_VL53L0X vlx_center;
Adafruit_VL53L0X vlx_right;

float left_ema = -1, center_ema = -1, right_ema = -1;
unsigned long lastLoop = 0;
unsigned long lastSearchSweep = 0;
bool inSweep = false;
unsigned long sweepStartTime = 0;
int sweepDirection = 1;

/* -------- MOTOR HELPERS -------- */
void setMotorA(int pwm, bool forward) {
    pwm = constrain(pwm, 0, 255);
    if (forward) { ledcWrite(M_A_IN1, pwm); ledcWrite(M_A_IN2, 0); }
    else         { ledcWrite(M_A_IN1, 0);   ledcWrite(M_A_IN2, pwm); }
}

void setMotorB(int pwm, bool forward) {
    pwm = constrain(pwm, 0, 255);
    if (forward) { ledcWrite(M_B_IN1, pwm); ledcWrite(M_B_IN2, 0); }
    else         { ledcWrite(M_B_IN1, 0);   ledcWrite(M_B_IN2, pwm); }
}

void stopMotors() {
    ledcWrite(M_A_IN1, 0); ledcWrite(M_A_IN2, 0);
    ledcWrite(M_B_IN1, 0); ledcWrite(M_B_IN2, 0);
}

void forward(int s)    { setMotorA(s, true); setMotorB(s, true); }
void reverse_(int s)   { setMotorA(s, false); setMotorB(s, false); }
void spinLeft(int s)   { setMotorA(s, false); setMotorB(s, true); }
void spinRight(int s)  { setMotorA(s, true);  setMotorB(s, false); }

/* -------- ESCAPE HELPER -------- */
void triggerEscape() {
    escapeMode = true;
    escapeStart = millis();
    reverse_(SPEED_REVERSE);
}

/* Initialize VL53L0X */
void initSingleVLX(Adafruit_VL53L0X *dev, int xshut_pin, uint8_t newAddr, const char* name) {
    digitalWrite(xshut_pin, LOW); delay(5);
    digitalWrite(xshut_pin, HIGH); delay(10);

    if (!dev->begin()) {
        Serial.print("Failed to boot "); Serial.println(name);
        return;
    }

    if (!dev->setAddress(newAddr)) {
        Serial.print("Failed setAddress for "); Serial.println(name);
    } else {
        Serial.print(name); Serial.print(" set to 0x"); Serial.println(newAddr, HEX);
    }
}

/* Sensor read + EMA */
uint16_t readVLX(Adafruit_VL53L0X &sensor, float &ema) {
    VL53L0X_RangingMeasurementData_t m;
    sensor.rangingTest(&m, false);

    uint16_t d = maxUsefulRange + 1;
    if (m.RangeStatus != 4) {
        d = m.RangeMilliMeter;
        if (d == 0 || d > maxUsefulRange) d = maxUsefulRange + 1;
    }

    if (ema < 0) ema = d;
    else ema = ema * (1 - ema_alpha) + d * ema_alpha;

    return d;
}

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); delay(700);
    digitalWrite(LED_PIN, LOW);  delay(700);
    digitalWrite(LED_PIN, HIGH); delay(700);
    digitalWrite(LED_PIN, LOW);  delay(700);
    digitalWrite(LED_PIN, HIGH); delay(700);
    digitalWrite(LED_PIN, LOW);

    pinMode(XSHUT_LEFT,   OUTPUT);
    pinMode(XSHUT_CENTER, OUTPUT);
    pinMode(XSHUT_RIGHT,  OUTPUT);

    digitalWrite(XSHUT_LEFT, LOW);
    digitalWrite(XSHUT_CENTER, LOW);
    digitalWrite(XSHUT_RIGHT, LOW);
    delay(10);

    initSingleVLX(&vlx_left,   XSHUT_LEFT,   ADDR_LEFT,   "LEFT");
    initSingleVLX(&vlx_center, XSHUT_CENTER, ADDR_CENTER, "CENTER");
    initSingleVLX(&vlx_right,  XSHUT_RIGHT,  ADDR_RIGHT,  "RIGHT");

    ledcAttach(M_A_IN1, PWM_FREQ, PWM_RES);
    ledcAttach(M_A_IN2, PWM_FREQ, PWM_RES);
    ledcAttach(M_B_IN1, PWM_FREQ, PWM_RES);
    ledcAttach(M_B_IN2, PWM_FREQ, PWM_RES);

    pinMode(LINE_LEFT, INPUT);
    pinMode(LINE_CENTER, INPUT);
    pinMode(LINE_RIGHT, INPUT);

    stopMotors();
    lastLoop = millis();
    lastSearchSweep = millis();
}

/* -------- MAIN LOOP -------- */
void loop() {

    unsigned long now = millis();
    if (now - lastLoop < loopIntervalMs) return;
    lastLoop = now;

    /* ESCAPE MODE */
    if (escapeMode) {
        unsigned long elapsed = now - escapeStart;
        if (elapsed < ESCAPE_REVERSE_TIME) {
            reverse_(SPEED_REVERSE);
            return;
        } else if (elapsed < ESCAPE_REVERSE_TIME + ESCAPE_TIME) {
            spinRight(SPEED_TURN);
            return;
        } else {
            escapeMode = false;
            stopMotors();
            delay(40);
            return;
        }
    }

    /* LINE SENSORS */
    int lineL = digitalRead(LINE_LEFT);
    int lineC = digitalRead(LINE_CENTER);
    int lineR = digitalRead(LINE_RIGHT);

    if (lineL == LOW || lineC == LOW || lineR == LOW) {
        triggerEscape();
        return;
    }

    /* DISTANCE SENSORS */
    int dL = readVLX(vlx_left, left_ema);
    int dC = readVLX(vlx_center, center_ema);
    int dR = readVLX(vlx_right, right_ema);

    int minDist = maxUsefulRange + 1;
    String best = "NONE";

    if (dL < minDist && dL <= sideThreshold)   { minDist = dL; best = "LEFT"; }
    if (dC < minDist && dC <= attackThreshold) { minDist = dC; best = "CENTER"; }
    if (dR < minDist && dR <= sideThreshold)   { minDist = dR; best = "RIGHT"; }

    /* ======= AGGRESSIVE ATTACK ======= */
    if (best == "CENTER") {
        forward(SPEED_ATTACK);   // ★ FULL POWER ATTACK
        return;
    }
    else if (best == "LEFT") {
        spinLeft(SPEED_TURN);
        delay(90);
        forward(SPEED_ATTACK);   // ★ FULL POWER ATTACK
        return;
    }
    else if (best == "RIGHT") {
        spinRight(SPEED_TURN);
        delay(90);
        forward(SPEED_ATTACK);   // ★ FULL POWER ATTACK
        return;
    }

    /* ======= SEARCH MODE (NORMAL SPEED) ======= */
    unsigned long wobble = now % microWobblePeriodMs;

    if (wobble < microWobblePeriodMs/2) {
        setMotorA(SPEED_FORWARD-20, true);
        setMotorB(SPEED_FORWARD, true);
    } else {
        setMotorA(SPEED_FORWARD, true);
        setMotorB(SPEED_FORWARD-20, true);
    }

    unsigned long sinceSweep = now - lastSearchSweep;
    if (sinceSweep >= searchSweepInterval) {
        lastSearchSweep = now;
        sweepStartTime = now;
        inSweep = true;
        sweepDirection = -sweepDirection;
    }

    if (inSweep) {
        unsigned long t = now - sweepStartTime;
        if (t < sweepDurationMs) {
            if (sweepDirection > 0) spinRight(SPEED_TURN);
            else spinLeft(SPEED_TURN);
        } else {
            inSweep = false;
            stopMotors();
        }
    }
}
