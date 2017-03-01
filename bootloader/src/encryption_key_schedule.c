/*
 *
 * University of Luxembourg
 * Laboratory of Algorithmics, Cryptology and Security (LACS)
 *
 * FELICS - Fair Evaluation of Lightweight Cryptographic Systems
 *
 * Copyright (C) 2015 University of Luxembourg
 *
 * Written in 2015 by Yann Le Corre <yann.lecorre@uni.lu>,
 *                    Jason Smith <jksmit3@tycho.ncsc.mil>,
 *                    Bryan Weeks <beweeks@tycho.ncsc.mil>
 *
 * This file is part of FELICS.
 *
 * FELICS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * FELICS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>

#include "cipher.h"
#include "rot32.h"
#include "constants.h"

void RunEncryptionKeySchedule(uint8_t *key, uint8_t *roundKeys)
{
  uint8_t i;
  uint8_t z_xor_3;
  uint32_t tmp;
  uint32_t *mk = (uint32_t *)key;
  uint32_t *rk = (uint32_t *)roundKeys;
  uint8_t z_xor_3_array[44];
  z_xor_3_array[0] = 2;
  z_xor_3_array[1] = 2;
  z_xor_3_array[2] = 3;
  z_xor_3_array[3] = 2;
  z_xor_3_array[4] = 2;
  z_xor_3_array[5] = 3;
  z_xor_3_array[6] = 2;
  z_xor_3_array[7] = 2;
  z_xor_3_array[8] = 2;
  z_xor_3_array[9] = 3;
  z_xor_3_array[10] = 2;
  z_xor_3_array[11] = 3;
  z_xor_3_array[12] = 2;
  z_xor_3_array[13] = 2;
  z_xor_3_array[14] = 3;
  z_xor_3_array[15] = 3;
  z_xor_3_array[16] = 3;
  z_xor_3_array[17] = 2;
  z_xor_3_array[18] = 2;
  z_xor_3_array[19] = 3;
  z_xor_3_array[20] = 3;
  z_xor_3_array[21] = 2;
  z_xor_3_array[22] = 3;
  z_xor_3_array[23] = 2;
  z_xor_3_array[24] = 2;
  z_xor_3_array[25] = 2;
  z_xor_3_array[26] = 2;
  z_xor_3_array[27] = 3;
  z_xor_3_array[28] = 3;
  z_xor_3_array[29] = 3;
  z_xor_3_array[30] = 3;
  z_xor_3_array[31] = 3;
  z_xor_3_array[32] = 3;
  z_xor_3_array[33] = 2;
  z_xor_3_array[34] = 3;
  z_xor_3_array[35] = 3;
  z_xor_3_array[36] = 2;
  z_xor_3_array[37] = 3;
  z_xor_3_array[38] = 3;
  z_xor_3_array[39] = 3;
  z_xor_3_array[40] = 2;
  z_xor_3_array[41] = 3;
  z_xor_3_array[42] = 2;
  z_xor_3_array[43] = 3;
  rk[0] = mk[0];
  rk[1] = mk[1];
  rk[2] = mk[2];
  rk[3] = mk[3];

  for (i = 4; i < NUMBER_OF_ROUNDS; ++i) {

    tmp  = rot32r3(rk[i - 1]) ^ rk[i - 3];
    tmp ^= rot32r1(tmp);

    z_xor_3 = z_xor_3_array[i - 4];

    rk[i] = ~(rk[i - 4]) ^ tmp ^ (uint32_t)z_xor_3;
  }
}
