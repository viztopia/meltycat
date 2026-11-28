#!/usr/bin/env bash
# Renders the cat into bitmaps, checks behaviour and compares built-in skins with Tests/golden.
# Usage: bash macos/test.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

FILES=()
for name in CatRenderer Animation Placement Cadence Localization MenuIcon SkinSpec SkinLibrary Poof Updates; do
    FILES+=("$ROOT/Sources/$name.swift")
done
swiftc -swift-version 5 -framework CoreGraphics -framework ImageIO "${FILES[@]}" "$ROOT"/Tests/*.swift -o "$TMP/tests"
"$TMP/tests" "$ROOT/../skins" "$ROOT/Tests/golden"
