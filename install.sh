#!/bin/sh
# Build ios2kbd and install it on a jailbroken iPhone or iPad (rootless /var/jb) over SSH.
#   ./install.sh root@<device-address> [extra ssh options...]
# Copies to a temporary name first and renames it into place, so a failed
# transfer never leaves a broken command.
set -eu
[ $# -ge 1 ] || { echo "usage: $0 root@<device-address> [ssh options...]" >&2; exit 64; }
target=$1; shift
here=$(cd "$(dirname "$0")" && pwd)
binary="$here/build/ios2kbd"
"$here/build.sh" "$binary" >/dev/null
ssh "$@" "$target" 'cat > /var/jb/usr/bin/ios2kbd.incoming &&
    chmod 755 /var/jb/usr/bin/ios2kbd.incoming &&
    mv -f /var/jb/usr/bin/ios2kbd.incoming /var/jb/usr/bin/ios2kbd &&
    echo "installed /var/jb/usr/bin/ios2kbd"' < "$binary"
