/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the SEED code.
 *
 * The Initial Developer of the Original Code is
 * KISA(Korea Information Security Agency).
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer.
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef HEADER_SEED_H
#define HEADER_SEED_H

#include <string.h>
#include "blapi.h"

#if !defined(NO_SYS_TYPES_H)
# include <sys/types.h>
#endif

typedef PRUint32 seed_word;

#define G_FUNC(v) \
    SS[0][((v)     & 0xff)] ^ \
    SS[1][((v)>> 8 & 0xff)] ^ \
    SS[2][((v)>>16 & 0xff)] ^ \
    SS[3][((v)>>24 & 0xff)]

#define char2word(c, i)  \
    (i) = ((((seed_word)((c)[0])) << 24) | \
           (((seed_word)((c)[1])) << 16) | \
           (((seed_word)((c)[2])) <<  8) | \
            ((seed_word)((c)[3])))

#define word2char(l, c)  \
    *((c)+0) = (unsigned char)((l)>>24); \
    *((c)+1) = (unsigned char)((l)>>16); \
    *((c)+2) = (unsigned char)((l)>> 8); \
    *((c)+3) = (unsigned char)((l)    )

#define KEYSCHEDULE_UPDATE0(T0, T1, K0, K1, K2, K3, KC)  \
    (T0) = (K2);                                          \
    (K2) = (((K2)<<8) ^ ((K3)>>24));                     \
    (K3) = (((K3)<<8) ^ ((T0)>>24));                     \
    (T0) = ((K0) + (K2) - (KC));                         \
    (T1) = ((K1) + (KC) - (K3))

#define KEYSCHEDULE_UPDATE1(T0, T1, K0, K1, K2, K3, KC) \
    (T0) = (K0);                                         \
    (K0) = (((K0)>>8) ^ ((K1)<<24));                    \
    (K1) = (((K1)>>8) ^ ((T0)<<24));                    \
    (T0) = ((K0) + (K2) - (KC));                         \
    (T1) = ((K1) + (KC) - (K3))

#define KEYUPDATE_TEMP(T0, T1, K)   \
    (K)[0] = G_FUNC((T0));          \
    (K)[1] = G_FUNC((T1))

#define XOR_SEEDBLOCK(DST, SRC)  \
    (DST)[0] ^= (SRC)[0];    \
    (DST)[1] ^= (SRC)[1];    \
    (DST)[2] ^= (SRC)[2];    \
    (DST)[3] ^= (SRC)[3]

#define MOV_SEEDBLOCK(DST, SRC)  \
    (DST)[0] = (SRC)[0];     \
    (DST)[1] = (SRC)[1];     \
    (DST)[2] = (SRC)[2];     \
    (DST)[3] = (SRC)[3]

# define CHAR2WORD(C, I)          \
    char2word((C),    (I)[0]);    \
    char2word((C)+4,  (I)[1]);    \
    char2word((C)+8,  (I)[2]);    \
    char2word((C)+12, (I)[3])

# define WORD2CHAR(I, C)          \
    word2char((I)[0], (C));       \
    word2char((I)[1], (C+4));     \
    word2char((I)[2], (C+8));     \
    word2char((I)[3], (C+12))

# define E_SEED(T0, T1, X1, X2, X3, X4, rbase)  \
    (T0)  = (X3) ^ (ks->data)[(rbase)];         \
    (T1)  = (X4) ^ (ks->data)[(rbase)+1];       \
    (T1) ^= (T0);       \
    (T1)  = G_FUNC(T1); \
    (T0) += (T1);       \
    (T0)  = G_FUNC(T0); \
    (T1) += (T0);       \
    (T1)  = G_FUNC(T1); \
    (T0) += (T1);       \
    (X1) ^= (T0);       \
    (X2) ^= (T1)


#ifdef  __cplusplus
extern "C" {
#endif

typedef struct seed_key_st {
    PRUint32 data[32];
} SEED_KEY_SCHEDULE;



struct SEEDContextStr {
    unsigned char iv[SEED_BLOCK_SIZE];
    SEED_KEY_SCHEDULE ks;
    int mode;
    unsigned int encrypt;
};

void SEED_set_key(const unsigned char rawkey[SEED_KEY_LENGTH], 
                  SEED_KEY_SCHEDULE *ks);

void SEED_encrypt(const unsigned char s[SEED_BLOCK_SIZE], 
                  unsigned char d[SEED_BLOCK_SIZE], 
                  const SEED_KEY_SCHEDULE *ks);
void SEED_decrypt(const unsigned char s[SEED_BLOCK_SIZE], 
                  unsigned char d[SEED_BLOCK_SIZE], 
                  const SEED_KEY_SCHEDULE *ks);

void SEED_ecb_encrypt(const unsigned char *in, unsigned char *out, 
                      const SEED_KEY_SCHEDULE *ks, int enc);
void SEED_cbc_encrypt(const unsigned char *in, unsigned char *out, 
                      size_t len, const SEED_KEY_SCHEDULE *ks, 
                      unsigned char ivec[SEED_BLOCK_SIZE], int enc);

#ifdef  __cplusplus
}
#endif

#endif /* HEADER_SEED_H */
