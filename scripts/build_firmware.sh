#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="${WORKDIR:-$REPO_ROOT/build}"
MESHCORE_REF="${MESHCORE_REF:-main}"
MESHCORE_REPO="${MESHCORE_REPO:-https://github.com/meshcore-dev/MeshCore.git}"
MESHCORE_DIR="${MESHCORE_DIR:-$WORKDIR/MeshCore}"
TARGET_ENV="${TARGET_ENV:-RAK_4631_companion_radio_eth}"
DIST_DIR="${DIST_DIR:-$REPO_ROOT/dist/$TARGET_ENV}"

mkdir -p "$WORKDIR" "$DIST_DIR"

if [ ! -d "$MESHCORE_DIR/.git" ]; then
  echo "cloning MeshCore ref $MESHCORE_REF"
  git clone --depth 1 --branch "$MESHCORE_REF" "$MESHCORE_REPO" "$MESHCORE_DIR"
else
  echo "using existing MeshCore checkout: $MESHCORE_DIR"
  git -C "$MESHCORE_DIR" fetch --depth 1 origin "$MESHCORE_REF"
  git -C "$MESHCORE_DIR" checkout FETCH_HEAD
fi

python3 "$REPO_ROOT/scripts/prepare_meshcore_tree.py" \
  --meshcore "$MESHCORE_DIR" \
  --overlay "$REPO_ROOT"

if ! command -v pio >/dev/null 2>&1; then
  echo "PlatformIO not found; installing for current Python user"
  python3 -m pip install --user --upgrade platformio
  export PATH="$HOME/.local/bin:$PATH"
fi

cd "$MESHCORE_DIR"

echo "building $TARGET_ENV"
pio run -e "$TARGET_ENV"

echo "collecting firmware artifacts"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

BUILD_DIR="$MESHCORE_DIR/.pio/build/$TARGET_ENV"
if [ ! -d "$BUILD_DIR" ]; then
  echo "error: build dir not found: $BUILD_DIR" >&2
  exit 1
fi

# Keep the common PlatformIO / nRF52 outputs. Some envs emit only a subset.
find "$BUILD_DIR" -maxdepth 1 -type f \
  \( -name 'firmware.*' -o -name '*.uf2' -o -name '*.zip' -o -name '*.hex' -o -name '*.bin' -o -name '*.elf' -o -name '*.map' \) \
  -exec cp -v {} "$DIST_DIR/" \;

# Publish a stable DFU package name for release automation.
if [ -f "$DIST_DIR/firmware.zip" ]; then
  cp -v "$DIST_DIR/firmware.zip" "$DIST_DIR/${TARGET_ENV}-dfu.zip"
fi

# Record enough context to reproduce the artifact.
{
  echo "target_env=$TARGET_ENV"
  echo "meshcore_repo=$MESHCORE_REPO"
  echo "meshcore_ref=$MESHCORE_REF"
  echo "meshcore_commit=$(git -C "$MESHCORE_DIR" rev-parse HEAD)"
  echo "overlay_commit=$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "built_at_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$DIST_DIR/BUILD_INFO.txt"

ls -lah "$DIST_DIR"
echo "done: $DIST_DIR"
