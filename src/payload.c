#include "payload.h"
#include <stdlib.h>
#include <string.h>

int pl_pack(const char *name, const uint8_t *data, size_t len, uint8_t **out, size_t *out_len) {
    size_t nl = name ? strlen(name) : 0;
    if (!out || !out_len || nl > PL_MAX_NAME || (!data && len)) return -1;
    uint8_t *b = malloc(1 + nl + len);
    if (!b) return -1;
    b[0] = (uint8_t)nl;
    if (nl) memcpy(b + 1, name, nl);
    if (len) memcpy(b + 1 + nl, data, len);
    *out = b;
    *out_len = 1 + nl + len;
    return 0;
}

int pl_unpack(const uint8_t *buf, size_t buf_len, char *name, const uint8_t **data, size_t *len) {
    if (!buf || !name || !data || !len || buf_len < 1) return -1;
    size_t nl = buf[0];
    if (buf_len < 1 + nl) return -1;
    memcpy(name, buf + 1, nl);
    name[nl] = 0;
    /* refuse les noms dangereux (chemins, octets nuls) */
    if (strlen(name) != nl || strchr(name, '/') || strchr(name, '\\') ||
        !strcmp(name, ".") || !strcmp(name, ".."))
        return -1;
    *data = buf + 1 + nl;
    *len = buf_len - 1 - nl;
    return 0;
}