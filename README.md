# ESP32 UDP Face Tracker

Real-time face detection and pan/tilt tracking script in Python using OpenCV and UDP commands sent to an ESP32 controller.

This is the software side of my Smart Face-Tracking Fan, an undergraduate research project at UT Dallas (UR@ECE, Spring 2026).

## System Overview
The project uses two ESP32 boards and a computer running Python:

| Component | Role |
|---|---|
| ESP32-S3 with camera attachment | Captures video and provides the camera stream |
| Python script (`esp32_face_tracker.py`) | Reads the stream, detects and tracks faces with OpenCV, and sends pan/tilt commands over UDP |
| ESP32-WROOM | Receives the commands and drives the pan/tilt motors |

The ESP32-S3 and ESP32-WROOM boards communicate with each other over ESP-NOW.## How It Works
- Reads video frames from `STREAM_URL` and flips the image to match camera orientation.
- Detects faces using OpenCV's Caffe DNN detector (`deploy.prototxt` and `res10_300x300_ssd_iter_140000.caffemodel`), falling back to a Haar cascade if the DNN files are unavailable.
- Smooths horizontal and vertical face-center offsets using a rolling window, applies deadzone and gain/step limits, and sends `"x,y"` movement commands over UDP to `WROOM_IP:UDP_PORT`.
- Supports locking onto a specific face by size and position, entering an idle search sweep after `IDLE_TIMEOUT`, and re-acquiring a matching target size.

## Setup & Usage
1. Install dependencies:
`2. Place the DNN model files (`deploy.prototxt` and `res10_300x300_ssd_iter_140000.caffemodel`) and update `PROTOTXT_PATH` and `CAFFEMODEL_PATH` in `esp32_face_tracker.py` as needed (without them, the script falls back to the Haar cascade).
3. Edit the configuration values at the top of `esp32_face_tracker.py`:
- `STREAM_URL`: URL of the ESP32-S3 camera stream
- `WROOM_IP`: IP address of the ESP32-WROOM
- `UDP_PORT`: UDP port the ESP32-WROOM listens on
- `IDLE_TIMEOUT`: seconds without a face before the idle search sweep starts
4. Flash the ESP32 firmware from the firmware folders in this repo.
5. Run the tracker:
`## Repository Contents
- `esp32_face_tracker.py`: Python face detection and UDP command sender
- `SMART_FAN_PROJECT_CODE/`: ESP32 camera, pan/tilt, and motor Arduino sketches and libraries
- `Smart Fan/`: Hardware, PCB, and mechanical mount files for the Smart Fan build
- `CREDITS.md`: Attribution for third-party files

## Tools
Python, OpenCV, NumPy, C/C++ (ESP32 firmware), FreeRTOS, ESP-NOW, UDP

## Credits
The pan/tilt mount files are by isaac879. See `CREDITS.md` for attribution.
