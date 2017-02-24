#ifndef SPECK_ENCRYPT_H
#define SPECK_ENCRYPT_H

void __attribute__((naked)) Encrypt(uint8_t *block, uint8_t *roundKeys);

#endif
