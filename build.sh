#!/bin/sh
set -eu

TOOLCHAIN="$HOME/Downloads/embedded-linux-bbb-labs/integration/buildroot/output/host/share/buildroot/toolchainfile.cmake"

echo "=== Building native client ==="
cmake -S . -B build-client 
cmake --build build-client --target client 

echo "=== Building ARM server ==="
cmake -S . -B build-server \
-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
cmake --build build-server --target server

mkdir -p ./bin 
cp build-client/client ./bin 
cp build-server/server ./bin


