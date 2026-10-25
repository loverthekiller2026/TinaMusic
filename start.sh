
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p "$SCRIPT_DIR/data"

# Restore the cache copied into the image if the runtime cache is missing.
if [ ! -s "$SCRIPT_DIR/data/peer_cache.json" ] && \
   [ -s "/app/data/peer_cache.json" ]; then
    cp "/app/data/peer_cache.json" "$SCRIPT_DIR/data/peer_cache.json"
fi

# Generate the peer cache if it does not exist.
if [ ! -s "$SCRIPT_DIR/data/peer_cache.json" ]; then
    echo "[SYSTEM] [STARTUP] Peer cache missing; generating it..."

    if [ ! -f "$SCRIPT_DIR/generate_cache.py" ]; then
        echo "[ERROR] generate_cache.py is missing."
        exit 1
    fi

    if [ ! -x "/opt/peer-env/bin/python" ]; then
        echo "[ERROR] Python virtual environment not found."
        exit 1
    fi

    /opt/peer-env/bin/python "$SCRIPT_DIR/generate_cache.py"

    if [ ! -s "$SCRIPT_DIR/data/peer_cache.json" ]; then
        echo "[ERROR] Peer cache generation failed."
        exit 1
    fi
fi

echo "[SYSTEM] [STARTUP] Peer cache status:"
ls -lh "$SCRIPT_DIR/data/peer_cache.json"

# Start the Render HTTP health server.
echo "[SYSTEM] [STARTUP] Starting HTTP health server..."
python3 /app/health_server.py &

HEALTH_PID=$!

# Stop the container if the health server exits unexpectedly.
sleep 1
if ! kill -0 "$HEALTH_PID" 2>/dev/null; then
    echo "[ERROR] HTTP health server failed to start."
    wait "$HEALTH_PID" || true
    exit 1
fi

# Start the C++ Telegram music bot.
if [ ! -x "$SCRIPT_DIR/bin/tina-music-bot" ]; then
    echo "[ERROR] tina-music-bot binary is missing or not executable."
    exit 1
fi

echo "[SYSTEM] [STARTUP] Launching pure C++20 TinaMusic engine..."
exec "$SCRIPT_DIR/bin/tina-music-bot"
