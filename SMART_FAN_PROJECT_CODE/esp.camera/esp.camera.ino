#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// --- NETWORK CONFIGURATION ---
const char* ssid     = "ATTUHb9tnD_EXT";          // Your home Wi-Fi Name
const char* password = "2146292161E";              // Your home Wi-Fi Password
const char* mainEspIP = "192.168.1.50";            // IP address of your main motor-driving ESP32
const int udpPort     = 12345;                    // Port to send control targets over

WiFiUDP udp;

// --- XIAO ESP32-S3 SENSE EXACT HARDWARE PIN MAPPING ---
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define Y9_GPIO_NUM       39
#define Y8_GPIO_NUM       40
#define Y7_GPIO_NUM       41
#define Y6_GPIO_NUM       42
#define Y5_GPIO_NUM       13
#define Y4_GPIO_NUM       14
#define Y3_GPIO_NUM       47
#define Y2_GPIO_NUM       48
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     18
#define PCLK_GPIO_NUM     12
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

WiFiServer server(80);
void startCameraServer();

void setup() {
  Serial.begin(115200);
  
  // CRITICAL: Explicit hardware settling delay to give the sensor time to wake up
  for(int i = 0; i < 4; i++) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nInitializing Stabilized Hardware Configuration...");

  // --- CAMERA CONFIGURATION ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // Lower the master clock slightly to 10MHz to ensure clean, reliable edge detection
  config.xclk_freq_hz = 10000000; 
  config.pixel_format = PIXFORMAT_JPEG; 

  // Allocate frame buffers using the successfully mounted OPI PSRAM
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;        // 640x480 resolution
    config.jpeg_quality = 10;                 // Clean image clarity
    config.fb_count = 2;                      // Stable double buffer allocation
    config.fb_location = CAMERA_FB_IN_PSRAM;  // Force frames into your 8MB PSRAM space
    config.grab_mode = CAMERA_GRAB_LATEST;    // CRITICAL: Prevents driver framework timeouts
    Serial.println("OPI PSRAM Confirmed. Streaming buffers locked.");
  } else {
    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Brief pause directly before probing the registers
  delay(200);

  // Initialize the camera module with stabilized variables
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed! Registry Error Code: 0x%x\n", err);
    return;
  }
  Serial.println("Camera hardware successfully identified and initialized!");

  // --- CONNECT TO WI-FI ---
  Serial.printf("\nConnecting to network: %s ", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nWi-Fi Link Established Successfully!");

  // Start the video streaming framework
  startCameraServer();

  Serial.print("\n=== SYSTEM ONLINE ===\nOpen this link on your laptop web browser:\nhttp://");
  Serial.println(WiFi.localIP());
  Serial.println("=====================");
}

// --- STREAM PIPELINE HANDLER ---
void handleStreamClient(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Pipeline Frame Drop Detected");
      break;
    }

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    
    esp_camera_fb_return(fb); 
    delay(1); 
  }
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String req = client.readStringUntil('\r');
    if (req.indexOf("/stream") != -1 || req.indexOf("/") != -1) {
      handleStreamClient(client);
    }
    client.stop();
  }
}

void startCameraServer() {
  server.begin();
}