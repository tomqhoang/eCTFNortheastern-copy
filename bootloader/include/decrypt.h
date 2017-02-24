#ifndef SPECK_DECRYPT_H
#define SPECK_DECRYPT_H

void __attribute__((naked)) Decrypt(uint8_t *block, uint8_t *roundKeys);

#endif
