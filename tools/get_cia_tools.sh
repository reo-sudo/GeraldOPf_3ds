#!/usr/bin/env bash
# Puts `makerom` and `bannertool` into ./bin (the Makefile adds ./bin to PATH).
# Tries the latest GitHub release first; if that fails, builds from source.
# Used by the GitHub Actions workflow, but works on any Linux box too.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bin work
AUTH=()
[ -n "${GH_TOKEN:-}" ] && AUTH=(-H "Authorization: Bearer ${GH_TOKEN}")

asset_url() {  # $1 = owner/repo   $2 = regex matched against the asset download URL
  curl -fsSL "${AUTH[@]}" "https://api.github.com/repos/$1/releases/latest" \
    | jq -r --arg re "$2" '[.assets[].browser_download_url | select(test($re))][0] // empty'
}

fetch_zip() {  # $1 = url, $2 = binary name, $3 = path regex for `find`
  local url="$1" name="$2" hint="$3"
  [ -n "$url" ] || return 1
  rm -rf "work/$name" && mkdir -p "work/$name"
  curl -fsSL -o "work/$name.zip" "$url" && unzip -qo "work/$name.zip" -d "work/$name" || return 1
  local f
  f=$(find "work/$name" -type f -name "$name" | grep -E "$hint" | head -n1)
  [ -n "$f" ] || f=$(find "work/$name" -type f -name "$name" | head -n1)
  [ -n "$f" ] || return 1
  cp "$f" "bin/$name" && chmod +x "bin/$name"
}

# ---- makerom -------------------------------------------------------------
if [ ! -x bin/makerom ]; then
  echo "== makerom: trying release download"
  fetch_zip "$(asset_url 3DSGuy/Project_CTR 'ubuntu.*x86_64|linux.*x86_64')" makerom 'ubuntu|linux' \
  || {
    echo "== makerom: building from source"
    rm -rf work/Project_CTR
    git clone --depth 1 https://github.com/3DSGuy/Project_CTR work/Project_CTR \
      && ( cd work/Project_CTR/makerom && (make deps || true) && make -j"$(nproc)" ) \
      && cp work/Project_CTR/makerom/bin/makerom bin/makerom
  }
fi

# ---- bannertool ----------------------------------------------------------
# The old Steveice10/bannertool repository was deleted, so cloning it 404s.
# Epicpkmn11's v1.2.2 archive contains the 64-bit Linux executable.
# Pin the archive hash: do not execute an unverified third-party binary.
BANNERTOOL_URL="${BANNERTOOL_URL:-https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip}"
BANNERTOOL_SHA256="${BANNERTOOL_SHA256:-e4259c08fe8944ebadd5f4b96f9a8603e5427338074cfc46323fbbc3410d51ed}"
if [ ! -x bin/bannertool ]; then
  echo "== bannertool: downloading pinned v1.2.2 Linux build"
  curl --fail --show-error --silent --location --retry 3 --retry-delay 2 \
    --connect-timeout 15 --max-time 180 -o work/bannertool-v1.2.2.zip "$BANNERTOOL_URL" \
    || { echo "ERROR: Could not download bannertool from $BANNERTOOL_URL" >&2; exit 1; }
  printf '%s  %s\n' "$BANNERTOOL_SHA256" work/bannertool-v1.2.2.zip | sha256sum --check \
    || { echo "ERROR: bannertool archive SHA-256 mismatch. Stop rather than execute it." >&2; exit 1; }
  # -p extracts precisely this file. The archive is NOT a flat zip.
  unzip -p work/bannertool-v1.2.2.zip linux-x86_64/bannertool > bin/bannertool \
    || { rm -f bin/bannertool; echo "ERROR: Linux bannertool missing from archive." >&2; exit 1; }
  chmod +x bin/bannertool
fi
# Runs a real executable check. Missing system libraries are caught here.
bin/bannertool --version

ls -l bin
for t in makerom bannertool; do
  [ -x "bin/$t" ] || { echo "ERROR: could not obtain $t"; exit 1; }
done
echo "makerom + bannertool ready"
