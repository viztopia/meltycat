#!/usr/bin/env bash
# Builds a universal (arm64 + x86_64) meltycat.app into macos/build.
# Usage: bash macos/build.sh [version]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
APP_NAME="meltycat"
BUNDLE_ID="io.github.viztopia.meltycat"
VERSION="${1:-0.0.0}"
MIN_MACOS="11.0"

OUT="$ROOT/build"
APP="$OUT/$APP_NAME.app"
CONTENTS="$APP/Contents"
MACOS_DIR="$CONTENTS/MacOS"
RES_DIR="$CONTENTS/Resources"

shopt -s nullglob
SOURCES=("$ROOT"/Sources/*.swift)
if [ ${#SOURCES[@]} -eq 0 ]; then
    echo "No Swift sources found in $ROOT/Sources" >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$MACOS_DIR" "$RES_DIR"

cp "$ROOT/Info.plist" "$CONTENTS/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $VERSION" "$CONTENTS/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $VERSION" "$CONTENTS/Info.plist"

# Built-in skins and the AI guide are shared with other platforms in the repository's skins folder.
mkdir -p "$RES_DIR/skins"
cp "$ROOT/../skins/"*.meltycat "$RES_DIR/skins/"
cp "$ROOT/../skins/AI-GUIDE.md" "$RES_DIR/AI-GUIDE.md"

if [ -f "$ROOT/Resources/AppIcon.icns" ]; then
    cp "$ROOT/Resources/AppIcon.icns" "$RES_DIR/AppIcon.icns"
fi

FLAGS=(
    -O
    -swift-version 5
    -framework AppKit
    -framework CoreGraphics
    -framework QuartzCore
)

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "==> Compiling arm64"
swiftc "${FLAGS[@]}" -target "arm64-apple-macos$MIN_MACOS" "${SOURCES[@]}" -o "$TMP/${APP_NAME}_arm64"

echo "==> Compiling x86_64"
swiftc "${FLAGS[@]}" -target "x86_64-apple-macos$MIN_MACOS" "${SOURCES[@]}" -o "$TMP/${APP_NAME}_x86_64"

echo "==> Creating universal binary"
lipo -create "$TMP/${APP_NAME}_arm64" "$TMP/${APP_NAME}_x86_64" -output "$MACOS_DIR/$APP_NAME"
lipo -info "$MACOS_DIR/$APP_NAME"

# Apple Silicon refuses to run unsigned binaries, so sign ad hoc.
echo "==> Signing (ad hoc)"
codesign --force --deep --sign - --identifier "$BUNDLE_ID" "$APP"

echo "==> Built $APP"
