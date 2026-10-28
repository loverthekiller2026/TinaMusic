import os
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

HEARTBEAT_PATH = "/tmp/tinamusic-bot-heartbeat"
STARTED_AT = time.monotonic()
STARTUP_GRACE_SECONDS = 180
MAX_HEARTBEAT_AGE_SECONDS = 120


class HealthHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path not in ("/", "/health"):
            self.send_response(404)
            self.end_headers()
            return

        now = time.time()
        try:
            heartbeat_age = now - os.path.getmtime(HEARTBEAT_PATH)
        except OSError:
            heartbeat_age = None

        if heartbeat_age is not None and heartbeat_age <= MAX_HEARTBEAT_AGE_SECONDS:
            status = 200
            message = "TinaMusic bot loop is healthy"
        elif heartbeat_age is None and time.monotonic() - STARTED_AT < STARTUP_GRACE_SECONDS:
            # Give peer-cache generation and Telegram/MTProto initialization time.
            status = 200
            message = "TinaMusic is starting"
        else:
            status = 503
            message = "TinaMusic bot heartbeat is missing or stale"

        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.end_headers()
        self.wfile.write(message.encode("utf-8"))

    def log_message(self, format, *args):
        pass


def main():
    port = int(os.environ.get("PORT", "10000"))
    server = HTTPServer(("0.0.0.0", port), HealthHandler)
    print(f"[HEALTH] Listening on 0.0.0.0:{port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
