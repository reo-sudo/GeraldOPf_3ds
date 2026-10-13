#!/usr/bin/env bash
# Gerald Generations Fusion / Nintendo 3DS CIA packaging tools
# Build makerom using the host toolchain so it links against this container's
# own glibc. The prebuilt Linux release may require GLIBC_2.38, while the
# devkitpro/devkitarm:20260610 image uses an older libc.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bin work

# -- makerom ---------------------------------------------------------------
# IMPORTANT: Do not fall back to the prebuilt release; its GLIBC requirement
# can exceed that of the 3DS SDK container. Use a pinned upstream source tag.
MAKEROM_TAG="makerom-v0.19.0"
MAKEROM_DIR="work/Project_CTR"
echo "== makerom: building ${MAKEROM_TAG} from source for this Linux container"
command -v git >/dev/null || { echo "ERROR: git is required to build makerom" >&2; exit 1; }
command -v gcc >/dev/null || { echo "ERROR: host gcc is required to build makerom" >&2; exit 1; }
command -v g++ >/dev/null || { echo "ERROR: host g++ is required to build makerom" >&2; exit 1; }
command -v make >/dev/null || { echo "ERROR: make is required to build makerom" >&2; exit 1; }
rm -f bin/makerom
rm -rf "$MAKEROM_DIR"
GIT_TERMINAL_PROMPT=0 git clone --depth 1 --single-branch --branch "$MAKEROM_TAG" \
  https://github.com/3DSGuy/Project_CTR.git "$MAKEROM_DIR"
# Project_CTR bundles its build dependencies in makerom/deps.
# Both commands MUST succeed; do not mask errors with `|| true`.
make -C "$MAKEROM_DIR/makerom" CC=gcc CXX=g++ deps
make -C "$MAKEROM_DIR/makerom" -j2 CC=gcc CXX=g++
test -s "$MAKEROM_DIR/makerom/bin/makerom" || {
  echo "ERROR: source build did not produce makerom/bin/makerom" >&2; exit 1;
}
install -m 755 "$MAKEROM_DIR/makerom/bin/makerom" bin/makerom
# This checks the Linux dynamic-loader dependencies before making a CIA.
# A GLIBC version error reports 'not found' here, even if a binary exists.
ldd bin/makerom 2>&1 | tee work/makerom-dependencies.log
if grep -Ei 'not found' work/makerom-dependencies.log; then
  echo "ERROR: locally-built makerom has unresolved runtime dependencies" >&2
  exit 1
fi
# makerom may exit nonzero on a help/empty command, so check for loader errors
# instead of treating an unsupported CLI option as a failure.
bin/makerom -v > work/makerom-probe.log 2>&1 || true
if grep -Ei 'GLIBC_[0-9.]+.*not found|error while loading shared libraries|symbol lookup error' work/makerom-probe.log; then
  cat work/makerom-probe.log >&2
  echo "ERROR: makerom cannot execute on this container" >&2
  exit 1
fi

# -- bannertool ------------------------------------------------------------
# The original upstream repository no longer supplies the expected binary.
# Pinned Epicpkmn11 release (the exact archive hash was verified previously).
BANNERTOOL_URL="${BANNERTOOL_URL:-https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip}"
BANNERTOOL_SHA256="${BANNERTOOL_SHA256:-e4259c08fe8944ebadd5f4b96f9a8603e5427338074cfc46323fbbc3410d51ed}"
if [ ! -x bin/bannertool ]; then
  echo "== bannertool: downloading pinned v1.2.2 Linux build"
  curl --fail --show-error --silent --location --retry 3 --retry-delay 2 \
    --connect-timeout 15 --max-time 180 -o work/bannertool-v1.2.2.zip "$BANNERTOOL_URL"
  printf '%s  %s\n' "$BANNERTOOL_SHA256" work/bannertool-v1.2.2.zip | sha256sum --check
  unzip -p work/bannertool-v1.2.2.zip linux-x86_64/bannertool > bin/bannertool
  chmod +x bin/bannertool
fi

for t in makerom bannertool; do
  [ -s "bin/$t" ] && [ -x "bin/$t" ] || {
    echo "ERROR: missing or non-executable bin/$t" >&2; exit 1;
  }
done
echo "makerom and bannertool ready"
