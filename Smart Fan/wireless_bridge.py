import serial
import socket
import json
import sys

# --- CONFIGURATION PROFILE ---
SERIAL_PORT = 'COM8'          # Matches your XIAO board port
BAUD_RATE = 921600            # Matches SenseCraft's Method One speed
MOTOR_ESP_IP = '192.168.1.50' # Replace this with the active IP of your motor ESP32
UDP_PORT = 12345              # Target wireless destination port

# Initialize the wireless socket pipeline
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(f"Opening hardware serial link on {SERIAL_PORT}...")
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print("Wireless tracking bridge ONLINE. Looking for faces...")
    # Force the terminal to show these opening text blocks immediately
    sys.stdout.flush()
    
    while True:
        if ser.in_waiting:
            # Read the tracking data line from SenseCraft
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            # Print the live tracking data directly to your screen instantly!
            print(f"Streaming target data: {line}")
            sys.stdout.flush()
            
            # Forward the data wirelessly over UDP instantly
            sock.sendto(line.encode(), (MOTOR_ESP_IP, UDP_PORT))
            
except KeyboardInterrupt:
    print("\nClosing wireless link profiles safely.")
except Exception as e:
    print(f"Connection Error: {e}")