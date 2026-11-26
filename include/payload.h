#ifndef PAYLOAD_H
#define PAYLOAD_H
#include <stddef.h>
#include <stdint.h>

#define PL_MAX_NAME 255

/* Charge utile : longueur du nom (1) | nom | données. Nom vide = message texte. */
int pl_pack(const char *name, const uint8_t *data, size_t len, uint8_t **out, size_t *out_len);
/* name : tampon de PL_MAX_NAME+1 octets. *data pointe dans buf. 0 = ok, -1 = invalide. */
int pl_unpack(const uint8_t *buf, size_t buf_len, char *name, const uint8_t **data, size_t *len);

#endif