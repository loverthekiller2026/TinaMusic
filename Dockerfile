
FROM ubuntu:24.04 AS builder

WORKDIR /build

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        g++ \
        make \
        cmake \
        libcurl4-openssl-dev \
        libyaml-cpp-dev \
        zlib1g-dev \
        nlohmann-json3-dev \
        curl \
        unzip \
        ca-certificates \
        python3 \
        python3-venv && \
    rm -rf /var/lib/apt/lists/*

# Create the Python environment used by generate_cache.py.
RUN python3 -m venv /opt/peer-env && \
    /opt/peer-env/bin/python -m pip install --no-cache-dir \
        Pyrogram==2.0.106

COPY . .

# Download the required static ntgcalls library if it is missing,
# then compile the C++20 application.
RUN set -eux; \
    ARCH="$(uname -m)"; \
    mkdir -p vendor/ntgcalls/lib vendor/ntgcalls/include; \
    if [ ! -f vendor/ntgcalls/lib/libntgcalls.a ]; then \
        case "$ARCH" in \
            x86_64) \
                URL="https://github.com/pytgcalls/ntgcalls/releases/download/v2.1.0/ntgcalls.linux-x86_64-static_libs.zip" ;; \
            aarch64|arm64) \
                URL="https://github.com/pytgcalls/ntgcalls/releases/download/v2.1.0/ntgcalls.linux-arm64-static_libs.zip" ;; \
            *) echo "Unsupported architecture: $ARCH"; exit 1 ;; \
        esac; \
        curl -fLsS "$URL" -o /tmp/ntgcalls.zip; \
        mkdir -p /tmp/ntgcalls_extracted; \
        unzip -q /tmp/ntgcalls.zip -d /tmp/ntgcalls_extracted; \
        LIB="$(find /tmp/ntgcalls_extracted -name libntgcalls.a -print -quit)"; \
        HEADER="$(find /tmp/ntgcalls_extracted -name ntgcalls.h -print -quit)"; \
        test -n "$LIB"; \
        test -n "$HEADER"; \
        cp "$LIB" vendor/ntgcalls/lib/libntgcalls.a; \
        cp "$HEADER" vendor/ntgcalls/include/ntgcalls.h; \
        rm -rf /tmp/ntgcalls.zip /tmp/ntgcalls_extracted; \
    fi; \
    make -j1

# Validate build outputs and prepare runtime data.
RUN set -eux; \
    test -f /build/start.sh; \
    test -f /build/health_server.py; \
    test -f /build/generate_cache.py; \
    test -x /build/bin/tina-music-bot || \
        chmod +x /build/bin/tina-music-bot; \
    test -d /build/locales; \
    mkdir -p /runtime/data; \
    if [ -d /build/data ]; then \
        cp -a /build/data/. /runtime/data/; \
    fi; \
    /opt/peer-env/bin/python -c \
        "import pyrogram; print('Pyrogram builder check passed:', pyrogram.__version__)"

FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ffmpeg \
        curl \
        ca-certificates \
        libcurl4 \
        libyaml-cpp0.8 \
        zlib1g \
        libstdc++6 \
        libgcc-s1 \
        python3 \
        python3-venv && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

EXPOSE 10000

COPY --from=builder /build/bin /app/bin
COPY --from=builder /build/locales /app/locales
COPY --from=builder /build/start.sh /app/start.sh
COPY --from=builder /build/health_server.py /app/health_server.py
COPY --from=builder /runtime/data /app/data
COPY --from=builder /opt/peer-env /opt/peer-env
COPY --from=builder /build/generate_cache.py /app/generate_cache.py

# Verify Pyrogram in the final runtime image.
RUN set -eux; \
    test -x /opt/peer-env/bin/python; \
    /opt/peer-env/bin/python -c \
        "import pyrogram; print('Pyrogram runtime check passed:', pyrogram.__version__)"; \
    test -f /app/health_server.py; \
    test -f /app/generate_cache.py; \
    test -x /app/bin/tina-music-bot; \
    chmod +x /app/start.sh

CMD ["bash", "/app/start.sh"]
