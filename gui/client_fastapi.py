import logging
import os
import threading
import time
from pathlib import Path

import cv2
import uvicorn
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, JSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles

import multicast_core

MCAST_GRP = "224.0.0.1"
MCAST_PORT = 5000

# Создание необходимых директорий
os.makedirs("logs", exist_ok=True)
os.makedirs("static", exist_ok=True)
os.makedirs("templates", exist_ok=True)

# Конфигурация логгера
logging.basicConfig(
    level=logging.INFO,
    filename="logs/client_logs.log",
    filemode="a",
    format="[%(asctime)s] %(levelname)s - %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger(__name__)


# Wrapper С++ библиотеки
class StreamManager:
    def __init__(self):
        self.client = multicast_core.Receiver(MCAST_GRP, MCAST_PORT)
        self.is_streaming = False
        self.last_preview = None

    def start_streaming(self):
        """Start streaming if not already started."""
        if not self.is_streaming:
            if self.client.start():
                self.is_streaming = True
                logger.info("Streaming started successfully")
            else:
                raise RuntimeError("Failed to start streaming")

    def stop_streaming(self):
        """Stop streaming if currently streaming."""
        if self.is_streaming:
            self.client.stop()
            self.is_streaming = False
            self.last_preview = None
            logger.info("Streaming stopped")

    def get_latest_frame(self):
        """Get the last preview frame if streaming."""
        if self.is_streaming:
            frame = self.client.get_latest_frame()
            if frame is not None:
                self.last_preview = frame
                return frame
        return None

    def get_status(self):
        """Get the current status of the streaming."""
        return "streaming" if self.is_streaming else "stopped"

    def check_server_activity(self):
        """Check if the server is active."""
        if self.client.is_active():
            logger.info("Server is active")
            return True
        else:
            logger.warning("Server is not active")
            return False


class WebServer:
    def __init__(self, controller):
        self.app = FastAPI()
        self.controller = controller
        self.templates_path = (
            Path(__file__).parent / "templates" / "dashboard_client.html"
        )

        self.app.add_api_route("/stats", self.get_stats)
        self.app.add_api_route("/video_feed", self.video_feed)
        self.app.add_api_route("/control", self.control_stream, methods=["POST"])
        self.app.add_api_route("/status", self.get_status)
        self.app.add_api_route("/", self.get_html, response_class=HTMLResponse)
        self.app.mount("/static", StaticFiles(directory="static"), name="static")

    def get_stats(self):
        stats = self.controller.client.getStatistics()
        return JSONResponse({
            "total_packets": stats.totalPacketsReceived,
            "lost_packets": stats.totalCorruptedPackets,
            "complete_frames": stats.totalFramesDecoded,
            "average_fps": round(stats.avgFps, 2),
            "packet_loss": round(
                (stats.totalCorruptedPackets / stats.totalPacketsReceived * 100) if stats.totalPacketsReceived > 0 else 0.0, 2
            )
        })


    def video_feed(self):
        """Endpoint for video streaming."""
        return StreamingResponse(
            self.generate_frames(),
            media_type="multipart/x-mixed-replace; boundary=frame",
        )

    def get_placeholder_image(self):
        """Return a simple 'No stream available' image"""
        import numpy as np

        img = 255 * np.ones((480, 640, 3), dtype=np.uint8)
        cv2.putText(
            img,
            "No Stream Available",
            (100, 240),
            cv2.FONT_HERSHEY_SIMPLEX,
            1.2,
            (0, 0, 255),
            2,
        )
        return img

    def generate_frames(self):
        """Generate video frames for streaming."""
        while True:
            try:
                frame = self.controller.get_latest_frame()

                # Если долго нет нового кадра — показываем заглушку
                if frame is None or not self.controller.check_server_activity():
                    logger.warning("No frame received recently, showing placeholder.")
                    placeholder = self.get_placeholder_image()
                    _, jpeg = cv2.imencode(".jpg", placeholder)
                else:
                    _, jpeg = cv2.imencode(".jpg", frame)

                yield (
                    b"--frame\r\n"
                    b"Content-Type: image/jpeg\r\n\r\n" + jpeg.tobytes() + b"\r\n"
                )
                time.sleep(1 / 30)
            except Exception as e:
                logger.error(f"Preview generation error: {str(e)}")
                break

    async def control_stream(self, request: Request):
        """Control the streaming based on the request data."""
        data = await request.json()
        action = data.get("action")

        if action == "start":
            self.controller.start_streaming()
        elif action == "stop":
            self.controller.stop_streaming()

        return JSONResponse({"status": self.controller.get_status()})

    async def get_status(self):
        """Get the current status of the streaming."""
        return JSONResponse({"status": self.controller.get_status()})

    def get_html(self):
        """Get the HTML template for the dashboard."""
        try:
            with open(self.templates_path, "r", encoding="utf-8") as f:
                return HTMLResponse(f.read())
        except FileNotFoundError:
            return HTMLResponse(
                "<h1>Error: Template file not found</h1>", status_code=500
            )


def main():
    controller = StreamManager()
    web_server = WebServer(controller)

    server_thread = threading.Thread(
        target=uvicorn.run,
        args=(web_server.app,),
        kwargs={"host": "0.0.0.0", "port": 8001, "log_level": "info"},
        daemon=True,
    )
    server_thread.start()
    logger.info("Web server started at http://0.0.0.0:8001")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logger.info("Shutting down...")
        controller.stop_streaming()


if __name__ == "__main__":
    main()
