import cv2
import socket
import time
import numpy as np
from collections import deque

# --- SETTINGS ---
STREAM_URL = "http://192.168.1.117:81/stream"
WROOM_IP   = "192.168.1.106"
UDP_PORT   = 4210

CENTER_DEADZONE      = 40
IDLE_TIMEOUT         = 10.0
MIN_FACE_SIZE        = 80
CONFIDENCE_THRESHOLD = 0.6

PAN_GAIN  = 0.8
TILT_GAIN = 0.6

MAX_PAN_STEP  = 180
MAX_TILT_STEP = 80

LOCK_SIZE_TOLERANCE = 0.35

SEARCH_PAN_SPEED       = 400
SEARCH_TILT_RANGE      = 20
SEARCH_TOTAL_ROTATIONS = 4
SEARCH_ROTATION_TIME   = 3.0

PROTOTXT_PATH   = r"C:\Users\tony0\deploy.prototxt"
CAFFEMODEL_PATH = r"C:\Users\tony0\res10_300x300_ssd_iter_140000.caffemodel"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    net = cv2.dnn.readNetFromCaffe(PROTOTXT_PATH, CAFFEMODEL_PATH)
    USE_DNN = True
    print("DNN face detector loaded successfully")
except Exception as e:
    print(f"DNN model failed to load ({e}), falling back to Haar cascade")
    USE_DNN = False
    net = None

if not USE_DNN:
    face_cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
    )

history_x = deque(maxlen=12)
history_y = deque(maxlen=12)
cap = cv2.VideoCapture(STREAM_URL)

# --- STATE ---
locked               = False
locked_face_pos      = None
locked_face_size     = None
original_face_size   = None
waiting_for_original = False

last_face_time       = time.time()
search_mode          = False
search_start_time    = None
returning_to_neutral = False
neutral_sent         = False

print(f"Connecting to {WROOM_IP}...")
print("Press L to lock/unlock face | Q to quit")


def detect_faces_dnn(frame):
    h, w = frame.shape[:2]
    blob = cv2.dnn.blobFromImage(
        cv2.resize(frame, (300, 300)), 1.0,
        (300, 300), (104.0, 177.0, 123.0)
    )
    net.setInput(blob)
    detections = net.forward()
    faces = []
    for i in range(detections.shape[2]):
        confidence = detections[0, 0, i, 2]
        if confidence > CONFIDENCE_THRESHOLD:
            box = detections[0, 0, i, 3:7] * np.array([w, h, w, h])
            x1, y1, x2, y2 = box.astype(int)
            fw, fh = x2 - x1, y2 - y1
            if fw >= MIN_FACE_SIZE and fh >= MIN_FACE_SIZE:
                faces.append((x1, y1, fw, fh))
    return faces


def detect_faces_haar(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_cascade.detectMultiScale(
        gray, scaleFactor=1.1, minNeighbors=7,
        minSize=(MIN_FACE_SIZE, MIN_FACE_SIZE)
    )
    return list(faces) if len(faces) > 0 else []


def find_locked_face(faces, last_pos, last_size):
    """Match face by position + size. Fixed: no longer resets best_idx mid-loop."""
    lx, ly     = last_pos
    lw, lh     = last_size
    best_idx   = None
    best_score = float('inf')

    for i, (fx, fy, fw, fh) in enumerate(faces):
        face_cx  = fx + fw // 2
        face_cy  = fy + fh // 2
        pos_dist = abs(face_cx - lx) + abs(face_cy - ly)
        size_diff = ((abs(fw - lw) / max(lw, 1)) + (abs(fh - lh) / max(lh, 1))) / 2

        if size_diff > LOCK_SIZE_TOLERANCE:
            continue

        score = pos_dist + (size_diff * 500)
        if score < best_score:
            best_score = score   # FIX: was accidentally resetting best_idx here too
            best_idx   = i

    return best_idx


def find_original_face(faces, original_size):
    """Only accept a face whose size closely matches the original locked face."""
    lw, lh = original_size
    for i, (fx, fy, fw, fh) in enumerate(faces):
        size_diff = ((abs(fw - lw) / max(lw, 1)) + (abs(fh - lh) / max(lh, 1))) / 2
        if size_diff < 0.25:
            return i
    return None


def pick_nearest_face_to(faces, tx, ty):
    return min(
        range(len(faces)),
        key=lambda i: abs((faces[i][0] + faces[i][2]//2) - tx)
                    + abs((faces[i][1] + faces[i][3]//2) - ty)
    )


def send_move(x, y):
    sock.sendto(f"{int(x)},{int(y)}".encode(), (WROOM_IP, UDP_PORT))


while True:
    ret, frame = cap.read()
    if not ret:
        continue

    frame = cv2.flip(frame, -1)

    h, w, _ = frame.shape
    cx, cy   = w // 2, h // 2
    now      = time.time()

    faces = detect_faces_dnn(frame) if USE_DNN else detect_faces_haar(frame)

    cv2.rectangle(frame,
                  (cx - CENTER_DEADZONE, cy - CENTER_DEADZONE),
                  (cx + CENTER_DEADZONE, cy + CENTER_DEADZONE),
                  (255, 200, 0), 1)

    target_idx  = None
    target_face = None

    if len(faces) > 0:

        if waiting_for_original and original_face_size is not None:
            orig_idx = find_original_face(faces, original_face_size)

            if orig_idx is not None:
                target_face          = faces[orig_idx]
                target_idx           = orig_idx
                locked               = True
                locked_face_pos      = (target_face[0] + target_face[2]//2,
                                        target_face[1] + target_face[3]//2)
                locked_face_size     = (target_face[2], target_face[3])
                waiting_for_original = False
                search_mode          = False
                search_start_time    = None
                returning_to_neutral = False
                neutral_sent         = False
                last_face_time       = now
                print("Original face re-acquired — locked back on")
            else:
                for (fx, fy, fw, fh) in faces:
                    cv2.rectangle(frame, (fx, fy), (fx+fw, fy+fh), (40, 40, 40), 1)
                cv2.putText(frame, "WAITING FOR ORIGINAL FACE...",
                            (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 80, 255), 2)

        elif locked and locked_face_pos is not None and locked_face_size is not None:
            target_idx = find_locked_face(faces, locked_face_pos, locked_face_size)

            if target_idx is None:
                waiting_for_original = True
                locked               = False
                last_face_time       = now - IDLE_TIMEOUT
                print("ORIGINAL FACE LOST — entering idle search, ignoring other faces")
            else:
                target_face     = faces[target_idx]
                locked_face_pos = (target_face[0] + target_face[2]//2,
                                   target_face[1] + target_face[3]//2)

        else:
            if not waiting_for_original:
                last_face_time       = now
                search_mode          = False
                search_start_time    = None
                returning_to_neutral = False
                neutral_sent         = False
                target_idx  = pick_nearest_face_to(faces, cx, cy)
                target_face = faces[target_idx]

        if not waiting_for_original:
            for i, (fx, fy, fw, fh) in enumerate(faces):
                if i == target_idx:
                    color     = (255, 50, 50) if locked else (50, 255, 50)
                    thickness = 2
                    label     = "LOCKED" if locked else "TRACKING"
                    cv2.putText(frame, label, (fx, fy - 8),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
                else:
                    color     = (80, 80, 80)
                    thickness = 1
                cv2.rectangle(frame, (fx, fy), (fx+fw, fy+fh), color, thickness)

        if target_face is not None:
            fx, fy, fw, fh = target_face
            face_cx = fx + fw // 2
            face_cy = fy + fh // 2

            history_x.append(face_cx - cx)
            history_y.append(face_cy - cy)
            avg_x = sum(history_x) / len(history_x)
            avg_y = sum(history_y) / len(history_y)

            if abs(avg_x) > CENTER_DEADZONE:
                error_x  = avg_x - (CENTER_DEADZONE * np.sign(avg_x))
                move_pan = int(np.clip(error_x * PAN_GAIN, -MAX_PAN_STEP, MAX_PAN_STEP))
            else:
                move_pan = 0

            if abs(avg_y) > CENTER_DEADZONE:
                error_y   = avg_y - (CENTER_DEADZONE * np.sign(avg_y))
                move_tilt = int(np.clip(error_y * TILT_GAIN, -MAX_TILT_STEP, MAX_TILT_STEP))
            else:
                move_tilt = 0

            if move_pan != 0 or move_tilt != 0:
                send_move(move_pan, -move_tilt)

            cv2.circle(frame, (face_cx, face_cy), 4,
                       (255, 50, 50) if locked else (50, 255, 50), -1)

            cv2.putText(frame,
                        f"err x:{int(avg_x)} y:{int(avg_y)}  pan:{move_pan} tilt:{move_tilt}",
                        (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (200, 200, 200), 1)

    else:
        idle_elapsed = now - last_face_time

        if idle_elapsed > IDLE_TIMEOUT:
            if not search_mode:
                if not returning_to_neutral:
                    returning_to_neutral = True
                    neutral_sent         = False
                    print("Returning tilt to neutral before idle search")

                if not neutral_sent:
                    send_move(0, 0)
                    neutral_sent = True
                    time.sleep(1.0)
                    search_mode       = True
                    search_start_time = now
                    print("Tilt at neutral (pos zero) — starting ±20 deg tilt search")

        elif idle_elapsed > 1.0:
            remaining = max(0, IDLE_TIMEOUT - idle_elapsed)
            cv2.putText(frame, f"Search in {remaining:.1f}s",
                        (w - 185, h - 15), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (0, 180, 255), 1)

    if search_mode and search_start_time is not None:
        search_elapsed    = now - search_start_time
        total_search_time = SEARCH_TOTAL_ROTATIONS * SEARCH_ROTATION_TIME

        if search_elapsed < total_search_time:
            pan_cmd  = SEARCH_PAN_SPEED
            tilt_cmd = int(SEARCH_TILT_RANGE * np.sin(search_elapsed * 1.2))
            send_move(pan_cmd, tilt_cmd)

            rotations_done = search_elapsed / SEARCH_ROTATION_TIME
            progress       = int((search_elapsed / total_search_time) * 100)
            flash_color    = (0, 0, 255) if int(now * 2) % 2 == 0 else (0, 80, 200)
            label = "SEARCHING FOR ORIGINAL FACE..." if waiting_for_original \
                    else f"SEARCHING... {rotations_done:.1f}/4 rotations ({progress}%)"
            cv2.putText(frame, label,
                        (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.55, flash_color, 2)

            tilt_deg = int(SEARCH_TILT_RANGE * np.sin(search_elapsed * 1.2))
            cv2.putText(frame, f"Tilt: {tilt_deg:+d} deg from neutral",
                        (10, 65), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (180, 180, 0), 1)
        else:
            send_move(0, 0)
            search_mode          = False
            search_start_time    = None
            returning_to_neutral = False
            neutral_sent         = False
            if not waiting_for_original:
                last_face_time = now
            print("Search sweep complete")

    if waiting_for_original:
        cv2.putText(frame, "ORIGINAL FACE LOST — SEARCHING",
                    (10, h - 40), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 80, 255), 2)

    lock_text  = "LOCKED" if locked else ("WAITING FOR ORIGINAL" if waiting_for_original else "TRACKING")
    lock_color = (255, 50, 50) if locked else (0, 80, 255) if waiting_for_original else (50, 255, 50)
    cv2.putText(frame, f"[L] {lock_text}",
                (10, h - 15), cv2.FONT_HERSHEY_SIMPLEX, 0.55, lock_color, 2)

    cv2.imshow("Face Tracker", frame)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif key == ord('l') or key == ord('L'):
        if not locked and not waiting_for_original:
            if target_face is not None:
                locked               = True
                waiting_for_original = False
                locked_face_pos      = (target_face[0] + target_face[2]//2,
                                        target_face[1] + target_face[3]//2)
                locked_face_size     = (target_face[2], target_face[3])
                original_face_size   = locked_face_size
                print(f"Locked onto face at {locked_face_pos} size {locked_face_size}")
            else:
                print("No face to lock onto")
        elif waiting_for_original:
            waiting_for_original = False
            locked               = False
            locked_face_pos      = None
            locked_face_size     = None
            original_face_size   = None
            last_face_time       = now
            print("Cancelled original face wait — back to normal tracking")
        else:
            locked             = False
            locked_face_pos    = None
            locked_face_size   = None
            original_face_size = None
            print("Unlocked — tracking nearest face")

cap.release()
cv2.destroyAllWindows()