import time

import cv2

import multicast_core

MCAST_GRP = "224.0.0.1"
MCAST_PORT = 5000


def main():
    sender = multicast_core.Sender(MCAST_GRP, MCAST_PORT)

    if not sender.start_stream():
        print("Failed to start stream")
        return

    try:
        while True:
            frame = sender.get_preview_frame()
            if frame is None or frame.size == 0:
                print("No valid frame")
                time.sleep(0.1)
                continue

            cv2.imshow("Sender Preview", frame)
            if cv2.waitKey(1) & 0xFF == 27:
                break
    except KeyboardInterrupt:
        print("Stream interrupted by user")
    finally:
        sender.stop_stream()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
