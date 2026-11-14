#include "vault.h"

#include <furi.h>
#include <string.h>

#define FKM_TAG "FliiperKeys"

// Duplicate [start, end) into a fresh NUL-terminated heap string.
static char* dup_range(const char* start, const char* end) {
    size_t n = (size_t)(end - start);
    char* s = malloc(n + 1);
    if(!s) return NULL;
    memcpy(s, start, n);
    s[n] = '\0';
    return s;
}

Vault* vault_parse(const uint8_t* plaintext, size_t len) {
    Vault* vault = malloc(sizeof(Vault));
    if(!vault) return NULL;
    vault->entries = NULL;
    vault->count = 0;

    const char* p = (const char*)plaintext;
    const char* const end = p + len;

    while(p < end) {
        // Find end of this line.
        const char* nl = memchr(p, '\n', (size_t)(end - p));
        const char* line_end = nl ? nl : end;

        // Trim a trailing '\r' (CRLF files).
        const char* real_end = line_end;
        if(real_end > p && real_end[-1] == '\r') real_end--;

        // Skip blank lines and comments.
        if(real_end > p && *p != '#') {
            const char* colon = memchr(p, ':', (size_t)(real_end - p));
            if(colon) {
                char* domain = dup_range(p, colon);
                char* password = dup_range(colon + 1, real_end);
                if(domain && password) {
                    VaultEntry* grown =
                        realloc(vault->entries, sizeof(VaultEntry) * (vault->count + 1));
                    if(grown) {
                        vault->entries = grown;
                        vault->entries[vault->count].domain = domain;
                        vault->entries[vault->count].password = password;
                        vault->count++;
                    } else {
                        free(domain);
                        free(password);
                    }
                } else {
                    free(domain);
                    free(password);
                }
            } else {
                FURI_LOG_W(FKM_TAG, "line without ':' ignored");
            }
        }

        if(!nl) break;
        p = nl + 1;
    }

    return vault;
}

void vault_free(Vault* vault) {
    if(!vault) return;
    for(size_t i = 0; i < vault->count; i++) {
        free(vault->entries[i].domain);
        if(vault->entries[i].password) {
            // Wipe secret material before releasing.
            memset(vault->entries[i].password, 0, strlen(vault->entries[i].password));
            free(vault->entries[i].password);
        }
    }
    free(vault->entries);
    free(vault);
}
