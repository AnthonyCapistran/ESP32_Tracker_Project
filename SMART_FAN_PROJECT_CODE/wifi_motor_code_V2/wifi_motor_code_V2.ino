#include <WiFi.h>
#include <WiFiUdp.h>
#include <AccelStepper.h>

// PIN DEFINITIONS
const int PAN_STEP = 2;  const int PAN_DIR = 15; const int PAN_EN = 4;
const int TILT_STEP = 22; const int TILT_DIR = 21; const int TILT_EN = 23;
const int POT_PIN = 34;

WiFiUDP udp;
AccelStepper pan(1, PAN_STEP, PAN_DIR);
AccelStepper tilt(1, TILT_STEP, TILT_DIR);

// Potentiometer smoothing
const int POT_SAMPLES = 10;
int potReadings[POT_SAMPLES];
int potIndex = 0;
long potSum = 0;

float lastMaxSpeed = -1;
int lastMoveX = 0;
int lastMoveY = 0;

bool connectWiFi() {
  WiFi.disconnect();
  delay(100);
  WiFi.begin("ATTUHb9tnD_EXT", "2146292161E");

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 20000) {
      return false;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting...");

  pinMode(PAN_EN, OUTPUT); pinMode(TILT_EN, OUTPUT);
  digitalWrite(PAN_EN, LOW); digitalWrite(TILT_EN, LOW);
  Serial.println("Pins set.");

  // Pre-fill pot buffer
  for (int i = 0; i < POT_SAMPLES; i++) {
    potReadings[i] = analogRead(POT_PIN);
    potSum += potReadings[i];
  }
  Serial.println("Pot buffer filled.");

  // Try connecting up to 3 times, 20s each, since signal is weak
  bool connected = false;
  for (int attempt = 1; attempt <= 3 && !connected; attempt++) {
    Serial.print("Connecting to WiFi (attempt ");
    Serial.print(attempt);
    Serial.println(")...");
    connected = connectWiFi();
  }

  if (connected) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    // For reference: -50 to -60 = great, -70 = ok, -80 or weaker = poor/unreliable
  } else {
    Serial.println("\nWiFi FAILED after 3 attempts. Continuing anyway (no UDP control).");
  }

  udp.begin(4210);
  Serial.println("UDP started.");

  pan.setAcceleration(2000);
  tilt.setAcceleration(2000);

  pan.setMaxSpeed(1000);
  tilt.setMaxSpeed(1000);
  lastMaxSpeed = 1000;

  Serial.println("System Ready");
}

void loop() {
  // 1. POTENTIOMETER — clockwise = faster, counter-clockwise = slower, full CCW = stop
  potSum -= potReadings[potIndex];
  potReadings[potIndex] = analogRead(POT_PIN);
  potSum += potReadings[potIndex];
  potIndex = (potIndex + 1) % POT_SAMPLES;

  int rawPot = potSum / POT_SAMPLES;
  int smoothedPot = 4095 - rawPot;  // inverted so clockwise = higher value

  float maxSpeed;
  if (smoothedPot < 150) {
    maxSpeed = 0;
  } else {
    maxSpeed = (float)(smoothedPot - 150) / (4095.0f - 150.0f) * (10000.0f - 500.0f) + 500.0f;
  }

  if (abs(maxSpeed - lastMaxSpeed) > 50) {
    pan.setMaxSpeed(maxSpeed);
    tilt.setMaxSpeed(maxSpeed);
    lastMaxSpeed = maxSpeed;

    if (maxSpeed > 0) {
      if (abs(lastMoveX) > 5) pan.move(lastMoveX);
      if (abs(lastMoveY) > 5) tilt.move(lastMoveY);
    } else {
      pan.stop();
      tilt.stop();
      lastMoveX = 0;
      lastMoveY = 0;
    }

    Serial.print("Speed updated: "); Serial.println(maxSpeed);
  }

  // 2. UDP PACKET HANDLING
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char buf[32];
    udp.read(buf, packetSize);
    buf[packetSize] = '\0';
    String data = String(buf);
    int comma = data.indexOf(',');

    if (comma > 0) {
      int x = data.substring(0, comma).toInt();
      int y = data.substring(comma + 1).toInt();

      int moveX = constrain((x / 10) * 4, -400, 400);
      int moveY = constrain((y / 10) * 4, -400, 400);

      if (abs(moveX) > 5) { pan.move(moveX);  lastMoveX = moveX; }
      if (abs(moveY) > 5) { tilt.move(moveY); lastMoveY = moveY; }
    }
  }

  pan.run();
  tilt.run();
}