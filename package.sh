#!/bin/sh
# Build ios2kbd and package it for release (on a Mac with Xcode and dpkg-deb).
#   ./package.sh VERSION [OUTPUT_DIR]
# Output: ios2kbd (the signed binary), ios2kbd_VERSION_iphoneos-arm64.deb (for
# rootless jailbreaks, installs /var/jb/usr/bin/ios2kbd) and SHA256SUMS.
set -eu
[ $# -ge 1 ] || { echo "usage: $0 VERSION [OUTPUT_DIR]" >&2; exit 64; }
version=$1
here=$(cd "$(dirname "$0")" && pwd)
out=${2:-"$here/build/release"}
case $version in
    [0-9]*) ;;
    *) echo "VERSION must start with a digit (got '$version')" >&2; exit 64 ;;
esac
rm -rf "$out"
mkdir -p "$out"
"$here/build.sh" "$out/ios2kbd" >/dev/null

root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
chmod 755 "$root"
mkdir -p "$root/DEBIAN" "$root/var/jb/usr/bin"
cp "$out/ios2kbd" "$root/var/jb/usr/bin/ios2kbd"
chmod 755 "$root/var/jb/usr/bin/ios2kbd"
cat > "$root/DEBIAN/control" <<CONTROL
Package: com.getboolean.ios2kbd
Name: ios2kbd
Version: $version
Architecture: iphoneos-arm64
Maintainer: getBoolean
Author: getBoolean
Section: Utilities
Homepage: https://github.com/getBoolean/ios2kbd
Description: Type text into another device as a USB keyboard
 Turns a jailbroken USB-C iPhone or iPad into a USB keyboard for the
 computer or console it is plugged into. Run: sudo ios2kbd TEXT
CONTROL
deb="$out/ios2kbd_${version}_iphoneos-arm64.deb"
dpkg-deb --root-owner-group -Zgzip --build "$root" "$deb" >/dev/null
(cd "$out" && shasum -a 256 ios2kbd "$(basename "$deb")" > SHA256SUMS)
cat "$out/SHA256SUMS"
