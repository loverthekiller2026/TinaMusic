
import os
from http.server import BaseHTTPRequestHandler, HTTPServer


class HealthHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path in ("/", "/health"):
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(b"TinaMusic is running")
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass


def main():
    port = int(os.environ.get("PORT", "10000"))

    server = HTTPServer(("0.0.0.0", port), HealthHandler)

    print(
        f"[HEALTH] Listening on 0.0.0.0:{port}",
        flush=True,
    )

    server.serve_forever()


if __name__ == "__main__":
    main()
