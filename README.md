
# Mega Sumo Robot v1 with ESP32 and VL53L0X

## Overview

This is the version 1 Mega Sumo robot, designed and built for Sumo robotics competitions.
- **Awards:** 3rd place in a mega sumo robots competition!
- **Sensors:** Three VL53L0X distance sensors (front, left, right), three QRE1113IR line sensors (bottom).
- **Motors:** Two JGA-25 DC motors, driven by an MC33886 dual motor driver.
- **Controller:** ESP32.
- **Weight:** 1.4 kg (pushes up to 1.9 kg).
- **Features:** Advanced line detection and attack logic.

---

## Table of Contents
- [Overview](#overview)
- [Photos](#photos)
- [Hardware List](#hardware-list)
- [Pinout](#pinout)
- [Electrical Schematic](#electrical-schematic)
- [Assembly Instructions](#assembly-instructions)
- [Code](#code)
- [How it Works](#how-it-works)
- [Competition Results](#competition-results)
- [License](#license)

---

## Photos

> **Add photos here!**
>
> - ![Robot front view](docs/images/robot_front.jpg)
> - ![Sensor placement](docs/images/sensors.jpg)
> - ![Sumo in competition](docs/images/in_competition.jpg)

---

## Hardware List

| Component              | Quantity | Notes                           |
|------------------------|----------|---------------------------------|
| ESP32 Dev Board        |    1     | Main controller                 |
| VL53L0X ToF Sensor     |    3     | Distance sensing (front array)  |
| QRE1113GR Line Sensor  |    3     | Bottom, line detection          |
| JGA-25 DC Gear Motor   |    2     | Left and right drive            |
| MC33886 Motor Driver   |    1     | Dual channel, motor control     |
| LiPo Battery           |    1     | (match voltage to motor/spec)   |
| Chassis & Wheels       |    1 set | Mechanical assembly             |
| Various wires, PCB     |   as needed |                               |

---

## Pinout

| Function                   | ESP32 Pin  | Description                   |
|----------------------------|------------|-------------------------------|
| VL53L0X Left XSHUT         |    15      | Power/XSHUT left sensor       |
| VL53L0X Center XSHUT       |     2      | Power/XSHUT center sensor     |
| VL53L0X Right XSHUT        |     4      | Power/XSHUT right sensor      |
| VL53L0X SDA                |    21      | I2C Data                      |
| VL53L0X SCL                |    22      | I2C Clock                     |
| QRE1113IR Line Left        |    32      | Detects line (left, bottom)   |
| QRE1113IR Line Center      |    33      | Detects line (center, bottom) |
| QRE1113IR Line Right       |    25      | Detects line (right, bottom)  |
| Motor A IN1 (PWM)          |    12      | MC33886 IN1                   |
| Motor A IN2 (PWM)          |    14      | MC33886 IN2                   |
| Motor B IN1 (PWM)          |    27      | MC33886 IN1                   |
| Motor B IN2 (PWM)          |    26      | MC33886 IN2                   |
| Status LED                 |    13      | Onboard LED                   |

#### VL53L0X I2C Addresses

| Sensor   | I2C Address |
|----------|-------------|
| Left     |   0x30      |
| Center   |   0x39      |
| Right    |   0x32      |

---

## Electrical Schematic

> **Add schematic here!**
>
> - [ ] Draw circuit schematic showing ESP32, sensors, motors, motor driver, and all connections.

---

## Assembly Instructions

1. Mount the ESP32 on the chassis.
2. Attach the three VL53L0X sensors: one in the center, and two at each front corner.
3. Fix the three QRE1113IR line sensors underneath the robot (left, center, and right positions).
4. Mount and wire up the MC33886 motor driver to the ESP32 and to both DC motors.
5. Connect battery (ensure proper voltage and protection!).
6. Double-check all wires and sensor orientation.
7. (Optional) Add protective covers for motors/sensors/wires.

---

## Code

> **Place your latest code in [main.ino](src/main.ino) or in `src/` directory.**
>
> **Below is your current main sketch (summarized for reference):**

```cpp name=src/main.ino
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

// --- Pin Definitions ---
#define SDA_PIN 21
#define SCL_PIN 22
const int LED_PIN = 13;
const int XSHUT_LEFT = 15;
const int XSHUT_CENTER = 2;
const int XSHUT_RIGHT = 4;

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

// ... (remaining code for logic, motor, and sensor control)
```
> [Full code available in `src/main.ino`](src/main.ino)

---

## How it Works

### Sensing
- **Front Distance Sensing:** 3x VL53L0X sensors detect opponent's position.
- **Line Avoidance:** 3x QRE1113IR sensors avoid going out of bounds (detect white line).

### Behavior Logic
- If a line is detected by any bottom sensor, the robot instantly reverses and turns away.
- If an opponent is detected in the center, goes forward at full power; if detected left/right, spins, then attacks.
- If no opponent detected: the robot "wobbles" and sweeps to search for targets.

### Motors and Movement
- Dual JGA-25 motors for left/right drive.
- All movements (forward, spin, wobble, reverse) handled in code for best attack and defense!

---

## Competition Results

- **Event:** [Event Name / Date Here]
- **Result:** 3rd Place — Successfully pushed robots up to 1.9 kg!

---

## License

> Specify your license here (e.g., MIT, GPL, etc.)

---

## Credits

- Built by @0xNaviMetal
- [Any collaborators, thanks, etc.]

---

> For any issues, open an [issue](https://github.com/0xNaviMetal/ESP32-Mega-Sumo-VL53L0X/issues).
