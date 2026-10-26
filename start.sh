
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

# Verify that every configured assistant has its own peer cache.
SESSION_CONFIG="${STRING_SESSIONS:-${STRING_SESSION:-}}"
IFS=',' read -r -a SESSION_LIST <<< "$SESSION_CONFIG"
SESSION_INDEX=0
for SESSION_VALUE in "${SESSION_LIST[@]}"; do
    [ -z "${SESSION_VALUE//[[:space:]]/}" ] && continue
    SESSION_INDEX=$((SESSION_INDEX + 1))
    if [ "$SESSION_INDEX" -eq 1 ]; then
        CACHE_FILE="$SCRIPT_DIR/data/peer_cache.json"
    else
        CACHE_FILE="$SCRIPT_DIR/data/peer_cache_${SESSION_INDEX}.json"
    fi
    if [ ! -s "$CACHE_FILE" ]; then
        echo "[ERROR] Missing or empty peer cache for assistant session $SESSION_INDEX: $CACHE_FILE"
        exit 1
    fi
    ls -lh "$CACHE_FILE"
done
if [ "$SESSION_INDEX" -eq 0 ]; then
    echo "[ERROR] No STRING_SESSIONS configured."
    exit 1
fi
echo "[SYSTEM] [STARTUP] Peer caches ready for $SESSION_INDEX assistant session(s)."

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
