
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

RUN python3 -m venv /opt/peer-env && \
    /opt/peer-env/bin/pip install --no-cache-dir Pyrogram==2.0.106

COPY . .

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

# Validate required runtime files; do not create fake application scripts.
RUN set -eux; \
    test -x /build/start.sh || test -f /build/start.sh; \
    test -x /build/bin/tina-music-bot || test -f /build/bin/tina-music-bot; \
    test -d /build/locales; \
    if [ ! -f /build/generate_cache.py ]; then \
        echo "ERROR: generate_cache.py is missing from the project root."; \
        exit 1; \
    fi; \
    mkdir -p /runtime/data; \
    if [ -d /build/data ]; then \
        cp -a /build/data/. /runtime/data/; \
    fi
    
FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ffmpeg \
        curl \
        ca-certificates \
        libcurl4 \
        libyaml-cpp0.8 \
        zlib1g \
        python3 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
EXPOSE 10000
COPY --from=builder /build/bin /app/bin
COPY --from=builder /build/locales /app/locales
COPY --from=builder /build/start.sh /app/start.sh
COPY --from=builder /runtime/data /app/data
COPY --from=builder /opt/peer-env /opt/peer-env
COPY --from=builder /build/generate_cache.py /app/generate_cache.py

RUN chmod +x /app/start.sh /app/bin/*

ENTRYPOINT ["/bin/bash", "/app/start.sh"]
