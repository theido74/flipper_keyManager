#!/usr/bin/env bash
# Fetch the two vendored, public-domain crypto primitives into src/vendor/.
# Run once from the project root:  ./tools/fetch_deps.sh
set -euo pipefail

DEST="$(dirname "$0")/../src/vendor"
mkdir -p "$DEST"

# kokke/tiny-AES-c  (Unlicense / public domain) — AES-256-CTR
AES_BASE="https://raw.githubusercontent.com/kokke/tiny-AES-c/master"
curl -fSL "$AES_BASE/aes.h" -o "$DEST/aes.h"
curl -fSL "$AES_BASE/aes.c" -o "$DEST/aes.c"

# B-Con/crypto-algorithms  (public domain) — SHA-256
SHA_BASE="https://raw.githubusercontent.com/B-Con/crypto-algorithms/master"
curl -fSL "$SHA_BASE/sha256.h" -o "$DEST/sha256.h"
curl -fSL "$SHA_BASE/sha256.c" -o "$DEST/sha256.c"

echo "Vendored deps in $DEST:"
ls -1 "$DEST"
echo
echo "Sanity check (optional): the SHA-256 of \"abc\" must be"
echo "  ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
