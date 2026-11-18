#!/bin/sh
# Build and ad-hoc sign ios2kbd for arm64 iOS (on a Mac with Xcode).
# Output: build/ios2kbd, or the path given as $1.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${1:-"$here/build/ios2kbd"}
mkdir -p "$(dirname "$out")"
xcrun --sdk iphoneos clang -target arm64-apple-ios15.0 -std=c11 -O2 -Wall -Wextra -Werror \
    -framework IOKit -framework CoreFoundation \
    "$here/main.c" "$here/core.c" "$here/iokit.c" -o "$out"
codesign --force --sign - --entitlements "$here/ios2kbd.entitlements" "$out"
shasum -a 256 "$out"
