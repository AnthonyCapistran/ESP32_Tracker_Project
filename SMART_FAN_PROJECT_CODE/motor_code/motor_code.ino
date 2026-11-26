#include <AccelStepper.h>
#include "BluetoothSerial.h" // Built-in ESP32 Bluetooth library

// Check if Bluetooth is properly configured in the Arduino IDE
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run 'make menuconfig' to enable it
#endif

BluetoothSerial SerialBT; // Create the Bluetooth object

// --- PIN DEFINITIONS ---
const int PAN_EN_PIN   = 4;   // D4
const int PAN_STEP_PIN = 2;   // D2
const int PAN_DIR_PIN  = 15;  // D15

const int TILT_EN_PIN   = 23;  // D23
const int TILT_STEP_PIN = 22;  // D22
const int TILT_DIR_PIN  = 21;  // D21

const int POT_PIN        = 34;  // D34
const int PAN_HALL_PIN   = 25;  // D25
const int TILT_HALL_PIN  = 33;  // D33
const int BLUE_LED_PIN   = 2;   // D2 

#define MOTOR_INTERFACE_TYPE 1
AccelStepper panStepper(MOTOR_INTERFACE_TYPE, PAN_STEP_PIN, PAN_DIR_PIN);
AccelStepper tiltStepper(MOTOR_INTERFACE_TYPE, TILT_STEP_PIN, TILT_DIR_PIN);

// --- RUNTIME VARIABLES ---
long userPanTarget = 0;
long userTiltTarget = 0;
bool isLoopingMode = false;
bool loopDirectionToggle = true; 

// --- HOMING FUNCTION ---
bool executeHomingRoutine() {
  Serial.println("Starting Coordinated Homing Sequence...");
  int clearanceAttempts = 0;
  while ((digitalRead(PAN_HALL_PIN) == LOW || digitalRead(TILT_HALL_PIN) == LOW) && clearanceAttempts < 500) {
    if (digitalRead(PAN_HALL_PIN) == LOW) panStepper.move(10);  
    if (digitalRead(TILT_HALL_PIN) == LOW) tiltStepper.move(10);
    panStepper.run();
    tiltStepper.run();
    delay(2);
    clearanceAttempts++;
  }

  panStepper.setMaxSpeed(150.0);
  tiltStepper.setMaxSpeed(150.0);
  panStepper.moveTo(-5000); 
  tiltStepper.moveTo(-2000);

  bool panHomed = false;
  bool tiltHomed = false;

  while ((!panHomed || !tiltHomed) && (panStepper.distanceToGo() != 0 || tiltStepper.distanceToGo() != 0)) {
    if (!panHomed && digitalRead(PAN_HALL_PIN) == LOW) {
      panStepper.stop();
      panStepper.setCurrentPosition(0);
      panHomed = true;
    }
    if (!tiltHomed && digitalRead(TILT_HALL_PIN) == LOW) {
      tiltStepper.stop();
      tiltStepper.setCurrentPosition(0);
      tiltHomed = true;
    }
    if (!panHomed) panStepper.run();
    if (!tiltHomed) tiltStepper.run();
  }
  return (panHomed && tiltHomed);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Bluetooth with a custom broadcast name
  SerialBT.begin("ESP32_PanTilt_Mount"); 
  Serial.println("Bluetooth Device Ready! Pair with: ESP32_PanTilt_Mount");

  pinMode(PAN_EN_PIN, OUTPUT);
  pinMode(TILT_EN_PIN, OUTPUT);
  digitalWrite(PAN_EN_PIN, LOW); 
  digitalWrite(TILT_EN_PIN, LOW);

  pinMode(PAN_HALL_PIN, INPUT_PULLUP);
  pinMode(TILT_HALL_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED_PIN, OUTPUT);

  panStepper.setAcceleration(100.0);
  tiltStepper.setAcceleration(100.0);

  executeHomingRoutine();
}

void loop() {
  // --- POTENTIOMETER SPEED CONTROL ---
  int potValue = analogRead(POT_PIN);
  float motorSpeed = map(potValue, 0, 4095, 0, 400);
  if (motorSpeed < 15) motorSpeed = 15; 

  panStepper.setMaxSpeed(motorSpeed);
  tiltStepper.setMaxSpeed(motorSpeed);

  // --- PARSE INCOMING DATA (Check USB Serial OR Bluetooth Serial) ---
  String inputString = "";
  if (Serial.available() > 0) {
    inputString = Serial
Filter your search...
Type:

All



103104105106107108109110111112113114115116117118119120121122123124125126127
#include <AccelStepper.h>
#include "BluetoothSerial.h" // Built-in ESP32 Bluetooth library

// Check if Bluetooth is properly configured in the Arduino IDE
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run 'make menuconfig' to enable it
#endif

BluetoothSerial SerialBT; // Create the Bluetooth object


.readStringUntil('\n');
  } else if (SerialBT.available() > 0) {
    inputString = SerialBT.readStringUntil('\n');
  }

  if (inputString != "") {
    inputString.trim();
    
    if (inputString.equalsIgnoreCase("LOOP")) {
      isLoopingMode = true;
      loopDirectionToggle = true;
      SerialBT.println(">> LOOP MODE ACTIVE.");
      panStepper.moveTo(userPanTarget);
      tiltStepper.moveTo(userTiltTarget);
    } 
    else if (inputString.startsWith("P") || inputString.startsWith("p")) {
      isLoopingMode = false; 
      userPanTarget = inputString.substring(1).toInt();
      panStepper.moveTo(userPanTarget);
      SerialBT.print(">> Pan Updated: "); SerialBT.println(userPanTarget);
    } 
    else if (inputString.startsWith("T") || inputString.startsWith("t")) {
      isLoopingMode = false; 
      userTiltTarget = inputString.substring(1).toInt();
      tiltStepper.moveTo(userTiltTarget);
      SerialBT.print(">> Tilt Updated: "); SerialBT.println(userTiltTarget);
    }
  }

  // --- LOOPING MOTION LOGIC ---
  if (isLoopingMode && panStepper.distanceToGo() == 0 && tiltStepper.distanceToGo() == 0) {
    if (loopDirectionToggle) {
      panStepper.moveTo(0);
      tiltStepper.moveTo(0);
      loopDirectionToggle = false;
    } else {
      panStepper.moveTo(userPanTarget);
      tiltStepper.moveTo(userTiltTarget);
      loopDirectionToggle = true;
    }
  }

  panStepper.run();
  tiltStepper.run();
}