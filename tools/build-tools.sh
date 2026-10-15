#!/bin/sh
# Test edison-upload and edison-cc and build them for every host the Arduino IDE runs on.
# Needs only Docker.
#
#   tools/build-tools.sh [OUT_DIR]     (default: dist)
#
# Writes OUT_DIR/edison-tools/<os>-<arch>/{edison-upload,edison-cc}[.exe].
set -eu

GO_IMAGE=golang:1.27.2

root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-dist}

docker run --rm -v "$root:/src" -w /src/tools -e OUT="/src/$out" \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" "$GO_IMAGE" sh -euc '
    for tool in edison-upload edison-cc; do
        cd "/src/tools/$tool"
        unformatted=$(gofmt -l .)
        [ -z "$unformatted" ] || { echo "not gofmt-formatted: $unformatted" >&2; exit 1; }
        go vet ./...
        go test ./...
    done
    for target in linux/amd64 linux/arm64 windows/amd64 darwin/amd64 darwin/arm64; do
        os=${target%/*} arch=${target#*/}
        ext=; [ "$os" = windows ] && ext=.exe
        dest="$OUT/edison-tools/$os-$arch"
        mkdir -p "$dest"
        for tool in edison-upload edison-cc; do
            cd "/src/tools/$tool"
            CGO_ENABLED=0 GOOS=$os GOARCH=$arch go build -trimpath -buildvcs=false \
                -ldflags="-s -w" -o "$dest/$tool$ext" .
        done
        echo "built $dest"
    done
    chown -R "$HOST_UID:$HOST_GID" "$OUT/edison-tools"
'
