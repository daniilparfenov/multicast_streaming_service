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
    filename="logs/sender_logs.log",
    filemode="a",
    format="[%(asctime)s] %(levelname)s - %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger(__name__)


# Wrapper С++ библиотеки
class StreamManager:
    def __init__(self):
        self.sender = multicast_core.Sender(MCAST_GRP, MCAST_PORT)
        self.is_streaming = False
        self.last_preview = None

    def start_streaming(self):
        """Start streaming if not already started."""
        if not self.is_streaming:
            if self.sender.start_stream():
                self.is_streaming = True
                logger.info("Streaming started successfully")
            else:
                raise RuntimeError("Failed to start streaming")

    def stop_streaming(self):
        """Stop streaming if currently streaming."""
        if self.is_streaming:
            self.sender.stop_stream()
            self.is_streaming = False
            logger.info("Streaming stopped")

    def get_preview_frame(self):
        """Get the last preview frame if streaming."""
        if self.is_streaming:
            frame = self.sender.get_preview_frame()
            if frame is not None:
                self.last_preview = frame
            return self.last_preview
        return None

    def get_status(self):
        """Get the current status of the streaming."""
        return "streaming" if self.is_streaming else "stopped"


class WebServer:
    def __init__(self, controller):
        self.app = FastAPI()
        self.controller = controller
        self.templates_path = (
            Path(__file__).parent / "templates" / "dashboard_sender.html"
        )

        self.app.add_api_route("/stats", self.get_stats)
        self.app.add_api_route("/video_feed", self.video_feed)
        self.app.add_api_route("/control", self.control_stream, methods=["POST"])
        self.app.add_api_route("/status", self.get_status)
        self.app.add_api_route("/", self.get_html, response_class=HTMLResponse)
        self.app.mount("/static", StaticFiles(directory="static"), name="static")

    def get_stats(self):
        active_clients_count = self.controller.sender.get_active_client_count()
        return JSONResponse(
            {
                "viewers": active_clients_count,
            }
        )

    def video_feed(self):
        """Endpoint for video streaming."""
        return StreamingResponse(
            self.generate_frames(),
            media_type="multipart/x-mixed-replace; boundary=frame",
        )

    def generate_frames(self):
        """Generate video frames for streaming."""
        while True:
            try:
                frame = self.controller.get_preview_frame()
                if frame is None:
                    continue
                logger.info(f"Frame shape: {frame.shape}")
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
        """Render the HTML template for the dashboard."""
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
        kwargs={"host": "0.0.0.0", "port": 8000, "log_level": "info"},
        daemon=True,
    )
    server_thread.start()
    logger.info("Web server started at http://0.0.0.0:8000")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logger.info("Shutting down...")
        controller.stop_streaming()


if __name__ == "__main__":
    main()
