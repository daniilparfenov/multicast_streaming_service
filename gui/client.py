import time

import cv2

import multicast_core

MCAST_GRP = "224.0.0.1"
MCAST_PORT = 5000


def main():
    receiver = multicast_core.Receiver(MCAST_GRP, MCAST_PORT)

    if not receiver.start():
        print("Failed to start stream")
        return

    try:
        while True:
            frame = receiver.get_latest_frame()
            if frame is None or frame.size == 0:
                print("No valid frame")
                time.sleep(0.1)
                continue

            cv2.imshow("Receiver Preview", frame)
            if cv2.waitKey(1) & 0xFF == 27:
                break
    except KeyboardInterrupt:
        print("Stream interrupted by user")
    finally:
        receiver.stop()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
