
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

# Verify the peer cache for the single configured assistant session.
SESSION_VALUE="${STRING_SESSION:-${STRING_SESSIONS:-}}"
if [ -z "${SESSION_VALUE//[[:space:]]/}" ]; then
    echo "[ERROR] Set STRING_SESSION to one valid Pyrogram session string."
    exit 1
fi
CACHE_FILE="$SCRIPT_DIR/data/peer_cache.json"
if [ ! -s "$CACHE_FILE" ]; then
    echo "[ERROR] Missing or empty peer cache: $CACHE_FILE"
    exit 1
fi
ls -lh "$CACHE_FILE"
echo "[SYSTEM] [STARTUP] Single assistant peer cache ready."

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
