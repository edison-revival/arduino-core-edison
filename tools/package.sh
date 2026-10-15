#!/bin/sh
# Package a release: the archives the Arduino Boards Manager and PlatformIO
# (edison-revival/platform-edison) install, and the Boards Manager index. Needs only Docker.
#
#   tools/package.sh VERSION [PREVIOUS_INDEX]
#
# Needs dist/edison-tools and dist/edison-sysroot (tools/build-tools.sh and
# tools/make-sysroot.sh). Downloads Zig into downloads/ and checks it against the checksums
# below; put local copies there if ziglang.org ever goes away. Writes dist/release/: the
# archives and package_edison-revival_index.json, which adds VERSION to PREVIOUS_INDEX (the
# index from the last release) if given.
#
# Every archive has one top-level folder (the Boards Manager strips it) holding a
# package.json (how PlatformIO finds a package's root and checks its host).
set -eu

ZIG_VERSION=0.17.0
DEBIAN_IMAGE=debian:trixie
REPO_URL=https://github.com/edison-revival/arduino-core-edison
PACKAGER=edison-revival

# Our name, Zig's name, the Boards Manager's host, PlatformIO's system type; then the Zig
# archive's SHA256 (https://ziglang.org/download/index.json).
HOSTS='
linux-amd64   x86_64-linux   x86_64-linux-gnu    linux_x86_64  1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026
linux-arm64   aarch64-linux  aarch64-linux-gnu   linux_aarch64 9e8d11661d4ae3bd57702a3832781e23ad151dde5798e16a5ccd503f65234ff8
darwin-amd64  x86_64-macos   x86_64-apple-darwin darwin_x86_64 4f9a1c5269aa17ebda5e6d3c2b89d6cbf36f7d2b22a0306e9ab98f25f95529c6
darwin-arm64  aarch64-macos  arm64-apple-darwin  darwin_arm64  b607e9b9234790a008116ae5bdb71c6243b84b9fb42a53a9e70fde41c06c536a
windows-amd64 x86_64-windows x86_64-mingw32      windows_amd64 b5663f69581dcf391293fbf16c06cb80d81d806545ce618b4d0bab7f0eb8c428
'

[ $# -ge 1 ] || { echo "usage: tools/package.sh VERSION [PREVIOUS_INDEX]" >&2; exit 2; }
version=${1#v}
root=$(cd "$(dirname "$0")/.." && pwd)

# On the host: rerun this script in a container.
if [ -z "${IN_CONTAINER:-}" ]; then
    prev=
    if [ $# -ge 2 ]; then
        cp "$2" "$root/dist/previous-index.json"
        prev=/src/dist/previous-index.json
    fi
    epoch=$(git -C "$root" log -1 --format=%ct 2>/dev/null || echo 0)
    docker run --rm -v "$root:/src" -e IN_CONTAINER=1 -e SOURCE_DATE_EPOCH="$epoch" \
        -e DEBIAN_FRONTEND=noninteractive \
        -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" "$DEBIAN_IMAGE" \
        sh /src/tools/package.sh "$version" $prev
    rm -f "$root/dist/previous-index.json"
    echo "packaged $root/dist/release"
    exit
fi

apt-get update -qq
apt-get install -qq -y --no-install-recommends ca-certificates curl jq unzip xz-utils >/dev/null

grep -qx "version=$version" /src/platform.txt ||
    { echo "platform.txt's version isn't $version" >&2; exit 1; }
[ -d /src/dist/edison-tools ] && [ -d /src/dist/edison-sysroot ] ||
    { echo "run tools/build-tools.sh and tools/make-sysroot.sh first" >&2; exit 1; }

out=/src/dist/release
stage=/tmp/stage
rm -rf "$out" "$stage"
mkdir -p "$out" "$stage" /src/downloads
url="$REPO_URL/releases/download/v$version"

# package_json NAME VERSION DESCRIPTION LICENSE [PIO_SYSTEM] > package.json
package_json() {
    jq -n --arg name "$1" --arg version "$2" --arg description "$3" --arg license "$4" \
        --arg system "${5:-}" --arg homepage "$REPO_URL" '
        {name: $name, version: $version, description: $description, license: $license,
         homepage: $homepage} + (if $system == "" then {} else {system: [$system]} end)'
}

# archive DIR NAME: tar DIR's contents under the folder NAME, reproducibly, into
# $out/NAME.tar.xz, and print the Boards Manager's fields for it.
archive() {
    file=$2.tar.xz
    tar -C "$1" --transform "s,^\.,$2," --sort=name --owner=0 --group=0 --numeric-owner \
        --mtime="@$SOURCE_DATE_EPOCH" -cf - . | xz -T0 -9 >"$out/$file"
    jq -n --arg url "$url/$file" --arg file "$file" \
        --arg sha "$(sha256sum "$out/$file" | cut -d' ' -f1)" \
        --arg size "$(stat -c %s "$out/$file")" \
        '{url: $url, archiveFileName: $file, checksum: ("SHA-256:" + $sha), size: $size}'
}

# The core: what the Boards Manager installs as the platform, and PlatformIO's framework.
mkdir "$stage/core"
cp -r /src/boards.txt /src/platform.txt /src/cores /src/variants /src/LICENSE \
    /src/README.md "$stage/core/"
package_json framework-arduino-edison "$version" "Arduino core for the Intel Edison" \
    LGPL-2.1-or-later >"$stage/core/package.json"
core=$(archive "$stage/core" "edison-core-$version")

# The sysroot is the same on every host.
mkdir "$stage/sysroot"
cp -r /src/dist/edison-sysroot/. "$stage/sysroot/"
package_json tool-edison-sysroot "$version" \
    "libgpiod headers and library for sketches ($(cat /src/dist/edison-sysroot/VERSION))" \
    LGPL-2.1-or-later >"$stage/sysroot/package.json"
sysroot=$(archive "$stage/sysroot" "edison-sysroot-$version")

echo "$HOSTS" | while read -r host zig arduino pio sha; do
    [ -n "$host" ] || continue

    # Zig, repackaged with a package.json.
    case $zig in *windows*) ext=zip ;; *) ext=tar.xz ;; esac
    src=zig-$zig-$ZIG_VERSION.$ext
    [ -f "/src/downloads/$src" ] ||
        curl -fsSL -o "/src/downloads/$src" "https://ziglang.org/download/$ZIG_VERSION/$src"
    echo "$sha  /src/downloads/$src" | sha256sum -c - >/dev/null ||
        { echo "checksum mismatch: downloads/$src" >&2; exit 1; }
    mkdir "$stage/zig-$host"
    case $ext in
        zip) unzip -q "/src/downloads/$src" -d "$stage/zig-$host" ;;
        *) tar -C "$stage/zig-$host" -xJf "/src/downloads/$src" ;;
    esac
    z=$stage/zig-$host/zig-$zig-$ZIG_VERSION
    package_json toolchain-zig "$ZIG_VERSION" "Zig $ZIG_VERSION ($zig), the compiler for sketches" \
        MIT "$pio" >"$z/package.json"
    entry=$(archive "$z" "edison-zig-$ZIG_VERSION-$host")
    echo "$entry" | jq --arg host "$arduino" '{host: $host} + .' >>"$stage/zig.json"
    rm -rf "$stage/zig-$host"

    # edison-upload and edison-cc.
    mkdir "$stage/tools-$host"
    cp /src/dist/edison-tools/"$host"/* "$stage/tools-$host/"
    chmod 755 "$stage/tools-$host"/*
    package_json tool-edison "$version" "edison-upload and edison-cc" LGPL-2.1-or-later \
        "$pio" >"$stage/tools-$host/package.json"
    entry=$(archive "$stage/tools-$host" "edison-tools-$version-$host")
    echo "$entry" | jq --arg host "$arduino" '{host: $host} + .' >>"$stage/tools.json"

    echo "$sysroot" | jq --arg host "$arduino" '{host: $host} + .' >>"$stage/sysroot.json"
    echo "packaged $host" >&2
done

platform=$(echo "$core" | jq --arg version "$version" --arg zig "$ZIG_VERSION" \
    --arg packager "$PACKAGER" --arg help "$REPO_URL" '
    {name: "Intel Edison (edison-revival)", architecture: "edison", version: $version,
     category: "Contributed", help: {online: $help}} + . + {
     boards: [{name: "Intel Edison (Kit for Arduino)"}],
     toolsDependencies: [
       {packager: $packager, name: "edison-zig", version: $zig},
       {packager: $packager, name: "edison-sysroot", version: $version},
       {packager: $packager, name: "edison-tools", version: $version}]}')
tools=$(jq -n --arg version "$version" --arg zig "$ZIG_VERSION" \
    --slurpfile z "$stage/zig.json" --slurpfile t "$stage/tools.json" \
    --slurpfile s "$stage/sysroot.json" '[
    {name: "edison-zig", version: $zig, systems: $z},
    {name: "edison-sysroot", version: $version, systems: $s},
    {name: "edison-tools", version: $version, systems: $t}]')

# Add this version to the previous index. A tool version already listed keeps its entry, so
# its checksum doesn't change under anyone who has it installed.
prev=${2:-/dev/null}
[ -s "$prev" ] || { prev=$stage/empty.json; echo '{"packages": []}' >"$prev"; }
jq --argjson platform "$platform" --argjson tools "$tools" --arg packager "$PACKAGER" \
    --arg site "$REPO_URL" '
    (.packages[0] // {}) as $p
    | ($p.tools // []) as $old
    | {packages: [{
        name: $packager, maintainer: $packager, websiteURL: $site, email: "",
        help: {online: $site},
        platforms: ([($p.platforms // [])[] | select(.version != $platform.version)]
                    + [$platform]),
        tools: ($old + [$tools[] | select(. as $t
                  | $old | any(.name == $t.name and .version == $t.version) | not)])
      }]}' "$prev" >"$out/package_${PACKAGER}_index.json"

(cd "$out" && sha256sum -- *.tar.xz >SHA256SUMS)
chown -R "$HOST_UID:$HOST_GID" "$out"
