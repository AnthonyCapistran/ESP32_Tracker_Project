#include <AccelStepper.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- BLE CONFIGURATION ---
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
String bleInputString = "";

// Standard UUIDs for Nordic UART Service (commonly used for BLE terminals)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Server callbacks to track connection status
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println(">> Phone connected via BLE!");
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println(">> Phone disconnected. Advertising restarted.");
      pServer->getAdvertising()->start(); // Restart advertising so you can reconnect
    }
};

// Callback to handle incoming data from the phone app
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // FIX: Changed std::string to Arduino String to match ESP32 Core v3+
      String rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i++) {
          char c = rxValue[i];
          if (c == '\n' || c == '\r') {
            if (bleInputString.length() > 0) {
              handleBLECommand(bleInputString);
              bleInputString = ""; 
            }
          } else {
            bleInputString += c;
          }
        }
      }
    }
    
    // Helper inside the callback to map variables immediately
    void handleBLECommand(String cmd) {
      extern long userPanTarget;
      extern long userTiltTarget;
      extern bool isLoopingMode;
      extern bool loopDirectionToggle;
      extern AccelStepper panStepper;
      extern AccelStepper tiltStepper;

      cmd.trim();
      if (cmd.equalsIgnoreCase("LOOP")) {
        isLoopingMode = true;
        loopDirectionToggle = true;
        panStepper.moveTo(userPanTarget);
        tiltStepper.moveTo(userTiltTarget);
        Serial.println("BLE: LOOP ACTIVE");
      } 
      else if (cmd.startsWith("P") || cmd.startsWith("p")) {
        isLoopingMode = false; 
        userPanTarget = cmd.substring(1).toInt();
        panStepper.moveTo(userPanTarget);
        Serial.print("BLE: Pan updated to "); Serial.println(userPanTarget);
      } 
      else if (cmd.startsWith("T") || cmd.startsWith("t")) {
        isLoopingMode = false; 
        userTiltTarget = cmd.substring(1).toInt();
        tiltStepper.moveTo(userTiltTarget);
        Serial.print("BLE: Tilt updated to "); Serial.println(userTiltTarget);
      }
    }
};

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

// Global operational variables shared with the BLE callback
long userPanTarget = 0;
long userTiltTarget = 0;
bool isLoopingMode = false;
bool loopDirectionToggle = true; 

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

  // --- START BLE SYSTEM IMMEDIATELY AT POWER UP ---
  BLEDevice::init("ESP32_BLE_PanTilt");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the UART Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a TX Characteristic (for sending data to the phone)
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());

  // Create an RX Characteristic (for receiving data from the phone)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service and start advertising
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE Wireless Service Active! Broadcasting as: ESP32_BLE_PanTilt");

  // --- HARDWARE CONFIGURATION ---
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