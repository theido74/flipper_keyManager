#pragma once

// Vault = the decrypted list of entries parsed from the plaintext.
//
// Plaintext line format (one entry per line, UTF-8, '\n' separated):
//
//     domain:password
//
// The first ':' splits domain from password, so a password may itself
// contain ':'. Lines that are empty or start with '#' are ignored.
// Only the domain is ever shown on screen; the password is typed over
// USB HID and never rendered.

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char* domain; // owned, NUL-terminated
    char* password; // owned, NUL-terminated
} VaultEntry;

typedef struct {
    VaultEntry* entries;
    size_t count;
} Vault;

// Parse decrypted plaintext (not NUL-terminated necessarily) into a Vault.
// Returns NULL on allocation failure. Caller frees with vault_free().
Vault* vault_parse(const uint8_t* plaintext, size_t len);

// Overwrites every password buffer with zeros, then frees everything.
void vault_free(Vault* vault);
