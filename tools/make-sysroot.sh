#!/bin/sh
# Build the edison-sysroot tool: the libgpiod headers and library sketches link against,
# taken from the Debian 13 packages edison-image ships. Needs only Docker.
#
#   tools/make-sysroot.sh [OUT_DIR]     (default: dist)
#
# Writes OUT_DIR/edison-sysroot/ and OUT_DIR/edison-sysroot.tar.gz.
set -eu

# Keep in step with edison-image (dpkg-query -W libgpiod3 on the board).
LIBGPIOD_VERSION=2.2.1-2+deb13u1

out=$(cd "$(dirname "$0")/.." && pwd)/${1:-dist}
mkdir -p "$out"
rm -rf "$out/edison-sysroot"

docker run --rm -v "$out:/out" -e LIBGPIOD_VERSION="$LIBGPIOD_VERSION" \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" debian:trixie sh -euc '
    apt-get update -qq
    cd /tmp
    apt-get download -qq "libgpiod-dev=$LIBGPIOD_VERSION" "libgpiod3=$LIBGPIOD_VERSION"
    for deb in *.deb; do dpkg-deb -x "$deb" root; done
    s=/out/edison-sysroot
    mkdir -p "$s/include" "$s/lib"
    cp root/usr/include/gpiod.h "$s/include/"
    # A plain file under the soname (no symlinks, which zip and Windows handle badly).
    cp root/usr/lib/x86_64-linux-gnu/libgpiod.so.3.* "$s/lib/libgpiod.so.3"
    cp root/usr/share/doc/libgpiod3/copyright "$s/COPYRIGHT.libgpiod"
    echo "libgpiod $LIBGPIOD_VERSION (Debian 13)" > "$s/VERSION"
    tar -C /out -czf /out/edison-sysroot.tar.gz edison-sysroot
    chown -R "$HOST_UID:$HOST_GID" /out/edison-sysroot /out/edison-sysroot.tar.gz
'
echo "built $out/edison-sysroot"
