#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_psram.h"

const char *ssid = "ATTUHb9tnD_EXT";
const char *password = "2146292161E";
WiFiUDP udp;

#define CAMERA_MODEL_XIAO_ESP32S3
#include "camera_pins.h"

void startCameraServer();

void setup() {
  Serial.begin(115200);
  
  // Prevent Wi-Fi drops
  WiFi.setSleep(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  // HIGH QUALITY SETTINGS
  config.frame_size = FRAMESIZE_VGA;    // 640x480 for clear image
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST; 
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;             // Sharp detail
  config.fb_count = 2;                  // Smooth double-buffering

  esp_camera_init(&config);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  startCameraServer();
  udp.begin(4210);
  Serial.println("System Ready: VGA Mode Active");
}

void loop() {
  delay(10);
}