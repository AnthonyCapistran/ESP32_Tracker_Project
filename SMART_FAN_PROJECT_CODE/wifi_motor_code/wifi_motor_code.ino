#include <WiFi.h>
#include <WiFiUdp.h>
#include <AccelStepper.h>

const int PAN_STEP = 2;  const int PAN_DIR = 15; const int PAN_EN = 4;
const int TILT_STEP = 22; const int TILT_DIR = 21; const int TILT_EN = 23;
const int POT_PIN = 34;

WiFiUDP udp;
AccelStepper pan(1, PAN_STEP, PAN_DIR);
AccelStepper tilt(1, TILT_STEP, TILT_DIR);

const int POT_SAMPLES = 10;
int potReadings[POT_SAMPLES];
int potIndex = 0;
long potSum = 0;

int lastMoveX = 0;
int lastMoveY = 0;
unsigned long lastPacketTime = 0;
int packetCount = 0;

bool connectWiFi() {
  WiFi.disconnect();
  delay(100);
  WiFi.begin("ATTUHb9tnD_EXT", "2146292161E");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 20000) return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting...");

  pinMode(PAN_EN, OUTPUT); pinMode(TILT_EN, OUTPUT);
  digitalWrite(PAN_EN, LOW); digitalWrite(TILT_EN, LOW);

  for (int i = 0; i < POT_SAMPLES; i++) {
    potReadings[i] = analogRead(POT_PIN);
    potSum += potReadings[i];
  }

  bool connected = false;
  for (int attempt = 1; attempt <= 3 && !connected; attempt++) {
    Serial.print("Connecting to WiFi (attempt ");
    Serial.print(attempt);
    Serial.println(")...");
    connected = connectWiFi();
  }

  if (connected) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  } else {
    Serial.println("\nWiFi FAILED after 3 attempts. Continuing anyway.");
  }

  udp.begin(4210);
  Serial.println("UDP listening on port 4210");

  pan.setAcceleration(3000);
  tilt.setAcceleration(3000);
  pan.setMaxSpeed(5000);
  tilt.setMaxSpeed(5000);

  Serial.println("System Ready");
}

void loop() {
  // POTENTIOMETER
  potSum -= potReadings[potIndex];
  potReadings[potIndex] = analogRead(POT_PIN);
  potSum += potReadings[potIndex];
  potIndex = (potIndex + 1) % POT_SAMPLES;

  int rawPot = potSum / POT_SAMPLES;
  int smoothedPot = 4095 - rawPot;

  if (smoothedPot < 150) {
    pan.setMaxSpeed(1);
    tilt.setMaxSpeed(1);
  } else {
    float potFraction = (float)(smoothedPot - 150) / (4095.0f - 150.0f);
    float panSpeed  = potFraction * (33750.0f - 500.0f) + 500.0f;
    float tiltSpeed = potFraction * (22500.0f - 500.0f) + 500.0f;
    pan.setMaxSpeed(panSpeed);
    tilt.setMaxSpeed(tiltSpeed);
  }

  // UDP PACKET HANDLING
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char buf[32];
    udp.read(buf, packetSize);
    buf[packetSize] = '\0';
    String data = String(buf);

    packetCount++;
    if (packetCount % 30 == 0) {
      Serial.print("UDP #"); Serial.print(packetCount);
      Serial.print(" data: "); Serial.println(data);
    }
    lastPacketTime = millis();

    int comma = data.indexOf(',');
    if (comma > 0) {
      int x = data.substring(0, comma).toInt();
      int y = data.substring(comma + 1).toInt();

      int moveX = constrain((x / 10) * 4, -400, 400);
      int moveY = constrain((y / 10) * 4, -400, 400);

      // KEY FIX: removed pan.stop() and tilt.stop() entirely
      // Calling stop() every frame when inside deadzone was resetting
      // AccelStepper state and killing all motion
      // Now we simply don't issue a move when inside deadzone — motor
      // naturally decelerates to a stop via setAcceleration()
      if (abs(moveX) > 20) {
        pan.move(moveX);
        lastMoveX = moveX;
      } else {
        lastMoveX = 0;  // clear last move so it doesn't re-issue
      }

      if (abs(moveY) > 20) {
        tilt.move(moveY);
        lastMoveY = moveY;
      } else {
        lastMoveY = 0;
      }
    }
  } else {
    // No new packet — re-issue last move so motor doesn't stall mid-track
    if (abs(lastMoveX) > 20) pan.move(lastMoveX);
    if (abs(lastMoveY) > 20) tilt.move(lastMoveY);
  }

  // run() must be called every loop — no delays above this line
  pan.run();
  tilt.run();
}