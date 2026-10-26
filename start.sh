
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p "$SCRIPT_DIR/data"

# Regenerate the peer cache on every startup.
echo "[SYSTEM] [STARTUP] Regenerating peer cache..."

# Check that the cache generator exists.
if [ ! -f "$SCRIPT_DIR/generate_cache.py" ]; then
    echo "[ERROR] generate_cache.py is missing."
    exit 1
fi

# Check that the Python virtual environment exists.
if [ ! -x "/opt/peer-env/bin/python" ]; then
    echo "[ERROR] Python virtual environment not found: /opt/peer-env/bin/python"
    exit 1
fi

# Verify Pyrogram is installed in the correct environment.
if ! /opt/peer-env/bin/python -c "import pyrogram"; then
    echo "[ERROR] Pyrogram is not installed in /opt/peer-env."
    exit 1
fi

echo "[SYSTEM] [STARTUP] Pyrogram dependency check passed."

# Generate the Telegram peer cache.
/opt/peer-env/bin/python "$SCRIPT_DIR/generate_cache.py"

# Verify that the cache file exists and is not empty.
if [ ! -s "$SCRIPT_DIR/data/peer_cache.json" ]; then
    echo "[ERROR] Peer cache generation failed or produced an empty file."
    exit 1
fi

echo "[SYSTEM] [STARTUP] Peer cache status:"
ls -lh "$SCRIPT_DIR/data/peer_cache.json"

# Start the Render HTTP health server.
echo "[SYSTEM] [STARTUP] Starting HTTP health server..."
python3 /app/health_server.py &

HEALTH_PID=$!

# Check whether the health server starts successfully.
sleep 1

if ! kill -0 "$HEALTH_PID" 2>/dev/null; then
    echo "[ERROR] HTTP health server failed to start."
    wait "$HEALTH_PID" || true
    exit 1
fi

# Verify that the C++ music bot executable exists.
if [ ! -x "$SCRIPT_DIR/bin/tina-music-bot" ]; then
    echo "[ERROR] tina-music-bot binary is missing or not executable."
    exit 1
fi

# Launch the C++20 Telegram music engine.
echo "[SYSTEM] [STARTUP] Launching pure C++20 TinaMusic engine..."
exec "$SCRIPT_DIR/bin/tina-music-bot"
