#include <Wire.h>
#include <Adafruit_VL53L0X.h>

#define SDA_PIN 21
#define SCL_PIN 22

const int LED_PIN = 13;
const int XSHUT_LEFT   = 15;
const int XSHUT_CENTER = 2;
const int XSHUT_RIGHT  = 4;

#define ADDR_LEFT   0x30
#define ADDR_CENTER 0x39
#define ADDR_RIGHT  0x32

const int M_A_IN1 = 12; 
const int M_A_IN2 = 14; 
const int M_B_IN1 = 27; 
const int M_B_IN2 = 26; 

     
const int LINE_LEFT   = 32;
const int LINE_CENTER = 33;
const int LINE_RIGHT  = 25;
 
const uint16_t attackThreshold  = 800;
const uint16_t sideThreshold    = 570;
const uint16_t maxUsefulRange   = 1200;

const float ema_alpha = 0.8;

const unsigned long loopIntervalMs = 30;
const unsigned long searchSweepInterval = 1800;
const unsigned long sweepDurationMs = 450;
const unsigned long microWobblePeriodMs = 300;

const int SPEED_FORWARD = 230;
const int SPEED_TURN = 180;
const int SPEED_REVERSE = 195;

const int PWM_FREQ = 2000;      // 1kHz
const int PWM_RES  = 8;         // 8-bit (0..255)



Adafruit_VL53L0X vlx_left  = Adafruit_VL53L0X();
Adafruit_VL53L0X vlx_center = Adafruit_VL53L0X();
Adafruit_VL53L0X vlx_right = Adafruit_VL53L0X();

float left_ema = -1, center_ema = -1, right_ema = -1;
unsigned long lastLoop = 0;
unsigned long lastSearchSweep = 0;
bool inSweep = false;
long sweepStartTime = 0;
int sweepDirection = 1;

void setup() {

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  delay(200);
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  delay(200);
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  


  Serial.begin(115200);
  delay(100);
  Serial.println("Mega Sumo with Line Sensors starting...");

  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(XSHUT_LEFT, OUTPUT);
  pinMode(XSHUT_CENTER, OUTPUT);
  pinMode(XSHUT_RIGHT, OUTPUT);

  digitalWrite(XSHUT_LEFT, LOW);
  digitalWrite(XSHUT_CENTER, LOW);
  digitalWrite(XSHUT_RIGHT, LOW);
  delay(10);

  initSingleVLX(&vlx_left, XSHUT_LEFT, ADDR_LEFT, "LEFT");
  initSingleVLX(&vlx_center, XSHUT_CENTER, ADDR_CENTER, "CENTER");
  initSingleVLX(&vlx_right, XSHUT_RIGHT, ADDR_RIGHT, "RIGHT");

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

/* Initialize VL53L0X */
void initSingleVLX(Adafruit_VL53L0X *dev, int xshut_pin, uint8_t newAddr, const char* name) {
  digitalWrite(xshut_pin, LOW);
  delay(5);
  digitalWrite(xshut_pin, HIGH);
  delay(10);

  if (!dev->begin()) {
    Serial.print("Failed to boot ");
    Serial.println(name);
    return;
  }
  if (!dev->setAddress(newAddr)) {
    Serial.print("Failed setAddress for ");
    Serial.println(name);
  } else {
    Serial.print(name); Serial.print(" set to 0x"); Serial.println(newAddr, HEX);
  }
}

/* Sensor read + EMA */
uint16_t readVLX(Adafruit_VL53L0X &sensor, float &ema) {
  VL53L0X_RangingMeasurementData_t measure;
  sensor.rangingTest(&measure, false);
  if (measure.RangeStatus != 4) {
    uint16_t dist = measure.RangeMilliMeter;
    if (dist == 0) dist = maxUsefulRange + 1;
    if (dist > maxUsefulRange) dist = maxUsefulRange + 1;
    if (ema < 0) ema = dist;
    else ema = ema * (1.0 - ema_alpha) + (float)dist * ema_alpha;
    return dist;
  } else {
    if (ema < 0) ema = maxUsefulRange + 1;
    else ema = ema * (1.0 - ema_alpha) + (float)(maxUsefulRange + 1) * ema_alpha;
    return maxUsefulRange + 1;
  }
}

void setMotorA(int pwm, bool forward) {
  pwm = constrain(pwm, 0, 255);
  if (forward) {
    ledcWrite(M_A_IN1, pwm); // Send PWM to IN1
    ledcWrite(M_A_IN2, 0);   // Send LOW to IN2
  } else {
    ledcWrite(M_A_IN1, 0);   // Send LOW to IN1
    ledcWrite(M_A_IN2, pwm); // Send PWM to IN2
  }
}

void setMotorB(int pwm, bool forward) {
  pwm = constrain(pwm, 0, 255);
  if (forward) {
    ledcWrite(M_B_IN1, pwm); // Send PWM to IN1
    ledcWrite(M_B_IN2, 0);   // Send LOW to IN2
  } else {
    ledcWrite(M_B_IN1, 0);   // Send LOW to IN1
    ledcWrite(M_B_IN2, pwm); // Send PWM to IN2
  }
}

void stopMotors() {
  ledcWrite(M_A_IN1, 0);
  ledcWrite(M_A_IN2, 0);
  ledcWrite(M_B_IN1, 0);
  ledcWrite(M_B_IN2, 0);
}

void forward(int speed) { setMotorA(speed, true); setMotorB(speed, true); }
void reverse_(int speed) { setMotorA(speed, false); setMotorB(speed, false); }
void turnLeft(int speed) { setMotorA(speed/2, true); setMotorB(speed, true); }
void turnRight(int speed){ setMotorA(speed, true); setMotorB(speed/2, true); }
void spinLeft(int speed) { setMotorA(speed, false); setMotorB(speed, true); }
void spinRight(int speed){ setMotorA(speed, true); setMotorB(speed, false); }

void loop() {
  unsigned long now = millis();
  if (now - lastLoop < loopIntervalMs) return;
  lastLoop = now;

  int lineL = digitalRead(LINE_LEFT);
  int lineC = digitalRead(LINE_CENTER);
  int lineR = digitalRead(LINE_RIGHT);

  // --- Serial print line sensors ---
  Serial.print("  IR L: "); Serial.print(lineL);
  Serial.print(" | IR C: "); Serial.print(lineC);
  Serial.print(" | IR R: "); Serial.println(lineR);

  // ----------------- LINE SENSOR BEHAVIOR -----------------
  // LOW = detects white line
  if (lineL == LOW || lineC == LOW || lineR == LOW) {

    // Case 1: all three sensors -> full edge detected
    if (lineL == LOW && lineC == LOW && lineR == LOW) {
      reverse_(SPEED_REVERSE);
      delay(400);
      spinRight(SPEED_TURN);
      delay(300);
    }

    // Case 2: center only
    else if (lineC == LOW && lineL == HIGH && lineR == HIGH) {
      reverse_(SPEED_REVERSE);
      delay(300);
      // random small turn to left or right
      if (random(0, 2) == 0) spinLeft(SPEED_TURN);
      else spinRight(SPEED_TURN);
      delay(200);
    }

    // Case 3: left only
    else if (lineL == LOW && lineC == HIGH && lineR == HIGH) {
      reverse_(SPEED_REVERSE);
      delay(200);
      spinRight(SPEED_TURN);
      delay(250);
    }

    // Case 4: right only
    else if (lineR == LOW && lineC == HIGH && lineL == HIGH) {
      reverse_(SPEED_REVERSE);
      delay(200);
      spinLeft(SPEED_TURN);
      delay(250);
    }

    // Case 5: left + center
    else if (lineL == LOW && lineC == LOW) {
      reverse_(SPEED_REVERSE);
      delay(300);
      spinRight(SPEED_TURN);
      delay(250);
    }

    // Case 6: right + center
    else if (lineR == LOW && lineC == LOW) {
      reverse_(SPEED_REVERSE);
      delay(300);
      spinLeft(SPEED_TURN);
      delay(250);
    }

    stopMotors();
    return; // skip rest of loop
  }

  // ----------------- DISTANCE SENSOR BEHAVIOR -----------------
  int distLeft   = readVLX(vlx_left, left_ema);
  int distCenter = readVLX(vlx_center, center_ema);
  int distRight  = readVLX(vlx_right, right_ema);

  // --- Serial print VL53L0X distances ---
  Serial.print("                                       Dist L:"); Serial.print(distLeft/10);
  Serial.print("    | C:"); Serial.print(distCenter / 10);
  Serial.print("    | R:"); Serial.print(distRight / 10);
  Serial.println(" cm");

  // Behavior (unchanged)
  int minDist = maxUsefulRange + 1;
  String minSensor = "NONE";

  if (distLeft   < minDist && distLeft   <= sideThreshold) { minDist = distLeft;   minSensor = "LEFT"; }
  if (distCenter < minDist && distCenter <= attackThreshold) { minDist = distCenter; minSensor = "CENTER"; }
  if (distRight  < minDist && distRight  <= sideThreshold) { minDist = distRight;  minSensor = "RIGHT"; }

  if (minSensor == "CENTER") forward(SPEED_FORWARD);
  else if (minSensor == "LEFT") { spinLeft(SPEED_TURN); delay(120); forward(SPEED_FORWARD); }
  else if (minSensor == "RIGHT"){ spinRight(SPEED_TURN); delay(120); forward(SPEED_FORWARD); }
  else {
    unsigned long sinceLastSearch = now - lastSearchSweep;
    unsigned long wobblePhase = now % microWobblePeriodMs;

    if (wobblePhase < microWobblePeriodMs / 2) {
      setMotorA(SPEED_FORWARD - 20, true);
      setMotorB(SPEED_FORWARD, true);
    } else {
      setMotorA(SPEED_FORWARD, true);
      setMotorB(SPEED_FORWARD - 20, true);
    }

    if (sinceLastSearch >= searchSweepInterval) {
      lastSearchSweep = now;
      sweepStartTime = now;
      inSweep = true;
      sweepDirection = (sweepDirection > 0) ? -1 : 1;
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
}
