
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

pkill -9 -f "tina-music-bot" 2>/dev/null || true
pkill -9 -f "yukki-music-bot" 2>/dev/null || true
pkill -9 -f "assistant-bridge" 2>/dev/null || true
sleep 1

if [ ! -f "bin/tina-music-bot" ]; then
    echo "[SYSTEM] [STARTUP] TinaMusic binary not found, compiling..."
    make -j1
fi

mkdir -p "$SCRIPT_DIR/data"

if [ ! -s "$SCRIPT_DIR/data/peer_cache.json" ]; then
    echo "[SYSTEM] [STARTUP] Peer cache missing; generating it..."

    if [ ! -f "$SCRIPT_DIR/generate_cache.py" ]; then
        echo "[ERROR] generate_cache.py is missing from $SCRIPT_DIR"
        exit 1
    fi

    if [ ! -x "/opt/peer-env/bin/python" ]; then
        echo "[ERROR] Python virtual environment not found at /opt/peer-env"
        exit 1
    fi

    /opt/peer-env/bin/python "$SCRIPT_DIR/generate_cache.py"

    if [ ! -s "$SCRIPT_DIR/data/peer_cache.json" ]; then
        echo "[ERROR] Peer cache generation failed; refusing to start."
        exit 1
    fi
fi

echo "[SYSTEM] [STARTUP] Peer cache status:"
ls -lh "$SCRIPT_DIR/data/peer_cache.json"

echo "[SYSTEM] [STARTUP] Launching pure C++20 TinaMusic engine..."
exec "$SCRIPT_DIR/bin/tina-music-bot"
