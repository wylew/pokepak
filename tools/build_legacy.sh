#!/bin/bash
# =============================================================================
#  build_legacy.sh — Build for TRIMUI Brick using Distrobox (Legacy Glibc)
# =============================================================================

CONTAINER_NAME="pokepak-builder-legacy"
IMAGE="debian:bullseye" # Bullseye has Glibc 2.31, which is often safe for older ARM64 distros

# 1. Create the container if it doesn't exist
if ! distrobox list | grep -q "$CONTAINER_NAME"; then
    echo "Creating distrobox container $CONTAINER_NAME ($IMAGE)..."
    distrobox create -n "$CONTAINER_NAME" -i "$IMAGE" --yes
fi

# 2. Run the build inside the container
echo "Running build inside $CONTAINER_NAME..."
distrobox enter "$CONTAINER_NAME" -- bash -ec "
    sudo dpkg --add-architecture arm64 && \
    sudo apt-get update -q && \
    sudo apt-get install -y -q build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
    pkg-config libsdl2-dev:arm64 libsdl2-image-dev:arm64 libsdl2-ttf-dev:arm64 && \
    cd /home/wlewin/Documents/Code/pokepak && \
    export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig && \
    make clean && \
    aarch64-linux-gnu-gcc -Iinclude -Wall -O2 -D_REENTRANT \
    \$(PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
    -c -o src/main.o src/main.c && \
    echo '--- main.o compiled OK ---' && \
    aarch64-linux-gnu-gcc -Iinclude -Wall -O2 -D_REENTRANT \
    -c -o src/tsv_parser.o src/tsv_parser.c && \
    echo '--- tsv_parser.o compiled OK ---' && \
    mkdir -p bin && \
    aarch64-linux-gnu-gcc -o bin/pokedex src/main.o src/tsv_parser.o \
    \$(PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig pkg-config --libs sdl2 SDL2_image SDL2_ttf) -lm && \
    aarch64-linux-gnu-strip bin/pokedex && \
    mkdir -p Pokedex.pak/bin/arm64 && \
    cp bin/pokedex Pokedex.pak/bin/arm64/pokedex && \
    echo '=== BUILD SUCCESSFUL ===' && \
    echo '--- GLIBC Check ---' && \
    aarch64-linux-gnu-readelf -s Pokedex.pak/bin/arm64/pokedex | grep GLIBC_ | sort -u
"

if [ $? -eq 0 ]; then
    echo "Build complete. Binary at Pokedex.pak/bin/arm64/pokedex"
else
    echo "BUILD FAILED. Check errors above."
    exit 1
fi
