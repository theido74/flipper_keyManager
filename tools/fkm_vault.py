#!/usr/bin/env python3
"""Create / inspect a Fliiper Keys vault file (vault.fkm).

The on-disk format and the KDF here MUST stay identical to src/crypto.c.

    magic  "FKM1"          4 bytes
    salt                   16 bytes (random)
    iv                     16 bytes (random, AES-CTR initial counter)
    ciphertext             AES-256-CTR(plaintext)

    key = KDF(passphrase, salt)
    KDF: h = SHA256(salt || passphrase); repeat ITER times h = SHA256(h)

Plaintext is UTF-8, one entry per line:  domain:password
('#' comment lines and blank lines are ignored by the firmware.)

Usage:
    pip install cryptography
    ./fkm_vault.py encrypt accounts.txt vault.fkm
    ./fkm_vault.py decrypt vault.fkm          # prints plaintext to verify

Then copy vault.fkm to the SD card at:
    /ext/apps_data/fliiper_keys/vault.fkm

SECURITY: accounts.txt is plaintext. Keep it off shared storage, and
shred it when done (e.g. `shred -u accounts.txt`).
"""
import hashlib
import os
import sys
from getpass import getpass

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
except ImportError:
    sys.exit("Missing dependency: pip install cryptography")

MAGIC = b"FKM1"
SALT_LEN = 16
IV_LEN = 16
KEY_LEN = 32
KDF_ITERATIONS = 100_000  # keep in sync with FKM_KDF_ITERATIONS in crypto.h


def kdf(passphrase: str, salt: bytes) -> bytes:
    h = hashlib.sha256(salt + passphrase.encode("utf-8")).digest()
    for _ in range(KDF_ITERATIONS):
        h = hashlib.sha256(h).digest()
    return h[:KEY_LEN]


def _ctr(key: bytes, iv: bytes, data: bytes) -> bytes:
    cipher = Cipher(algorithms.AES(key), modes.CTR(iv))
    enc = cipher.encryptor()
    return enc.update(data) + enc.finalize()


def encrypt(src_path: str, dst_path: str) -> None:
    with open(src_path, "rb") as f:
        plaintext = f.read()

    p1 = getpass("Passphrase: ")
    p2 = getpass("Confirm:    ")
    if p1 != p2:
        sys.exit("Passphrases do not match.")
    if not p1:
        sys.exit("Empty passphrase refused.")

    salt = os.urandom(SALT_LEN)
    iv = os.urandom(IV_LEN)
    key = kdf(p1, salt)
    ciphertext = _ctr(key, iv, plaintext)

    with open(dst_path, "wb") as f:
        f.write(MAGIC + salt + iv + ciphertext)
    print(f"Wrote {dst_path} ({len(ciphertext)} bytes ciphertext).")


def decrypt(path: str) -> None:
    with open(path, "rb") as f:
        blob = f.read()
    if blob[:4] != MAGIC:
        sys.exit("Bad magic — not an FKM1 file.")
    salt = blob[4:20]
    iv = blob[20:36]
    ciphertext = blob[36:]

    key = kdf(getpass("Passphrase: "), salt)
    plaintext = _ctr(key, iv, ciphertext)
    try:
        sys.stdout.write(plaintext.decode("utf-8"))
    except UnicodeDecodeError:
        sys.exit("Decrypted bytes are not UTF-8 — wrong passphrase?")


def main() -> None:
    if len(sys.argv) >= 4 and sys.argv[1] == "encrypt":
        encrypt(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 3 and sys.argv[1] == "decrypt":
        decrypt(sys.argv[2])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
