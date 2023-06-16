/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "secitem.h"
#include "blapi.h"
#include "nssutil.h"
#include "secerr.h"
#include "secder.h"
#include "secdig.h"
#include "secoid.h"
#include "ec.h"
#include "hasht.h"
#include "lowkeyi.h"
#include "softoken.h"
#include "pkcs11t.h"
#define __PASTE(x, y) x##y
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST
#define CK_EXTERN extern
#define CK_PKCS11_FUNCTION_INFO(func) \
    CK_RV __PASTE(NS, func)
#define CK_NEED_ARG_LIST 1
#include "pkcs11f.h"
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST
#undef __PASTE
#define SSL3_RANDOM_LENGTH 32

#if 0
#include "../../lib/freebl/mpi/mpi.h"
#endif
#define MATCH_OPENSSL 1
/*#define MATCH_NIST 1 */
#ifdef MATCH_NIST
#define VERBOSE_REASON 1
#endif

extern SECStatus
EC_DecodeParams(const SECItem *encodedParams, ECParams **ecparams);
extern SECStatus
EC_CopyParams(PLArenaPool *arena, ECParams *dstParams,
              const ECParams *srcParams);

#define ENCRYPT 1
#define DECRYPT 0
#define BYTE unsigned char
#define DEFAULT_RSA_PUBLIC_EXPONENT 0x10001
#define RSA_MAX_TEST_MODULUS_BITS 4096
#define RSA_MAX_TEST_MODULUS_BYTES RSA_MAX_TEST_MODULUS_BITS / 8
#define RSA_MAX_TEST_EXPONENT_BYTES 8
#define PQG_TEST_SEED_BYTES 20

SECStatus
hex_to_byteval(const char *c2, unsigned char *byteval)
{
    int i;
    unsigned char offset;
    *byteval = 0;
    for (i = 0; i < 2; i++) {
        if (c2[i] >= '0' && c2[i] <= '9') {
            offset = c2[i] - '0';
            *byteval |= offset << 4 * (1 - i);
        } else if (c2[i] >= 'a' && c2[i] <= 'f') {
            offset = c2[i] - 'a';
            *byteval |= (offset + 10) << 4 * (1 - i);
        } else if (c2[i] >= 'A' && c2[i] <= 'F') {
            offset = c2[i] - 'A';
            *byteval |= (offset + 10) << 4 * (1 - i);
        } else {
            return SECFailure;
        }
    }
    return SECSuccess;
}

SECStatus
byteval_to_hex(unsigned char byteval, char *c2, char a)
{
    int i;
    unsigned char offset;
    for (i = 0; i < 2; i++) {
        offset = (byteval >> 4 * (1 - i)) & 0x0f;
        if (offset < 10) {
            c2[i] = '0' + offset;
        } else {
            c2[i] = a + offset - 10;
        }
    }
    return SECSuccess;
}

void
to_hex_str(char *str, const unsigned char *buf, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
        byteval_to_hex(buf[i], &str[2 * i], 'a');
    }
    str[2 * len] = '\0';
}

void
to_hex_str_cap(char *str, const unsigned char *buf, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
        byteval_to_hex(buf[i], &str[2 * i], 'A');
    }
    str[2 * len] = '\0';
}

/*
 * Convert a string of hex digits (str) to an array (buf) of len bytes.
 * Return PR_TRUE if the hex string can fit in the byte array.  Return
 * PR_FALSE if the hex string is empty or is too long.
 */
PRBool
from_hex_str(unsigned char *buf, unsigned int len, const char *str)
{
    unsigned int nxdigit; /* number of hex digits in str */
    unsigned int i;       /* index into buf */
    unsigned int j;       /* index into str */

    /* count the hex digits */
    nxdigit = 0;
    for (nxdigit = 0; isxdigit(str[nxdigit]); nxdigit++) {
        /* empty body */
    }
    if (nxdigit == 0) {
        return PR_FALSE;
    }
    if (nxdigit > 2 * len) {
        /*
         * The input hex string is too long, but we allow it if the
         * extra digits are leading 0's.
         */
        for (j = 0; j < nxdigit - 2 * len; j++) {
            if (str[j] != '0') {
                return PR_FALSE;
            }
        }
        /* skip leading 0's */
        str += nxdigit - 2 * len;
        nxdigit = 2 * len;
    }
    for (i = 0, j = 0; i < len; i++) {
        if (2 * i < 2 * len - nxdigit) {
            /* Handle a short input as if we padded it with leading 0's. */
            if (2 * i + 1 < 2 * len - nxdigit) {
                buf[i] = 0;
            } else {
                char tmp[2];
                tmp[0] = '0';
                tmp[1] = str[j];
                hex_to_byteval(tmp, &buf[i]);
                j++;
            }
        } else {
            hex_to_byteval(&str[j], &buf[i]);
            j += 2;
        }
    }
    return PR_TRUE;
}

SECStatus
tdea_encrypt_buf(
    int mode,
    const unsigned char *key,
    const unsigned char *iv,
    unsigned char *output, unsigned int *outputlen, unsigned int maxoutputlen,
    const unsigned char *input, unsigned int inputlen)
{
    SECStatus rv = SECFailure;
    DESContext *cx;
    unsigned char doublecheck[8 * 20]; /* 1 to 20 blocks */
    unsigned int doublechecklen = 0;

    cx = DES_CreateContext(key, iv, mode, PR_TRUE);
    if (cx == NULL) {
        goto loser;
    }
    rv = DES_Encrypt(cx, output, outputlen, maxoutputlen, input, inputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (*outputlen != inputlen) {
        goto loser;
    }
    DES_DestroyContext(cx, PR_TRUE);
    cx = NULL;

    /*
     * Doublecheck our result by decrypting the ciphertext and
     * compare the output with the input plaintext.
     */
    cx = DES_CreateContext(key, iv, mode, PR_FALSE);
    if (cx == NULL) {
        goto loser;
    }
    rv = DES_Decrypt(cx, doublecheck, &doublechecklen, sizeof doublecheck,
                     output, *outputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (doublechecklen != *outputlen) {
        goto loser;
    }
    DES_DestroyContext(cx, PR_TRUE);
    cx = NULL;
    if (memcmp(doublecheck, input, inputlen) != 0) {
        goto loser;
    }
    rv = SECSuccess;

loser:
    if (cx != NULL) {
        DES_DestroyContext(cx, PR_TRUE);
    }
    return rv;
}

SECStatus
tdea_decrypt_buf(
    int mode,
    const unsigned char *key,
    const unsigned char *iv,
    unsigned char *output, unsigned int *outputlen, unsigned int maxoutputlen,
    const unsigned char *input, unsigned int inputlen)
{
    SECStatus rv = SECFailure;
    DESContext *cx;
    unsigned char doublecheck[8 * 20]; /* 1 to 20 blocks */
    unsigned int doublechecklen = 0;

    cx = DES_CreateContext(key, iv, mode, PR_FALSE);
    if (cx == NULL) {
        goto loser;
    }
    rv = DES_Decrypt(cx, output, outputlen, maxoutputlen,
                     input, inputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (*outputlen != inputlen) {
        goto loser;
    }
    DES_DestroyContext(cx, PR_TRUE);
    cx = NULL;

    /*
     * Doublecheck our result by encrypting the plaintext and
     * compare the output with the input ciphertext.
     */
    cx = DES_CreateContext(key, iv, mode, PR_TRUE);
    if (cx == NULL) {
        goto loser;
    }
    rv = DES_Encrypt(cx, doublecheck, &doublechecklen, sizeof doublecheck,
                     output, *outputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (doublechecklen != *outputlen) {
        goto loser;
    }
    DES_DestroyContext(cx, PR_TRUE);
    cx = NULL;
    if (memcmp(doublecheck, input, inputlen) != 0) {
        goto loser;
    }
    rv = SECSuccess;

loser:
    if (cx != NULL) {
        DES_DestroyContext(cx, PR_TRUE);
    }
    return rv;
}

/*
 * Perform the TDEA Known Answer Test (KAT) or Multi-block Message
 * Test (MMT) in ECB or CBC mode.  The KAT (there are five types)
 * and MMT have the same structure: given the key and IV (CBC mode
 * only), encrypt the given plaintext or decrypt the given ciphertext.
 * So we can handle them the same way.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
tdea_kat_mmt(char *reqfn)
{
    char buf[180]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "CIPHERTEXT = <180 hex digits>\n".
                         */
    FILE *req;     /* input stream from the REQUEST file */
    FILE *resp;    /* output stream to the RESPONSE file */
    int i, j;
    int mode = NSS_DES_EDE3; /* NSS_DES_EDE3 (ECB) or NSS_DES_EDE3_CBC */
    int crypt = DECRYPT;     /* 1 means encrypt, 0 means decrypt */
    unsigned char key[24];   /* TDEA 3 key bundle */
    unsigned int numKeys = 0;
    unsigned char iv[8];             /* for all modes except ECB */
    unsigned char plaintext[8 * 20]; /* 1 to 20 blocks */
    unsigned int plaintextlen;
    unsigned char ciphertext[8 * 20]; /* 1 to 20 blocks */
    unsigned int ciphertextlen;
    SECStatus rv;

    req = fopen(reqfn, "r");
    resp = stdout;
    while (fgets(buf, sizeof buf, req) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, resp);
            continue;
        }
        /* [ENCRYPT] or [DECRYPT] */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "ENCRYPT", 7) == 0) {
                crypt = ENCRYPT;
            } else {
                crypt = DECRYPT;
            }
            fputs(buf, resp);
            continue;
        }
        /* NumKeys */
        if (strncmp(&buf[0], "NumKeys", 7) == 0) {
            i = 7;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            numKeys = buf[i];
            fputs(buf, resp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* mode defaults to ECB, if dataset has IV mode will be set CBC */
            mode = NSS_DES_EDE3;
            /* zeroize the variables for the test with this data set */
            memset(key, 0, sizeof key);
            memset(iv, 0, sizeof iv);
            memset(plaintext, 0, sizeof plaintext);
            plaintextlen = 0;
            memset(ciphertext, 0, sizeof ciphertext);
            ciphertextlen = 0;
            fputs(buf, resp);
            continue;
        }
        if (numKeys == 0) {
            if (strncmp(buf, "KEYs", 4) == 0) {
                i = 4;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                    hex_to_byteval(&buf[i], &key[j]);
                    key[j + 8] = key[j];
                    key[j + 16] = key[j];
                }
                fputs(buf, resp);
                continue;
            }
        } else {
            /* KEY1 = ... */
            if (strncmp(buf, "KEY1", 4) == 0) {
                i = 4;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                    hex_to_byteval(&buf[i], &key[j]);
                }
                fputs(buf, resp);
                continue;
            }
            /* KEY2 = ... */
            if (strncmp(buf, "KEY2", 4) == 0) {
                i = 4;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 8; isxdigit(buf[i]); i += 2, j++) {
                    hex_to_byteval(&buf[i], &key[j]);
                }
                fputs(buf, resp);
                continue;
            }
            /* KEY3 = ... */
            if (strncmp(buf, "KEY3", 4) == 0) {
                i = 4;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 16; isxdigit(buf[i]); i += 2, j++) {
                    hex_to_byteval(&buf[i], &key[j]);
                }
                fputs(buf, resp);
                continue;
            }
        }

        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            mode = NSS_DES_EDE3_CBC;
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof iv; i += 2, j++) {
                hex_to_byteval(&buf[i], &iv[j]);
            }
            fputs(buf, resp);
            continue;
        }

        /* PLAINTEXT = ... */
        if (strncmp(buf, "PLAINTEXT", 9) == 0) {
            /* sanity check */
            if (crypt != ENCRYPT) {
                goto loser;
            }
            i = 9;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }
            plaintextlen = j;
            rv = tdea_encrypt_buf(mode, key,
                                  (mode == NSS_DES_EDE3) ? NULL : iv,
                                  ciphertext, &ciphertextlen, sizeof ciphertext,
                                  plaintext, plaintextlen);
            if (rv != SECSuccess) {
                goto loser;
            }

            fputs(buf, resp);
            fputs("CIPHERTEXT = ", resp);
            to_hex_str(buf, ciphertext, ciphertextlen);
            fputs(buf, resp);
            fputc('\n', resp);
            continue;
        }
        /* CIPHERTEXT = ... */
        if (strncmp(buf, "CIPHERTEXT", 10) == 0) {
            /* sanity check */
            if (crypt != DECRYPT) {
                goto loser;
            }

            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }
            ciphertextlen = j;

            rv = tdea_decrypt_buf(mode, key,
                                  (mode == NSS_DES_EDE3) ? NULL : iv,
                                  plaintext, &plaintextlen, sizeof plaintext,
                                  ciphertext, ciphertextlen);
            if (rv != SECSuccess) {
                goto loser;
            }

            fputs(buf, resp);
            fputs("PLAINTEXT = ", resp);
            to_hex_str(buf, plaintext, plaintextlen);
            fputs(buf, resp);
            fputc('\n', resp);
            continue;
        }
    }

loser:
    fclose(req);
}

/*
* Set the parity bit for the given byte
*/
BYTE
odd_parity(BYTE in)
{
    BYTE out = in;
    in ^= in >> 4;
    in ^= in >> 2;
    in ^= in >> 1;
    return (BYTE)(out ^ !(in & 1));
}

/*
 * Generate Keys [i+1] from Key[i], PT/CT[j-2], PT/CT[j-1], and PT/CT[j]
 * for TDEA Monte Carlo Test (MCT) in ECB and CBC modes.
 */
void
tdea_mct_next_keys(unsigned char *key,
                   const unsigned char *text_2, const unsigned char *text_1,
                   const unsigned char *text, unsigned int numKeys)
{
    int k;

    /* key1[i+1] = key1[i] xor PT/CT[j] */
    for (k = 0; k < 8; k++) {
        key[k] ^= text[k];
    }
    /* key2 */
    if (numKeys == 2 || numKeys == 3) {
        /* key2 independent */
        for (k = 8; k < 16; k++) {
            /* key2[i+1] = KEY2[i] xor PT/CT[j-1] */
            key[k] ^= text_1[k - 8];
        }
    } else {
        /* key2 == key 1 */
        for (k = 8; k < 16; k++) {
            /* key2[i+1] = KEY2[i] xor PT/CT[j] */
            key[k] = key[k - 8];
        }
    }
    /* key3 */
    if (numKeys == 1 || numKeys == 2) {
        /* key3 == key 1 */
        for (k = 16; k < 24; k++) {
            /* key3[i+1] = KEY3[i] xor PT/CT[j] */
            key[k] = key[k - 16];
        }
    } else {
        /* key3 independent */
        for (k = 16; k < 24; k++) {
            /* key3[i+1] = KEY3[i] xor PT/CT[j-2] */
            key[k] ^= text_2[k - 16];
        }
    }
    /* set the parity bits */
    for (k = 0; k < 24; k++) {
        key[k] = odd_parity(key[k]);
    }
}

/*
 * Perform the Monte Carlo Test
 *
 * mode = NSS_DES_EDE3 or NSS_DES_EDE3_CBC
 * crypt = ENCRYPT || DECRYPT
 * inputtext = plaintext or Cyphertext depending on the value of crypt
 * inputlength is expected to be size 8 bytes
 * iv = needs to be set for NSS_DES_EDE3_CBC mode
 * resp = is the output response file.
 */
void
tdea_mct_test(int mode, unsigned char *key, unsigned int numKeys,
              unsigned int crypt, unsigned char *inputtext,
              unsigned int inputlength, unsigned char *iv, FILE *resp)
{

    int i, j;
    unsigned char outputtext_1[8]; /* PT/CT[j-1] */
    unsigned char outputtext_2[8]; /* PT/CT[j-2] */
    char buf[80];                  /* holds one line from the input REQUEST file. */
    unsigned int outputlen;
    unsigned char outputtext[8];

    SECStatus rv;

    if (mode == NSS_DES_EDE3 && iv != NULL) {
        printf("IV must be NULL for NSS_DES_EDE3 mode");
        goto loser;
    } else if (mode == NSS_DES_EDE3_CBC && iv == NULL) {
        printf("IV must not be NULL for NSS_DES_EDE3_CBC mode");
        goto loser;
    }

    /* loop 400 times */
    for (i = 0; i < 400; i++) {
        /* if i == 0 CV[0] = IV  not necessary */
        /* record the count and key values and plainText */
        snprintf(buf, sizeof(buf), "COUNT = %d\n", i);
        fputs(buf, resp);
        /* Output KEY1[i] */
        fputs("KEY1 = ", resp);
        to_hex_str(buf, key, 8);
        fputs(buf, resp);
        fputc('\n', resp);
        /* Output KEY2[i] */
        fputs("KEY2 = ", resp);
        to_hex_str(buf, &key[8], 8);
        fputs(buf, resp);
        fputc('\n', resp);
        /* Output KEY3[i] */
        fputs("KEY3 = ", resp);
        to_hex_str(buf, &key[16], 8);
        fputs(buf, resp);
        fputc('\n', resp);
        if (mode == NSS_DES_EDE3_CBC) {
            /* Output CV[i] */
            fputs("IV = ", resp);
            to_hex_str(buf, iv, 8);
            fputs(buf, resp);
            fputc('\n', resp);
        }
        if (crypt == ENCRYPT) {
            /* Output PT[0] */
            fputs("PLAINTEXT = ", resp);
        } else {
            /* Output CT[0] */
            fputs("CIPHERTEXT = ", resp);
        }

        to_hex_str(buf, inputtext, inputlength);
        fputs(buf, resp);
        fputc('\n', resp);

        /* loop 10,000 times */
        for (j = 0; j < 10000; j++) {

            outputlen = 0;
            if (crypt == ENCRYPT) {
                /* inputtext == ciphertext outputtext == plaintext*/
                rv = tdea_encrypt_buf(mode, key,
                                      (mode ==
                                       NSS_DES_EDE3)
                                          ? NULL
                                          : iv,
                                      outputtext, &outputlen, 8,
                                      inputtext, 8);
            } else {
                /* inputtext == plaintext outputtext == ciphertext */
                rv = tdea_decrypt_buf(mode, key,
                                      (mode ==
                                       NSS_DES_EDE3)
                                          ? NULL
                                          : iv,
                                      outputtext, &outputlen, 8,
                                      inputtext, 8);
            }

            if (rv != SECSuccess) {
                goto loser;
            }
            if (outputlen != inputlength) {
                goto loser;
            }

            if (mode == NSS_DES_EDE3_CBC) {
                if (crypt == ENCRYPT) {
                    if (j == 0) {
                        /*P[j+1] = CV[0] */
                        memcpy(inputtext, iv, 8);
                    } else {
                        /* p[j+1] = C[j-1] */
                        memcpy(inputtext, outputtext_1, 8);
                    }
                    /* CV[j+1] = C[j] */
                    memcpy(iv, outputtext, 8);
                    if (j != 9999) {
                        /* save C[j-1] */
                        memcpy(outputtext_1, outputtext, 8);
                    }
                } else { /* DECRYPT */
                    /* CV[j+1] = C[j] */
                    memcpy(iv, inputtext, 8);
                    /* C[j+1] = P[j] */
                    memcpy(inputtext, outputtext, 8);
                }
            } else {
                /* ECB mode PT/CT[j+1] = CT/PT[j] */
                memcpy(inputtext, outputtext, 8);
            }

            /* Save PT/CT[j-2] and PT/CT[j-1] */
            if (j == 9997)
                memcpy(outputtext_2, outputtext, 8);
            if (j == 9998)
                memcpy(outputtext_1, outputtext, 8);
            /* done at the end of the for(j) loop */
        }

        if (crypt == ENCRYPT) {
            /* Output CT[j] */
            fputs("CIPHERTEXT = ", resp);
        } else {
            /* Output PT[j] */
            fputs("PLAINTEXT = ", resp);
        }
        to_hex_str(buf, outputtext, 8);
        fputs(buf, resp);
        fputc('\n', resp);

        /* Key[i+1] = Key[i] xor ...  outputtext_2 == PT/CT[j-2]
         *  outputtext_1 == PT/CT[j-1] outputtext == PT/CT[j]
         */
        tdea_mct_next_keys(key, outputtext_2,
                           outputtext_1, outputtext, numKeys);

        if (mode == NSS_DES_EDE3_CBC) {
            /* taken care of in the j=9999 iteration */
            if (crypt == ENCRYPT) {
                /* P[i] = C[j-1] */
                /* CV[i] = C[j] */
            } else {
                /* taken care of in the j=9999 iteration */
                /* CV[i] = C[j] */
                /* C[i] = P[j]  */
            }
        } else {
            /* ECB PT/CT[i] = PT/CT[j]  */
            memcpy(inputtext, outputtext, 8);
        }
        /* done at the end of the for(i) loop */
        fputc('\n', resp);
    }

loser:
    return;
}

/*
 * Perform the TDEA Monte Carlo Test (MCT) in ECB/CBC modes.
 * by gathering the input from the request file, and then
 * calling tdea_mct_test.
 *
 * reqfn is the pathname of the input REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
tdea_mct(int mode, char *reqfn)
{
    int i, j;
    char buf[80];           /* holds one line from the input REQUEST file. */
    FILE *req;              /* input stream from the REQUEST file */
    FILE *resp;             /* output stream to the RESPONSE file */
    unsigned int crypt = 0; /* 1 means encrypt, 0 means decrypt */
    unsigned char key[24];  /* TDEA 3 key bundle */
    unsigned int numKeys = 0;
    unsigned char plaintext[8];  /* PT[j] */
    unsigned char ciphertext[8]; /* CT[j] */
    unsigned char iv[8];

    /* zeroize the variables for the test with this data set */
    memset(key, 0, sizeof key);
    memset(plaintext, 0, sizeof plaintext);
    memset(ciphertext, 0, sizeof ciphertext);
    memset(iv, 0, sizeof iv);

    req = fopen(reqfn, "r");
    resp = stdout;
    while (fgets(buf, sizeof buf, req) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, resp);
            continue;
        }
        /* [ENCRYPT] or [DECRYPT] */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "ENCRYPT", 7) == 0) {
                crypt = ENCRYPT;
            } else {
                crypt = DECRYPT;
            }
            fputs(buf, resp);
            continue;
        }
        /* NumKeys */
        if (strncmp(&buf[0], "NumKeys", 7) == 0) {
            i = 7;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            numKeys = atoi(&buf[i]);
            continue;
        }
        /* KEY1 = ... */
        if (strncmp(buf, "KEY1", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            continue;
        }
        /* KEY2 = ... */
        if (strncmp(buf, "KEY2", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 8; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            continue;
        }
        /* KEY3 = ... */
        if (strncmp(buf, "KEY3", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 16; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            continue;
        }

        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof iv; i += 2, j++) {
                hex_to_byteval(&buf[i], &iv[j]);
            }
            continue;
        }

        /* PLAINTEXT = ... */
        if (strncmp(buf, "PLAINTEXT", 9) == 0) {

            /* sanity check */
            if (crypt != ENCRYPT) {
                goto loser;
            }
            /* PT[0] = PT */
            i = 9;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof plaintext; i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }

            /* do the Monte Carlo test */
            if (mode == NSS_DES_EDE3) {
                tdea_mct_test(NSS_DES_EDE3, key, numKeys, crypt, plaintext, sizeof plaintext, NULL, resp);
            } else {
                tdea_mct_test(NSS_DES_EDE3_CBC, key, numKeys, crypt, plaintext, sizeof plaintext, iv, resp);
            }
            continue;
        }
        /* CIPHERTEXT = ... */
        if (strncmp(buf, "CIPHERTEXT", 10) == 0) {
            /* sanity check */
            if (crypt != DECRYPT) {
                goto loser;
            }
            /* CT[0] = CT */
            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }

            /* do the Monte Carlo test */
            if (mode == NSS_DES_EDE3) {
                tdea_mct_test(NSS_DES_EDE3, key, numKeys, crypt, ciphertext, sizeof ciphertext, NULL, resp);
            } else {
                tdea_mct_test(NSS_DES_EDE3_CBC, key, numKeys, crypt, ciphertext, sizeof ciphertext, iv, resp);
            }
            continue;
        }
    }

loser:
    fclose(req);
}

SECStatus
aes_encrypt_buf(
    int mode,
    const unsigned char *key, unsigned int keysize,
    const unsigned char *iv,
    unsigned char *output, unsigned int *outputlen, unsigned int maxoutputlen,
    const unsigned char *input, unsigned int inputlen)
{
    SECStatus rv = SECFailure;
    AESContext *cx;
    unsigned char doublecheck[10 * 16]; /* 1 to 10 blocks */
    unsigned int doublechecklen = 0;

    cx = AES_CreateContext(key, iv, mode, PR_TRUE, keysize, 16);
    if (cx == NULL) {
        goto loser;
    }
    rv = AES_Encrypt(cx, output, outputlen, maxoutputlen, input, inputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (*outputlen != inputlen) {
        goto loser;
    }
    AES_DestroyContext(cx, PR_TRUE);
    cx = NULL;

    /*
     * Doublecheck our result by decrypting the ciphertext and
     * compare the output with the input plaintext.
     */
    cx = AES_CreateContext(key, iv, mode, PR_FALSE, keysize, 16);
    if (cx == NULL) {
        goto loser;
    }
    rv = AES_Decrypt(cx, doublecheck, &doublechecklen, sizeof doublecheck,
                     output, *outputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (doublechecklen != *outputlen) {
        goto loser;
    }
    AES_DestroyContext(cx, PR_TRUE);
    cx = NULL;
    if (memcmp(doublecheck, input, inputlen) != 0) {
        goto loser;
    }
    rv = SECSuccess;

loser:
    if (cx != NULL) {
        AES_DestroyContext(cx, PR_TRUE);
    }
    return rv;
}

SECStatus
aes_decrypt_buf(
    int mode,
    const unsigned char *key, unsigned int keysize,
    const unsigned char *iv,
    unsigned char *output, unsigned int *outputlen, unsigned int maxoutputlen,
    const unsigned char *input, unsigned int inputlen)
{
    SECStatus rv = SECFailure;
    AESContext *cx;
    unsigned char doublecheck[10 * 16]; /* 1 to 10 blocks */
    unsigned int doublechecklen = 0;

    cx = AES_CreateContext(key, iv, mode, PR_FALSE, keysize, 16);
    if (cx == NULL) {
        goto loser;
    }
    rv = AES_Decrypt(cx, output, outputlen, maxoutputlen,
                     input, inputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (*outputlen != inputlen) {
        goto loser;
    }
    AES_DestroyContext(cx, PR_TRUE);
    cx = NULL;

    /*
     * Doublecheck our result by encrypting the plaintext and
     * compare the output with the input ciphertext.
     */
    cx = AES_CreateContext(key, iv, mode, PR_TRUE, keysize, 16);
    if (cx == NULL) {
        goto loser;
    }
    rv = AES_Encrypt(cx, doublecheck, &doublechecklen, sizeof doublecheck,
                     output, *outputlen);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (doublechecklen != *outputlen) {
        goto loser;
    }
    AES_DestroyContext(cx, PR_TRUE);
    cx = NULL;
    if (memcmp(doublecheck, input, inputlen) != 0) {
        goto loser;
    }
    rv = SECSuccess;

loser:
    if (cx != NULL) {
        AES_DestroyContext(cx, PR_TRUE);
    }
    return rv;
}
/*
 * Perform the AES GCM tests.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
aes_gcm(char *reqfn, int encrypt)
{
    char buf[512]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "CIPHERTEXT = <320 hex digits>\n".
                         */
    FILE *aesreq;  /* input stream from the REQUEST file */
    FILE *aesresp; /* output stream to the RESPONSE file */
    int i, j;
    unsigned char key[32]; /* 128, 192, or 256 bits */
    unsigned int keysize = 0;
    unsigned char iv[128];            /* handle large gcm IV's */
    unsigned char plaintext[10 * 16]; /* 1 to 10 blocks */
    unsigned int plaintextlen;
    unsigned char ciphertext[11 * 16]; /* 1 to 10 blocks + tag */
    unsigned int ciphertextlen;
    unsigned char aad[11 * 16]; /* 1 to 10 blocks + tag */
    unsigned int aadlen = 0;
    unsigned int tagbits;
    unsigned int taglen = 0;
    unsigned int ivlen;
    CK_NSS_GCM_PARAMS params;
    SECStatus rv;

    aesreq = fopen(reqfn, "r");
    aesresp = stdout;
    while (fgets(buf, sizeof buf, aesreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, aesresp);
            continue;
        }
        /* [ENCRYPT] or [DECRYPT] */
        if (buf[0] == '[') {
            if (strncmp(buf, "[Taglen", 7) == 0) {
                if (sscanf(buf, "[Taglen = %d]", &tagbits) != 1) {
                    goto loser;
                }
                taglen = tagbits / 8;
            }
            if (strncmp(buf, "[IVlen", 6) == 0) {
                if (sscanf(buf, "[IVlen = %d]", &ivlen) != 1) {
                    goto loser;
                }
                ivlen = ivlen / 8;
            }
            fputs(buf, aesresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "Count", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(key, 0, sizeof key);
            keysize = 0;
            memset(iv, 0, sizeof iv);
            memset(plaintext, 0, sizeof plaintext);
            plaintextlen = 0;
            memset(ciphertext, 0, sizeof ciphertext);
            ciphertextlen = 0;
            fputs(buf, aesresp);
            continue;
        }
        /* KEY = ... */
        if (strncmp(buf, "Key", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            keysize = j;
            fputs(buf, aesresp);
            continue;
        }
        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof iv; i += 2, j++) {
                hex_to_byteval(&buf[i], &iv[j]);
            }
            fputs(buf, aesresp);
            continue;
        }
        /* PLAINTEXT = ... */
        if (strncmp(buf, "PT", 2) == 0) {
            /* sanity check */
            if (!encrypt) {
                goto loser;
            }

            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }
            plaintextlen = j;
            fputs(buf, aesresp);
            continue;
        }
        /* CIPHERTEXT = ... */
        if (strncmp(buf, "CT", 2) == 0) {
            /* sanity check */
            if (encrypt) {
                goto loser;
            }

            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }
            ciphertextlen = j;
            fputs(buf, aesresp);
            continue;
        }
        if (strncmp(buf, "AAD", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &aad[j]);
            }
            aadlen = j;
            fputs(buf, aesresp);
            if (encrypt) {
                if (encrypt == 2) {
                    rv = RNG_GenerateGlobalRandomBytes(iv, ivlen);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                }
                params.pIv = iv;
                params.ulIvLen = ivlen;
                params.pAAD = aad;
                params.ulAADLen = aadlen;
                params.ulTagBits = tagbits;
                rv = aes_encrypt_buf(NSS_AES_GCM, key, keysize,
                                     (unsigned char *)&params,
                                     ciphertext, &ciphertextlen, sizeof ciphertext,
                                     plaintext, plaintextlen);
                if (rv != SECSuccess) {
                    goto loser;
                }

                if (encrypt == 2) {
                    fputs("IV = ", aesresp);
                    to_hex_str(buf, iv, ivlen);
                    fputs(buf, aesresp);
                    fputc('\n', aesresp);
                }
                fputs("CT = ", aesresp);
                j = ciphertextlen - taglen;
                to_hex_str(buf, ciphertext, j);
                fputs(buf, aesresp);
                fputs("\nTag = ", aesresp);
                to_hex_str(buf, ciphertext + j, taglen);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
            }
            continue;
        }
        if (strncmp(buf, "Tag", 3) == 0) {
            /* sanity check */
            if (encrypt) {
                goto loser;
            }

            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j + ciphertextlen]);
            }
            ciphertextlen += j;
            params.pIv = iv;
            params.ulIvLen = ivlen;
            params.pAAD = aad;
            params.ulAADLen = aadlen;
            params.ulTagBits = tagbits;
            rv = aes_decrypt_buf(NSS_AES_GCM, key, keysize,
                                 (unsigned char *)&params,
                                 plaintext, &plaintextlen, sizeof plaintext,
                                 ciphertext, ciphertextlen);
            fputs(buf, aesresp);
            if (rv != SECSuccess) {
                fprintf(aesresp, "FAIL\n");
            } else {
                fputs("PT = ", aesresp);
                to_hex_str(buf, plaintext, plaintextlen);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
            }
            continue;
        }
    }
loser:
    fclose(aesreq);
}

/*
 * Perform the AES Known Answer Test (KAT) or Multi-block Message
 * Test (MMT) in ECB or CBC mode.  The KAT (there are four types)
 * and MMT have the same structure: given the key and IV (CBC mode
 * only), encrypt the given plaintext or decrypt the given ciphertext.
 * So we can handle them the same way.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
aes_kat_mmt(char *reqfn)
{
    char buf[512]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "CIPHERTEXT = <320 hex digits>\n".
                         */
    FILE *aesreq;  /* input stream from the REQUEST file */
    FILE *aesresp; /* output stream to the RESPONSE file */
    int i, j;
    int mode = NSS_AES;    /* NSS_AES (ECB) or NSS_AES_CBC */
    int encrypt = 0;       /* 1 means encrypt, 0 means decrypt */
    unsigned char key[32]; /* 128, 192, or 256 bits */
    unsigned int keysize = 0;
    unsigned char iv[16];             /* for all modes except ECB */
    unsigned char plaintext[10 * 16]; /* 1 to 10 blocks */
    unsigned int plaintextlen;
    unsigned char ciphertext[10 * 16]; /* 1 to 10 blocks */
    unsigned int ciphertextlen;
    SECStatus rv;

    aesreq = fopen(reqfn, "r");
    aesresp = stdout;
    while (fgets(buf, sizeof buf, aesreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, aesresp);
            continue;
        }
        /* [ENCRYPT] or [DECRYPT] */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "ENCRYPT", 7) == 0) {
                encrypt = 1;
            } else {
                encrypt = 0;
            }
            fputs(buf, aesresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            mode = NSS_AES;
            /* zeroize the variables for the test with this data set */
            memset(key, 0, sizeof key);
            keysize = 0;
            memset(iv, 0, sizeof iv);
            memset(plaintext, 0, sizeof plaintext);
            plaintextlen = 0;
            memset(ciphertext, 0, sizeof ciphertext);
            ciphertextlen = 0;
            fputs(buf, aesresp);
            continue;
        }
        /* KEY = ... */
        if (strncmp(buf, "KEY", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            keysize = j;
            fputs(buf, aesresp);
            continue;
        }
        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            mode = NSS_AES_CBC;
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof iv; i += 2, j++) {
                hex_to_byteval(&buf[i], &iv[j]);
            }
            fputs(buf, aesresp);
            continue;
        }
        /* PLAINTEXT = ... */
        if (strncmp(buf, "PLAINTEXT", 9) == 0) {
            /* sanity check */
            if (!encrypt) {
                goto loser;
            }

            i = 9;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }
            plaintextlen = j;

            rv = aes_encrypt_buf(mode, key, keysize,
                                 (mode ==
                                  NSS_AES)
                                     ? NULL
                                     : iv,
                                 ciphertext, &ciphertextlen, sizeof ciphertext,
                                 plaintext, plaintextlen);
            if (rv != SECSuccess) {
                goto loser;
            }

            fputs(buf, aesresp);
            fputs("CIPHERTEXT = ", aesresp);
            to_hex_str(buf, ciphertext, ciphertextlen);
            fputs(buf, aesresp);
            fputc('\n', aesresp);
            continue;
        }
        /* CIPHERTEXT = ... */
        if (strncmp(buf, "CIPHERTEXT", 10) == 0) {
            /* sanity check */
            if (encrypt) {
                goto loser;
            }

            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }
            ciphertextlen = j;

            rv = aes_decrypt_buf(mode, key, keysize,
                                 (mode ==
                                  NSS_AES)
                                     ? NULL
                                     : iv,
                                 plaintext, &plaintextlen, sizeof plaintext,
                                 ciphertext, ciphertextlen);
            if (rv != SECSuccess) {
                goto loser;
            }

            fputs(buf, aesresp);
            fputs("PLAINTEXT = ", aesresp);
            to_hex_str(buf, plaintext, plaintextlen);
            fputs(buf, aesresp);
            fputc('\n', aesresp);
            continue;
        }
    }
loser:
    fclose(aesreq);
}

/*
 * Generate Key[i+1] from Key[i], CT[j-1], and CT[j] for AES Monte Carlo
 * Test (MCT) in ECB and CBC modes.
 */
void
aes_mct_next_key(unsigned char *key, unsigned int keysize,
                 const unsigned char *ciphertext_1, const unsigned char *ciphertext)
{
    int k;

    switch (keysize) {
        case 16: /* 128-bit key */
            /* Key[i+1] = Key[i] xor CT[j] */
            for (k = 0; k < 16; k++) {
                key[k] ^= ciphertext[k];
            }
            break;
        case 24: /* 192-bit key */
            /*
         * Key[i+1] = Key[i] xor (last 64-bits of
         *            CT[j-1] || CT[j])
         */
            for (k = 0; k < 8; k++) {
                key[k] ^= ciphertext_1[k + 8];
            }
            for (k = 8; k < 24; k++) {
                key[k] ^= ciphertext[k - 8];
            }
            break;
        case 32: /* 256-bit key */
            /* Key[i+1] = Key[i] xor (CT[j-1] || CT[j]) */
            for (k = 0; k < 16; k++) {
                key[k] ^= ciphertext_1[k];
            }
            for (k = 16; k < 32; k++) {
                key[k] ^= ciphertext[k - 16];
            }
            break;
    }
}

/*
 * Perform the AES Monte Carlo Test (MCT) in ECB mode.  MCT exercises
 * our AES code in streaming mode because the plaintext or ciphertext
 * is generated block by block as we go, so we can't collect all the
 * plaintext or ciphertext in one buffer and encrypt or decrypt it in
 * one shot.
 *
 * reqfn is the pathname of the input REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
aes_ecb_mct(char *reqfn)
{
    char buf[80];  /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "KEY = <64 hex digits>\n".
                         */
    FILE *aesreq;  /* input stream from the REQUEST file */
    FILE *aesresp; /* output stream to the RESPONSE file */
    int i, j;
    int encrypt = 0;       /* 1 means encrypt, 0 means decrypt */
    unsigned char key[32]; /* 128, 192, or 256 bits */
    unsigned int keysize = 0;
    unsigned char plaintext[16];    /* PT[j] */
    unsigned char plaintext_1[16];  /* PT[j-1] */
    unsigned char ciphertext[16];   /* CT[j] */
    unsigned char ciphertext_1[16]; /* CT[j-1] */
    unsigned char doublecheck[16];
    unsigned int outputlen;
    AESContext *cx = NULL;  /* the operation being tested */
    AESContext *cx2 = NULL; /* the inverse operation done in parallel
                                 * to doublecheck our result.
                                 */
    SECStatus rv;

    aesreq = fopen(reqfn, "r");
    aesresp = stdout;
    while (fgets(buf, sizeof buf, aesreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, aesresp);
            continue;
        }
        /* [ENCRYPT] or [DECRYPT] */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "ENCRYPT", 7) == 0) {
                encrypt = 1;
            } else {
                encrypt = 0;
            }
            fputs(buf, aesresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(key, 0, sizeof key);
            keysize = 0;
            memset(plaintext, 0, sizeof plaintext);
            memset(ciphertext, 0, sizeof ciphertext);
            continue;
        }
        /* KEY = ... */
        if (strncmp(buf, "KEY", 3) == 0) {
            /* Key[0] = Key */
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            keysize = j;
            continue;
        }
        /* PLAINTEXT = ... */
        if (strncmp(buf, "PLAINTEXT", 9) == 0) {
            /* sanity check */
            if (!encrypt) {
                goto loser;
            }
            /* PT[0] = PT */
            i = 9;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof plaintext; i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }

            for (i = 0; i < 100; i++) {
                snprintf(buf, sizeof(buf), "COUNT = %d\n", i);
                fputs(buf, aesresp);
                /* Output Key[i] */
                fputs("KEY = ", aesresp);
                to_hex_str(buf, key, keysize);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
                /* Output PT[0] */
                fputs("PLAINTEXT = ", aesresp);
                to_hex_str(buf, plaintext, sizeof plaintext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                cx = AES_CreateContext(key, NULL, NSS_AES,
                                       PR_TRUE, keysize, 16);
                if (cx == NULL) {
                    goto loser;
                }
                /*
                 * doublecheck our result by decrypting the result
                 * and comparing the output with the plaintext.
                 */
                cx2 = AES_CreateContext(key, NULL, NSS_AES,
                                        PR_FALSE, keysize, 16);
                if (cx2 == NULL) {
                    goto loser;
                }
                for (j = 0; j < 1000; j++) {
                    /* Save CT[j-1] */
                    memcpy(ciphertext_1, ciphertext, sizeof ciphertext);

                    /* CT[j] = AES(Key[i], PT[j]) */
                    outputlen = 0;
                    rv = AES_Encrypt(cx,
                                     ciphertext, &outputlen, sizeof ciphertext,
                                     plaintext, sizeof plaintext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof plaintext) {
                        goto loser;
                    }

                    /* doublecheck our result */
                    outputlen = 0;
                    rv = AES_Decrypt(cx2,
                                     doublecheck, &outputlen, sizeof doublecheck,
                                     ciphertext, sizeof ciphertext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof ciphertext) {
                        goto loser;
                    }
                    if (memcmp(doublecheck, plaintext, sizeof plaintext)) {
                        goto loser;
                    }

                    /* PT[j+1] = CT[j] */
                    memcpy(plaintext, ciphertext, sizeof plaintext);
                }
                AES_DestroyContext(cx, PR_TRUE);
                cx = NULL;
                AES_DestroyContext(cx2, PR_TRUE);
                cx2 = NULL;

                /* Output CT[j] */
                fputs("CIPHERTEXT = ", aesresp);
                to_hex_str(buf, ciphertext, sizeof ciphertext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                /* Key[i+1] = Key[i] xor ... */
                aes_mct_next_key(key, keysize, ciphertext_1, ciphertext);
                /* PT[0] = CT[j] */
                /* done at the end of the for(j) loop */

                fputc('\n', aesresp);
            }

            continue;
        }
        /* CIPHERTEXT = ... */
        if (strncmp(buf, "CIPHERTEXT", 10) == 0) {
            /* sanity check */
            if (encrypt) {
                goto loser;
            }
            /* CT[0] = CT */
            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }

            for (i = 0; i < 100; i++) {
                snprintf(buf, sizeof(buf), "COUNT = %d\n", i);
                fputs(buf, aesresp);
                /* Output Key[i] */
                fputs("KEY = ", aesresp);
                to_hex_str(buf, key, keysize);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
                /* Output CT[0] */
                fputs("CIPHERTEXT = ", aesresp);
                to_hex_str(buf, ciphertext, sizeof ciphertext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                cx = AES_CreateContext(key, NULL, NSS_AES,
                                       PR_FALSE, keysize, 16);
                if (cx == NULL) {
                    goto loser;
                }
                /*
                 * doublecheck our result by encrypting the result
                 * and comparing the output with the ciphertext.
                 */
                cx2 = AES_CreateContext(key, NULL, NSS_AES,
                                        PR_TRUE, keysize, 16);
                if (cx2 == NULL) {
                    goto loser;
                }
                for (j = 0; j < 1000; j++) {
                    /* Save PT[j-1] */
                    memcpy(plaintext_1, plaintext, sizeof plaintext);

                    /* PT[j] = AES(Key[i], CT[j]) */
                    outputlen = 0;
                    rv = AES_Decrypt(cx,
                                     plaintext, &outputlen, sizeof plaintext,
                                     ciphertext, sizeof ciphertext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof ciphertext) {
                        goto loser;
                    }

                    /* doublecheck our result */
                    outputlen = 0;
                    rv = AES_Encrypt(cx2,
                                     doublecheck, &outputlen, sizeof doublecheck,
                                     plaintext, sizeof plaintext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof plaintext) {
                        goto loser;
                    }
                    if (memcmp(doublecheck, ciphertext, sizeof ciphertext)) {
                        goto loser;
                    }

                    /* CT[j+1] = PT[j] */
                    memcpy(ciphertext, plaintext, sizeof ciphertext);
                }
                AES_DestroyContext(cx, PR_TRUE);
                cx = NULL;
                AES_DestroyContext(cx2, PR_TRUE);
                cx2 = NULL;

                /* Output PT[j] */
                fputs("PLAINTEXT = ", aesresp);
                to_hex_str(buf, plaintext, sizeof plaintext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                /* Key[i+1] = Key[i] xor ... */
                aes_mct_next_key(key, keysize, plaintext_1, plaintext);
                /* CT[0] = PT[j] */
                /* done at the end of the for(j) loop */

                fputc('\n', aesresp);
            }

            continue;
        }
    }
loser:
    if (cx != NULL) {
        AES_DestroyContext(cx, PR_TRUE);
    }
    if (cx2 != NULL) {
        AES_DestroyContext(cx2, PR_TRUE);
    }
    fclose(aesreq);
}

/*
 * Perform the AES Monte Carlo Test (MCT) in CBC mode.  MCT exercises
 * our AES code in streaming mode because the plaintext or ciphertext
 * is generated block by block as we go, so we can't collect all the
 * plaintext or ciphertext in one buffer and encrypt or decrypt it in
 * one shot.
 *
 * reqfn is the pathname of the input REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
aes_cbc_mct(char *reqfn)
{
    char buf[80];  /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "KEY = <64 hex digits>\n".
                         */
    FILE *aesreq;  /* input stream from the REQUEST file */
    FILE *aesresp; /* output stream to the RESPONSE file */
    int i, j;
    int encrypt = 0;       /* 1 means encrypt, 0 means decrypt */
    unsigned char key[32]; /* 128, 192, or 256 bits */
    unsigned int keysize = 0;
    unsigned char iv[16];
    unsigned char plaintext[16];    /* PT[j] */
    unsigned char plaintext_1[16];  /* PT[j-1] */
    unsigned char ciphertext[16];   /* CT[j] */
    unsigned char ciphertext_1[16]; /* CT[j-1] */
    unsigned char doublecheck[16];
    unsigned int outputlen;
    AESContext *cx = NULL;  /* the operation being tested */
    AESContext *cx2 = NULL; /* the inverse operation done in parallel
                                 * to doublecheck our result.
                                 */
    SECStatus rv;

    aesreq = fopen(reqfn, "r");
    aesresp = stdout;
    while (fgets(buf, sizeof buf, aesreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, aesresp);
            continue;
        }
        /* [ENCRYPT] or [DECRYPT] */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "ENCRYPT", 7) == 0) {
                encrypt = 1;
            } else {
                encrypt = 0;
            }
            fputs(buf, aesresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(key, 0, sizeof key);
            keysize = 0;
            memset(iv, 0, sizeof iv);
            memset(plaintext, 0, sizeof plaintext);
            memset(ciphertext, 0, sizeof ciphertext);
            continue;
        }
        /* KEY = ... */
        if (strncmp(buf, "KEY", 3) == 0) {
            /* Key[0] = Key */
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            keysize = j;
            continue;
        }
        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            /* IV[0] = IV */
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof iv; i += 2, j++) {
                hex_to_byteval(&buf[i], &iv[j]);
            }
            continue;
        }
        /* PLAINTEXT = ... */
        if (strncmp(buf, "PLAINTEXT", 9) == 0) {
            /* sanity check */
            if (!encrypt) {
                goto loser;
            }
            /* PT[0] = PT */
            i = 9;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof plaintext; i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }

            for (i = 0; i < 100; i++) {
                snprintf(buf, sizeof(buf), "COUNT = %d\n", i);
                fputs(buf, aesresp);
                /* Output Key[i] */
                fputs("KEY = ", aesresp);
                to_hex_str(buf, key, keysize);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
                /* Output IV[i] */
                fputs("IV = ", aesresp);
                to_hex_str(buf, iv, sizeof iv);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
                /* Output PT[0] */
                fputs("PLAINTEXT = ", aesresp);
                to_hex_str(buf, plaintext, sizeof plaintext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                cx = AES_CreateContext(key, iv, NSS_AES_CBC,
                                       PR_TRUE, keysize, 16);
                if (cx == NULL) {
                    goto loser;
                }
                /*
                 * doublecheck our result by decrypting the result
                 * and comparing the output with the plaintext.
                 */
                cx2 = AES_CreateContext(key, iv, NSS_AES_CBC,
                                        PR_FALSE, keysize, 16);
                if (cx2 == NULL) {
                    goto loser;
                }
                /* CT[-1] = IV[i] */
                memcpy(ciphertext, iv, sizeof ciphertext);
                for (j = 0; j < 1000; j++) {
                    /* Save CT[j-1] */
                    memcpy(ciphertext_1, ciphertext, sizeof ciphertext);
                    /*
                     * If ( j=0 )
                     *      CT[j] = AES(Key[i], IV[i], PT[j])
                     *      PT[j+1] = IV[i] (= CT[j-1])
                     * Else
                     *      CT[j] = AES(Key[i], PT[j])
                     *      PT[j+1] = CT[j-1]
                     */
                    outputlen = 0;
                    rv = AES_Encrypt(cx,
                                     ciphertext, &outputlen, sizeof ciphertext,
                                     plaintext, sizeof plaintext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof plaintext) {
                        goto loser;
                    }

                    /* doublecheck our result */
                    outputlen = 0;
                    rv = AES_Decrypt(cx2,
                                     doublecheck, &outputlen, sizeof doublecheck,
                                     ciphertext, sizeof ciphertext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof ciphertext) {
                        goto loser;
                    }
                    if (memcmp(doublecheck, plaintext, sizeof plaintext)) {
                        goto loser;
                    }

                    memcpy(plaintext, ciphertext_1, sizeof plaintext);
                }
                AES_DestroyContext(cx, PR_TRUE);
                cx = NULL;
                AES_DestroyContext(cx2, PR_TRUE);
                cx2 = NULL;

                /* Output CT[j] */
                fputs("CIPHERTEXT = ", aesresp);
                to_hex_str(buf, ciphertext, sizeof ciphertext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                /* Key[i+1] = Key[i] xor ... */
                aes_mct_next_key(key, keysize, ciphertext_1, ciphertext);
                /* IV[i+1] = CT[j] */
                memcpy(iv, ciphertext, sizeof iv);
                /* PT[0] = CT[j-1] */
                /* done at the end of the for(j) loop */

                fputc('\n', aesresp);
            }

            continue;
        }
        /* CIPHERTEXT = ... */
        if (strncmp(buf, "CIPHERTEXT", 10) == 0) {
            /* sanity check */
            if (encrypt) {
                goto loser;
            }
            /* CT[0] = CT */
            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }

            for (i = 0; i < 100; i++) {
                snprintf(buf, sizeof(buf), "COUNT = %d\n", i);
                fputs(buf, aesresp);
                /* Output Key[i] */
                fputs("KEY = ", aesresp);
                to_hex_str(buf, key, keysize);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
                /* Output IV[i] */
                fputs("IV = ", aesresp);
                to_hex_str(buf, iv, sizeof iv);
                fputs(buf, aesresp);
                fputc('\n', aesresp);
                /* Output CT[0] */
                fputs("CIPHERTEXT = ", aesresp);
                to_hex_str(buf, ciphertext, sizeof ciphertext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                cx = AES_CreateContext(key, iv, NSS_AES_CBC,
                                       PR_FALSE, keysize, 16);
                if (cx == NULL) {
                    goto loser;
                }
                /*
                 * doublecheck our result by encrypting the result
                 * and comparing the output with the ciphertext.
                 */
                cx2 = AES_CreateContext(key, iv, NSS_AES_CBC,
                                        PR_TRUE, keysize, 16);
                if (cx2 == NULL) {
                    goto loser;
                }
                /* PT[-1] = IV[i] */
                memcpy(plaintext, iv, sizeof plaintext);
                for (j = 0; j < 1000; j++) {
                    /* Save PT[j-1] */
                    memcpy(plaintext_1, plaintext, sizeof plaintext);
                    /*
                     * If ( j=0 )
                     *      PT[j] = AES(Key[i], IV[i], CT[j])
                     *      CT[j+1] = IV[i] (= PT[j-1])
                     * Else
                     *      PT[j] = AES(Key[i], CT[j])
                     *      CT[j+1] = PT[j-1]
                     */
                    outputlen = 0;
                    rv = AES_Decrypt(cx,
                                     plaintext, &outputlen, sizeof plaintext,
                                     ciphertext, sizeof ciphertext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof ciphertext) {
                        goto loser;
                    }

                    /* doublecheck our result */
                    outputlen = 0;
                    rv = AES_Encrypt(cx2,
                                     doublecheck, &outputlen, sizeof doublecheck,
                                     plaintext, sizeof plaintext);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    if (outputlen != sizeof plaintext) {
                        goto loser;
                    }
                    if (memcmp(doublecheck, ciphertext, sizeof ciphertext)) {
                        goto loser;
                    }

                    memcpy(ciphertext, plaintext_1, sizeof ciphertext);
                }
                AES_DestroyContext(cx, PR_TRUE);
                cx = NULL;
                AES_DestroyContext(cx2, PR_TRUE);
                cx2 = NULL;

                /* Output PT[j] */
                fputs("PLAINTEXT = ", aesresp);
                to_hex_str(buf, plaintext, sizeof plaintext);
                fputs(buf, aesresp);
                fputc('\n', aesresp);

                /* Key[i+1] = Key[i] xor ... */
                aes_mct_next_key(key, keysize, plaintext_1, plaintext);
                /* IV[i+1] = PT[j] */
                memcpy(iv, plaintext, sizeof iv);
                /* CT[0] = PT[j-1] */
                /* done at the end of the for(j) loop */

                fputc('\n', aesresp);
            }

            continue;
        }
    }
loser:
    if (cx != NULL) {
        AES_DestroyContext(cx, PR_TRUE);
    }
    if (cx2 != NULL) {
        AES_DestroyContext(cx2, PR_TRUE);
    }
    fclose(aesreq);
}

void
write_compact_string(FILE *out, unsigned char *hash, unsigned int len)
{
    unsigned int i;
    int j, count = 0, last = -1, z = 0;
    long start = ftell(out);
    for (i = 0; i < len; i++) {
        for (j = 7; j >= 0; j--) {
            if (last < 0) {
                last = (hash[i] & (1 << j)) ? 1 : 0;
                fprintf(out, "%d ", last);
                count = 1;
            } else if (hash[i] & (1 << j)) {
                if (last) {
                    count++;
                } else {
                    last = 0;
                    fprintf(out, "%d ", count);
                    count = 1;
                    z++;
                }
            } else {
                if (!last) {
                    count++;
                } else {
                    last = 1;
                    fprintf(out, "%d ", count);
                    count = 1;
                    z++;
                }
            }
        }
    }
    fprintf(out, "^\n");
    fseek(out, start, SEEK_SET);
    fprintf(out, "%d ", z);
    fseek(out, 0, SEEK_END);
}

int
get_next_line(FILE *req, char *key, char *val, FILE *rsp)
{
    int ignore = 0;
    char *writeto = key;
    int w = 0;
    int c;
    while ((c = fgetc(req)) != EOF) {
        if (ignore) {
            fprintf(rsp, "%c", c);
            if (c == '\n')
                return ignore;
        } else if (c == '\n') {
            break;
        } else if (c == '#') {
            ignore = 1;
            fprintf(rsp, "%c", c);
        } else if (c == '=') {
            writeto[w] = '\0';
            w = 0;
            writeto = val;
        } else if (c == ' ' || c == '[' || c == ']') {
            continue;
        } else {
            writeto[w++] = c;
        }
    }
    writeto[w] = '\0';
    return (c == EOF) ? -1 : ignore;
}

typedef struct curveNameTagPairStr {
    char *curveName;
    SECOidTag curveOidTag;
} CurveNameTagPair;

#define DEFAULT_CURVE_OID_TAG SEC_OID_SECG_EC_SECP192R1
/* #define DEFAULT_CURVE_OID_TAG  SEC_OID_SECG_EC_SECP160R1 */

static CurveNameTagPair nameTagPair[] = {
    { "sect163k1", SEC_OID_SECG_EC_SECT163K1 },
    { "nistk163", SEC_OID_SECG_EC_SECT163K1 },
    { "sect163r1", SEC_OID_SECG_EC_SECT163R1 },
    { "sect163r2", SEC_OID_SECG_EC_SECT163R2 },
    { "nistb163", SEC_OID_SECG_EC_SECT163R2 },
    { "sect193r1", SEC_OID_SECG_EC_SECT193R1 },
    { "sect193r2", SEC_OID_SECG_EC_SECT193R2 },
    { "sect233k1", SEC_OID_SECG_EC_SECT233K1 },
    { "nistk233", SEC_OID_SECG_EC_SECT233K1 },
    { "sect233r1", SEC_OID_SECG_EC_SECT233R1 },
    { "nistb233", SEC_OID_SECG_EC_SECT233R1 },
    { "sect239k1", SEC_OID_SECG_EC_SECT239K1 },
    { "sect283k1", SEC_OID_SECG_EC_SECT283K1 },
    { "nistk283", SEC_OID_SECG_EC_SECT283K1 },
    { "sect283r1", SEC_OID_SECG_EC_SECT283R1 },
    { "nistb283", SEC_OID_SECG_EC_SECT283R1 },
    { "sect409k1", SEC_OID_SECG_EC_SECT409K1 },
    { "nistk409", SEC_OID_SECG_EC_SECT409K1 },
    { "sect409r1", SEC_OID_SECG_EC_SECT409R1 },
    { "nistb409", SEC_OID_SECG_EC_SECT409R1 },
    { "sect571k1", SEC_OID_SECG_EC_SECT571K1 },
    { "nistk571", SEC_OID_SECG_EC_SECT571K1 },
    { "sect571r1", SEC_OID_SECG_EC_SECT571R1 },
    { "nistb571", SEC_OID_SECG_EC_SECT571R1 },
    { "secp160k1", SEC_OID_SECG_EC_SECP160K1 },
    { "secp160r1", SEC_OID_SECG_EC_SECP160R1 },
    { "secp160r2", SEC_OID_SECG_EC_SECP160R2 },
    { "secp192k1", SEC_OID_SECG_EC_SECP192K1 },
    { "secp192r1", SEC_OID_SECG_EC_SECP192R1 },
    { "nistp192", SEC_OID_SECG_EC_SECP192R1 },
    { "secp224k1", SEC_OID_SECG_EC_SECP224K1 },
    { "secp224r1", SEC_OID_SECG_EC_SECP224R1 },
    { "nistp224", SEC_OID_SECG_EC_SECP224R1 },
    { "secp256k1", SEC_OID_SECG_EC_SECP256K1 },
    { "secp256r1", SEC_OID_SECG_EC_SECP256R1 },
    { "nistp256", SEC_OID_SECG_EC_SECP256R1 },
    { "secp384r1", SEC_OID_SECG_EC_SECP384R1 },
    { "nistp384", SEC_OID_SECG_EC_SECP384R1 },
    { "secp521r1", SEC_OID_SECG_EC_SECP521R1 },
    { "nistp521", SEC_OID_SECG_EC_SECP521R1 },

    { "prime192v1", SEC_OID_ANSIX962_EC_PRIME192V1 },
    { "prime192v2", SEC_OID_ANSIX962_EC_PRIME192V2 },
    { "prime192v3", SEC_OID_ANSIX962_EC_PRIME192V3 },
    { "prime239v1", SEC_OID_ANSIX962_EC_PRIME239V1 },
    { "prime239v2", SEC_OID_ANSIX962_EC_PRIME239V2 },
    { "prime239v3", SEC_OID_ANSIX962_EC_PRIME239V3 },

    { "c2pnb163v1", SEC_OID_ANSIX962_EC_C2PNB163V1 },
    { "c2pnb163v2", SEC_OID_ANSIX962_EC_C2PNB163V2 },
    { "c2pnb163v3", SEC_OID_ANSIX962_EC_C2PNB163V3 },
    { "c2pnb176v1", SEC_OID_ANSIX962_EC_C2PNB176V1 },
    { "c2tnb191v1", SEC_OID_ANSIX962_EC_C2TNB191V1 },
    { "c2tnb191v2", SEC_OID_ANSIX962_EC_C2TNB191V2 },
    { "c2tnb191v3", SEC_OID_ANSIX962_EC_C2TNB191V3 },
    { "c2onb191v4", SEC_OID_ANSIX962_EC_C2ONB191V4 },
    { "c2onb191v5", SEC_OID_ANSIX962_EC_C2ONB191V5 },
    { "c2pnb208w1", SEC_OID_ANSIX962_EC_C2PNB208W1 },
    { "c2tnb239v1", SEC_OID_ANSIX962_EC_C2TNB239V1 },
    { "c2tnb239v2", SEC_OID_ANSIX962_EC_C2TNB239V2 },
    { "c2tnb239v3", SEC_OID_ANSIX962_EC_C2TNB239V3 },
    { "c2onb239v4", SEC_OID_ANSIX962_EC_C2ONB239V4 },
    { "c2onb239v5", SEC_OID_ANSIX962_EC_C2ONB239V5 },
    { "c2pnb272w1", SEC_OID_ANSIX962_EC_C2PNB272W1 },
    { "c2pnb304w1", SEC_OID_ANSIX962_EC_C2PNB304W1 },
    { "c2tnb359v1", SEC_OID_ANSIX962_EC_C2TNB359V1 },
    { "c2pnb368w1", SEC_OID_ANSIX962_EC_C2PNB368W1 },
    { "c2tnb431r1", SEC_OID_ANSIX962_EC_C2TNB431R1 },

    { "secp112r1", SEC_OID_SECG_EC_SECP112R1 },
    { "secp112r2", SEC_OID_SECG_EC_SECP112R2 },
    { "secp128r1", SEC_OID_SECG_EC_SECP128R1 },
    { "secp128r2", SEC_OID_SECG_EC_SECP128R2 },

    { "sect113r1", SEC_OID_SECG_EC_SECT113R1 },
    { "sect113r2", SEC_OID_SECG_EC_SECT113R2 },
    { "sect131r1", SEC_OID_SECG_EC_SECT131R1 },
    { "sect131r2", SEC_OID_SECG_EC_SECT131R2 },
};

static SECItem *
getECParams(const char *curve)
{
    SECItem *ecparams;
    SECOidData *oidData = NULL;
    SECOidTag curveOidTag = SEC_OID_UNKNOWN; /* default */
    int i, numCurves;

    if (curve != NULL) {
        numCurves = sizeof(nameTagPair) / sizeof(CurveNameTagPair);
        for (i = 0; ((i < numCurves) && (curveOidTag == SEC_OID_UNKNOWN));
             i++) {
            if (PL_strcmp(curve, nameTagPair[i].curveName) == 0)
                curveOidTag = nameTagPair[i].curveOidTag;
        }
    }

    /* Return NULL if curve name is not recognized */
    if ((curveOidTag == SEC_OID_UNKNOWN) ||
        (oidData = SECOID_FindOIDByTag(curveOidTag)) == NULL) {
        fprintf(stderr, "Unrecognized elliptic curve %s\n", curve);
        return NULL;
    }

    ecparams = SECITEM_AllocItem(NULL, NULL, (2 + oidData->oid.len));

    /*
     * ecparams->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    ecparams->data[0] = SEC_ASN1_OBJECT_ID;
    ecparams->data[1] = oidData->oid.len;
    memcpy(ecparams->data + 2, oidData->oid.data, oidData->oid.len);

    return ecparams;
}

/*
 * HASH_ functions are available to full NSS apps and internally inside
 * freebl, but not exported to users of freebl. Create short stubs to
 * replace the functionality for fipstest.
 */
SECStatus
fips_hashBuf(HASH_HashType type, unsigned char *hashBuf,
             unsigned char *msg, int len)
{
    SECStatus rv = SECFailure;

    switch (type) {
        case HASH_AlgSHA1:
            rv = SHA1_HashBuf(hashBuf, msg, len);
            break;
        case HASH_AlgSHA224:
            rv = SHA224_HashBuf(hashBuf, msg, len);
            break;
        case HASH_AlgSHA256:
            rv = SHA256_HashBuf(hashBuf, msg, len);
            break;
        case HASH_AlgSHA384:
            rv = SHA384_HashBuf(hashBuf, msg, len);
            break;
        case HASH_AlgSHA512:
            rv = SHA512_HashBuf(hashBuf, msg, len);
            break;
        default:
            break;
    }
    return rv;
}

int
fips_hashLen(HASH_HashType type)
{
    int len = 0;

    switch (type) {
        case HASH_AlgSHA1:
            len = SHA1_LENGTH;
            break;
        case HASH_AlgSHA224:
            len = SHA224_LENGTH;
            break;
        case HASH_AlgSHA256:
            len = SHA256_LENGTH;
            break;
        case HASH_AlgSHA384:
            len = SHA384_LENGTH;
            break;
        case HASH_AlgSHA512:
            len = SHA512_LENGTH;
            break;
        default:
            break;
    }
    return len;
}

SECOidTag
fips_hashOid(HASH_HashType type)
{
    SECOidTag oid = SEC_OID_UNKNOWN;

    switch (type) {
        case HASH_AlgSHA1:
            oid = SEC_OID_SHA1;
            break;
        case HASH_AlgSHA224:
            oid = SEC_OID_SHA224;
            break;
        case HASH_AlgSHA256:
            oid = SEC_OID_SHA256;
            break;
        case HASH_AlgSHA384:
            oid = SEC_OID_SHA384;
            break;
        case HASH_AlgSHA512:
            oid = SEC_OID_SHA512;
            break;
        default:
            break;
    }
    return oid;
}

HASH_HashType
sha_get_hashType(int hashbits)
{
    HASH_HashType hashType = HASH_AlgNULL;

    switch (hashbits) {
        case 1:
        case (SHA1_LENGTH * PR_BITS_PER_BYTE):
            hashType = HASH_AlgSHA1;
            break;
        case (SHA224_LENGTH * PR_BITS_PER_BYTE):
            hashType = HASH_AlgSHA224;
            break;
        case (SHA256_LENGTH * PR_BITS_PER_BYTE):
            hashType = HASH_AlgSHA256;
            break;
        case (SHA384_LENGTH * PR_BITS_PER_BYTE):
            hashType = HASH_AlgSHA384;
            break;
        case (SHA512_LENGTH * PR_BITS_PER_BYTE):
            hashType = HASH_AlgSHA512;
            break;
        default:
            break;
    }
    return hashType;
}

HASH_HashType
hash_string_to_hashType(const char *src)
{
    HASH_HashType shaAlg = HASH_AlgNULL;
    if (strncmp(src, "SHA-1", 5) == 0) {
        shaAlg = HASH_AlgSHA1;
    } else if (strncmp(src, "SHA-224", 7) == 0) {
        shaAlg = HASH_AlgSHA224;
    } else if (strncmp(src, "SHA-256", 7) == 0) {
        shaAlg = HASH_AlgSHA256;
    } else if (strncmp(src, "SHA-384", 7) == 0) {
        shaAlg = HASH_AlgSHA384;
    } else if (strncmp(src, "SHA-512", 7) == 0) {
        shaAlg = HASH_AlgSHA512;
    } else if (strncmp(src, "SHA1", 4) == 0) {
        shaAlg = HASH_AlgSHA1;
    } else if (strncmp(src, "SHA224", 6) == 0) {
        shaAlg = HASH_AlgSHA224;
    } else if (strncmp(src, "SHA256", 6) == 0) {
        shaAlg = HASH_AlgSHA256;
    } else if (strncmp(src, "SHA384", 6) == 0) {
        shaAlg = HASH_AlgSHA384;
    } else if (strncmp(src, "SHA512", 6) == 0) {
        shaAlg = HASH_AlgSHA512;
    }
    return shaAlg;
}

/*
 * Perform the ECDSA Key Pair Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
ecdsa_keypair_test(char *reqfn)
{
    char buf[256];   /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * needs to be large enough to hold the longest
                         * line "Qx = <144 hex digits>\n".
                         */
    FILE *ecdsareq;  /* input stream from the REQUEST file */
    FILE *ecdsaresp; /* output stream to the RESPONSE file */
    char curve[16];  /* "nistxddd" */
    ECParams *ecparams = NULL;
    int N;
    int i;
    unsigned int len;

    ecdsareq = fopen(reqfn, "r");
    ecdsaresp = stdout;
    strcpy(curve, "nist");
    while (fgets(buf, sizeof buf, ecdsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ecdsaresp);
            continue;
        }
        /* [X-ddd] */
        if (buf[0] == '[') {
            const char *src;
            char *dst;
            SECItem *encodedparams;

            if (buf[1] == 'B') {
                fputs(buf, ecdsaresp);
                continue;
            }
            if (ecparams) {
                PORT_FreeArena(ecparams->arena, PR_FALSE);
                ecparams = NULL;
            }

            src = &buf[1];
            dst = &curve[4];
            *dst++ = tolower(*src);
            src += 2; /* skip the hyphen */
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst = '\0';
            encodedparams = getECParams(curve);
            if (encodedparams == NULL) {
                fprintf(stderr, "Unknown curve %s.", curve);
                goto loser;
            }
            if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
                fprintf(stderr, "Curve %s not supported.\n", curve);
                goto loser;
            }
            SECITEM_FreeItem(encodedparams, PR_TRUE);
            fputs(buf, ecdsaresp);
            continue;
        }
        /* N = x */
        if (buf[0] == 'N') {
            if (sscanf(buf, "N = %d", &N) != 1) {
                goto loser;
            }
            for (i = 0; i < N; i++) {
                ECPrivateKey *ecpriv;

                if (EC_NewKey(ecparams, &ecpriv) != SECSuccess) {
                    goto loser;
                }
                fputs("d = ", ecdsaresp);
                to_hex_str(buf, ecpriv->privateValue.data,
                           ecpriv->privateValue.len);
                fputs(buf, ecdsaresp);
                fputc('\n', ecdsaresp);
                if (EC_ValidatePublicKey(ecparams, &ecpriv->publicValue) !=
                    SECSuccess) {
                    goto loser;
                }
                len = ecpriv->publicValue.len;
                if (len % 2 == 0) {
                    goto loser;
                }
                len = (len - 1) / 2;
                if (ecpriv->publicValue.data[0] !=
                    EC_POINT_FORM_UNCOMPRESSED) {
                    goto loser;
                }
                fputs("Qx = ", ecdsaresp);
                to_hex_str(buf, &ecpriv->publicValue.data[1], len);
                fputs(buf, ecdsaresp);
                fputc('\n', ecdsaresp);
                fputs("Qy = ", ecdsaresp);
                to_hex_str(buf, &ecpriv->publicValue.data[1 + len], len);
                fputs(buf, ecdsaresp);
                fputc('\n', ecdsaresp);
                fputc('\n', ecdsaresp);
                PORT_FreeArena(ecpriv->ecParams.arena, PR_TRUE);
            }
            continue;
        }
    }
loser:
    if (ecparams) {
        PORT_FreeArena(ecparams->arena, PR_FALSE);
        ecparams = NULL;
    }
    fclose(ecdsareq);
}

/*
 * Perform the ECDSA Public Key Validation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
ecdsa_pkv_test(char *reqfn)
{
    char buf[256];   /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "Qx = <144 hex digits>\n".
                         */
    FILE *ecdsareq;  /* input stream from the REQUEST file */
    FILE *ecdsaresp; /* output stream to the RESPONSE file */
    char curve[16];  /* "nistxddd" */
    ECParams *ecparams = NULL;
    SECItem pubkey;
    unsigned int i;
    unsigned int len = 0;
    PRBool keyvalid = PR_TRUE;

    ecdsareq = fopen(reqfn, "r");
    ecdsaresp = stdout;
    strcpy(curve, "nist");
    pubkey.data = NULL;
    while (fgets(buf, sizeof buf, ecdsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ecdsaresp);
            continue;
        }
        /* [X-ddd] */
        if (buf[0] == '[') {
            const char *src;
            char *dst;
            SECItem *encodedparams;

            src = &buf[1];
            dst = &curve[4];
            *dst++ = tolower(*src);
            src += 2; /* skip the hyphen */
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst = '\0';
            if (ecparams != NULL) {
                PORT_FreeArena(ecparams->arena, PR_FALSE);
                ecparams = NULL;
            }
            encodedparams = getECParams(curve);
            if (encodedparams == NULL) {
                fprintf(stderr, "Unknown curve %s.", curve);
                goto loser;
            }
            if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
                fprintf(stderr, "Curve %s not supported.\n", curve);
                goto loser;
            }
            SECITEM_FreeItem(encodedparams, PR_TRUE);
            len = (ecparams->fieldID.size + 7) >> 3;
            if (pubkey.data != NULL) {
                PORT_Free(pubkey.data);
                pubkey.data = NULL;
            }
            SECITEM_AllocItem(NULL, &pubkey, EC_GetPointSize(ecparams));
            if (pubkey.data == NULL) {
                goto loser;
            }
            pubkey.data[0] = EC_POINT_FORM_UNCOMPRESSED;
            fputs(buf, ecdsaresp);
            continue;
        }
        /* Qx = ... */
        if (strncmp(buf, "Qx", 2) == 0) {
            fputs(buf, ecdsaresp);
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            keyvalid = from_hex_str(&pubkey.data[1], len, &buf[i]);
            continue;
        }
        /* Qy = ... */
        if (strncmp(buf, "Qy", 2) == 0) {
            fputs(buf, ecdsaresp);
            if (!keyvalid) {
                fputs("Result = F\n", ecdsaresp);
                continue;
            }
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            keyvalid = from_hex_str(&pubkey.data[1 + len], len, &buf[i]);
            if (!keyvalid) {
                fputs("Result = F\n", ecdsaresp);
                continue;
            }
            if (EC_ValidatePublicKey(ecparams, &pubkey) == SECSuccess) {
                fputs("Result = P\n", ecdsaresp);
            } else if (PORT_GetError() == SEC_ERROR_BAD_KEY) {
                fputs("Result = F\n", ecdsaresp);
            } else {
                goto loser;
            }
            continue;
        }
    }
loser:
    if (ecparams != NULL) {
        PORT_FreeArena(ecparams->arena, PR_FALSE);
    }
    if (pubkey.data != NULL) {
        PORT_Free(pubkey.data);
    }
    fclose(ecdsareq);
}

/*
 * Perform the ECDSA Signature Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
ecdsa_siggen_test(char *reqfn)
{
    char buf[1024];  /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * needs to be large enough to hold the longest
                         * line "Msg = <256 hex digits>\n".
                         */
    FILE *ecdsareq;  /* input stream from the REQUEST file */
    FILE *ecdsaresp; /* output stream to the RESPONSE file */
    char curve[16];  /* "nistxddd" */
    ECParams *ecparams = NULL;
    int i, j;
    unsigned int len;
    unsigned char msg[512]; /* message to be signed (<= 128 bytes) */
    unsigned int msglen;
    unsigned char sha[HASH_LENGTH_MAX];  /* SHA digest */
    unsigned int shaLength = 0;          /* length of SHA */
    HASH_HashType shaAlg = HASH_AlgNULL; /* type of SHA Alg */
    unsigned char sig[2 * MAX_ECKEY_LEN];
    SECItem signature, digest;

    ecdsareq = fopen(reqfn, "r");
    ecdsaresp = stdout;
    strcpy(curve, "nist");
    while (fgets(buf, sizeof buf, ecdsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ecdsaresp);
            continue;
        }
        /* [X-ddd] */
        if (buf[0] == '[') {
            const char *src;
            char *dst;
            SECItem *encodedparams;

            src = &buf[1];
            dst = &curve[4];
            *dst++ = tolower(*src);
            src += 2; /* skip the hyphen */
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst = '\0';
            src++; /* skip the comma */
            /* set the SHA Algorithm */
            shaAlg = hash_string_to_hashType(src);
            if (shaAlg == HASH_AlgNULL) {
                fprintf(ecdsaresp, "ERROR: Unable to find SHAAlg type");
                goto loser;
            }
            if (ecparams != NULL) {
                PORT_FreeArena(ecparams->arena, PR_FALSE);
                ecparams = NULL;
            }
            encodedparams = getECParams(curve);
            if (encodedparams == NULL) {
                fprintf(stderr, "Unknown curve %s.", curve);
                goto loser;
            }
            if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
                fprintf(stderr, "Curve %s not supported.\n", curve);
                goto loser;
            }
            SECITEM_FreeItem(encodedparams, PR_TRUE);
            fputs(buf, ecdsaresp);
            continue;
        }
        /* Msg = ... */
        if (strncmp(buf, "Msg", 3) == 0) {
            ECPrivateKey *ecpriv;

            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            msglen = j;
            shaLength = fips_hashLen(shaAlg);
            if (fips_hashBuf(shaAlg, sha, msg, msglen) != SECSuccess) {
                if (shaLength == 0) {
                    fprintf(ecdsaresp, "ERROR: SHAAlg not defined.");
                }
                fprintf(ecdsaresp, "ERROR: Unable to generate SHA%x",
                        shaLength == 160 ? 1 : shaLength);
                goto loser;
            }
            fputs(buf, ecdsaresp);

            if (EC_NewKey(ecparams, &ecpriv) != SECSuccess) {
                goto loser;
            }
            if (EC_ValidatePublicKey(ecparams, &ecpriv->publicValue) !=
                SECSuccess) {
                goto loser;
            }
            len = ecpriv->publicValue.len;
            if (len % 2 == 0) {
                goto loser;
            }
            len = (len - 1) / 2;
            if (ecpriv->publicValue.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
                goto loser;
            }
            fputs("Qx = ", ecdsaresp);
            to_hex_str(buf, &ecpriv->publicValue.data[1], len);
            fputs(buf, ecdsaresp);
            fputc('\n', ecdsaresp);
            fputs("Qy = ", ecdsaresp);
            to_hex_str(buf, &ecpriv->publicValue.data[1 + len], len);
            fputs(buf, ecdsaresp);
            fputc('\n', ecdsaresp);

            digest.type = siBuffer;
            digest.data = sha;
            digest.len = shaLength;
            signature.type = siBuffer;
            signature.data = sig;
            signature.len = sizeof sig;
            if (ECDSA_SignDigest(ecpriv, &signature, &digest) != SECSuccess) {
                goto loser;
            }
            len = signature.len;
            if (len % 2 != 0) {
                goto loser;
            }
            len = len / 2;
            fputs("R = ", ecdsaresp);
            to_hex_str(buf, &signature.data[0], len);
            fputs(buf, ecdsaresp);
            fputc('\n', ecdsaresp);
            fputs("S = ", ecdsaresp);
            to_hex_str(buf, &signature.data[len], len);
            fputs(buf, ecdsaresp);
            fputc('\n', ecdsaresp);

            PORT_FreeArena(ecpriv->ecParams.arena, PR_TRUE);
            continue;
        }
    }
loser:
    if (ecparams != NULL) {
        PORT_FreeArena(ecparams->arena, PR_FALSE);
    }
    fclose(ecdsareq);
}

/*
 * Perform the ECDSA Signature Verification Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
ecdsa_sigver_test(char *reqfn)
{
    char buf[1024];  /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "Msg = <256 hex digits>\n".
                         */
    FILE *ecdsareq;  /* input stream from the REQUEST file */
    FILE *ecdsaresp; /* output stream to the RESPONSE file */
    char curve[16];  /* "nistxddd" */
    ECPublicKey ecpub;
    unsigned int i, j;
    unsigned int flen = 0;  /* length in bytes of the field size */
    unsigned int olen = 0;  /* length in bytes of the base point order */
    unsigned char msg[512]; /* message that was signed (<= 128 bytes) */
    unsigned int msglen = 0;
    unsigned char sha[HASH_LENGTH_MAX];  /* SHA digest */
    unsigned int shaLength = 0;          /* length of SHA */
    HASH_HashType shaAlg = HASH_AlgNULL; /* type of SHA Alg */
    unsigned char sig[2 * MAX_ECKEY_LEN];
    SECItem signature, digest;
    PRBool keyvalid = PR_TRUE;
    PRBool sigvalid = PR_TRUE;

    ecdsareq = fopen(reqfn, "r");
    ecdsaresp = stdout;
    ecpub.ecParams.arena = NULL;
    strcpy(curve, "nist");
    while (fgets(buf, sizeof buf, ecdsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ecdsaresp);
            continue;
        }
        /* [X-ddd] */
        if (buf[0] == '[') {
            const char *src;
            char *dst;
            SECItem *encodedparams;
            ECParams *ecparams;

            src = &buf[1];
            dst = &curve[4];
            *dst++ = tolower(*src);
            src += 2; /* skip the hyphen */
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst = '\0';
            src++; /* skip the comma */
            /* set the SHA Algorithm */
            shaAlg = hash_string_to_hashType(src);
            if (shaAlg == HASH_AlgNULL) {
                fprintf(ecdsaresp, "ERROR: Unable to find SHAAlg type");
                goto loser;
            }
            encodedparams = getECParams(curve);
            if (encodedparams == NULL) {
                fprintf(stderr, "Unknown curve %s.", curve);
                goto loser;
            }
            if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
                fprintf(stderr, "Curve %s not supported.\n", curve);
                goto loser;
            }
            SECITEM_FreeItem(encodedparams, PR_TRUE);
            if (ecpub.ecParams.arena != NULL) {
                PORT_FreeArena(ecpub.ecParams.arena, PR_FALSE);
            }
            ecpub.ecParams.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
            if (ecpub.ecParams.arena == NULL) {
                goto loser;
            }
            if (EC_CopyParams(ecpub.ecParams.arena, &ecpub.ecParams, ecparams) !=
                SECSuccess) {
                goto loser;
            }
            PORT_FreeArena(ecparams->arena, PR_FALSE);
            flen = (ecpub.ecParams.fieldID.size + 7) >> 3;
            olen = ecpub.ecParams.order.len;
            if (2 * olen > sizeof sig) {
                goto loser;
            }
            ecpub.publicValue.type = siBuffer;
            ecpub.publicValue.data = NULL;
            ecpub.publicValue.len = 0;
            SECITEM_AllocItem(ecpub.ecParams.arena,
                              &ecpub.publicValue, 2 * flen + 1);
            if (ecpub.publicValue.data == NULL) {
                goto loser;
            }
            ecpub.publicValue.data[0] = EC_POINT_FORM_UNCOMPRESSED;
            fputs(buf, ecdsaresp);
            continue;
        }
        /* Msg = ... */
        if (strncmp(buf, "Msg", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            msglen = j;
            shaLength = fips_hashLen(shaAlg);
            if (fips_hashBuf(shaAlg, sha, msg, msglen) != SECSuccess) {
                if (shaLength == 0) {
                    fprintf(ecdsaresp, "ERROR: SHAAlg not defined.");
                }
                fprintf(ecdsaresp, "ERROR: Unable to generate SHA%x",
                        shaLength == 160 ? 1 : shaLength);
                goto loser;
            }
            fputs(buf, ecdsaresp);

            digest.type = siBuffer;
            digest.data = sha;
            digest.len = shaLength;

            continue;
        }
        /* Qx = ... */
        if (strncmp(buf, "Qx", 2) == 0) {
            fputs(buf, ecdsaresp);
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            keyvalid = from_hex_str(&ecpub.publicValue.data[1], flen,
                                    &buf[i]);
            continue;
        }
        /* Qy = ... */
        if (strncmp(buf, "Qy", 2) == 0) {
            fputs(buf, ecdsaresp);
            if (!keyvalid) {
                continue;
            }
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            keyvalid = from_hex_str(&ecpub.publicValue.data[1 + flen], flen,
                                    &buf[i]);
            if (!keyvalid) {
                continue;
            }
            if (EC_ValidatePublicKey(&ecpub.ecParams, &ecpub.publicValue) !=
                SECSuccess) {
                if (PORT_GetError() == SEC_ERROR_BAD_KEY) {
                    keyvalid = PR_FALSE;
                } else {
                    goto loser;
                }
            }
            continue;
        }
        /* R = ... */
        if (buf[0] == 'R') {
            fputs(buf, ecdsaresp);
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            sigvalid = from_hex_str(sig, olen, &buf[i]);
            continue;
        }
        /* S = ... */
        if (buf[0] == 'S') {
            fputs(buf, ecdsaresp);
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            if (sigvalid) {
                sigvalid = from_hex_str(&sig[olen], olen, &buf[i]);
            }
            signature.type = siBuffer;
            signature.data = sig;
            signature.len = 2 * olen;

            if (!keyvalid || !sigvalid) {
                fputs("Result = F\n", ecdsaresp);
            } else if (ECDSA_VerifyDigest(&ecpub, &signature, &digest) ==
                       SECSuccess) {
                fputs("Result = P\n", ecdsaresp);
            } else {
                fputs("Result = F\n", ecdsaresp);
            }
            continue;
        }
    }
loser:
    if (ecpub.ecParams.arena != NULL) {
        PORT_FreeArena(ecpub.ecParams.arena, PR_FALSE);
    }
    fclose(ecdsareq);
}

/*
 * Perform the ECDH Functional Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
#define MAX_ECC_PARAMS 256
void
ecdh_functional(char *reqfn, PRBool response)
{
    char buf[256];  /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "Qx = <144 hex digits>\n".
                         */
    FILE *ecdhreq;  /* input stream from the REQUEST file */
    FILE *ecdhresp; /* output stream to the RESPONSE file */
    char curve[16]; /* "nistxddd" */
    unsigned char hashBuf[HASH_LENGTH_MAX];
    ECParams *ecparams[MAX_ECC_PARAMS] = { NULL };
    ECPrivateKey *ecpriv = NULL;
    ECParams *current_ecparams = NULL;
    SECItem pubkey;
    SECItem ZZ;
    unsigned int i;
    unsigned int len = 0;
    unsigned int uit_len = 0;
    int current_curve = -1;
    HASH_HashType hash = HASH_AlgNULL; /* type of SHA Alg */

    ecdhreq = fopen(reqfn, "r");
    ecdhresp = stdout;
    strcpy(curve, "nist");
    pubkey.data = NULL;
    while (fgets(buf, sizeof buf, ecdhreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
            fputs(buf, ecdhresp);
            continue;
        }
        if (buf[0] == '[') {
            /* [Ex] */
            if (buf[1] == 'E' && buf[3] == ']') {
                current_curve = buf[2] - 'A';
                fputs(buf, ecdhresp);
                continue;
            }
            /* [Curve selected: x-nnn */
            if (strncmp(buf, "[Curve ", 7) == 0) {
                const char *src;
                char *dst;
                SECItem *encodedparams;

                if ((current_curve < 0) || (current_curve > MAX_ECC_PARAMS)) {
                    fprintf(stderr, "No curve type defined\n");
                    goto loser;
                }

                src = &buf[1];
                /* skip passed the colon */
                while (*src && *src != ':')
                    src++;
                if (*src != ':') {
                    fprintf(stderr,
                            "No colon in curve selected statement\n%s", buf);
                    goto loser;
                }
                src++;
                /* skip to the first non-space */
                while (*src && *src == ' ')
                    src++;
                dst = &curve[4];
                *dst++ = tolower(*src);
                src += 2; /* skip the hyphen */
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst = '\0';
                if (ecparams[current_curve] != NULL) {
                    PORT_FreeArena(ecparams[current_curve]->arena, PR_FALSE);
                    ecparams[current_curve] = NULL;
                }
                encodedparams = getECParams(curve);
                if (encodedparams == NULL) {
                    fprintf(stderr, "Unknown curve %s.", curve);
                    goto loser;
                }
                if (EC_DecodeParams(encodedparams, &ecparams[current_curve]) != SECSuccess) {
                    fprintf(stderr, "Curve %s not supported.\n", curve);
                    goto loser;
                }
                SECITEM_FreeItem(encodedparams, PR_TRUE);
                fputs(buf, ecdhresp);
                continue;
            }
            /* [Ex - SHAxxx] */
            if (buf[1] == 'E' && buf[3] == ' ') {
                const char *src;
                current_curve = buf[2] - 'A';
                if ((current_curve < 0) || (current_curve > 256)) {
                    fprintf(stderr, "bad curve type defined (%c)\n", buf[2]);
                    goto loser;
                }
                current_ecparams = ecparams[current_curve];
                if (current_ecparams == NULL) {
                    fprintf(stderr, "no curve defined for type %c defined\n",
                            buf[2]);
                    goto loser;
                }
                /* skip passed the colon */
                src = &buf[1];
                while (*src && *src != '-')
                    src++;
                if (*src != '-') {
                    fprintf(stderr,
                            "No data in curve selected statement\n%s", buf);
                    goto loser;
                }
                src++;
                /* skip to the first non-space */
                while (*src && *src == ' ')
                    src++;
                hash = hash_string_to_hashType(src);
                if (hash == HASH_AlgNULL) {
                    fprintf(ecdhresp, "ERROR: Unable to find SHAAlg type");
                    goto loser;
                }
                fputs(buf, ecdhresp);
                continue;
            }
            fputs(buf, ecdhresp);
            continue;
        }
        /* COUNT = ... */
        if (strncmp(buf, "COUNT", 5) == 0) {
            fputs(buf, ecdhresp);
            if (current_ecparams == NULL) {
                fprintf(stderr, "no curve defined for type %c defined\n",
                        buf[2]);
                goto loser;
            }
            len = (current_ecparams->fieldID.size + 7) >> 3;
            if (pubkey.data != NULL) {
                PORT_Free(pubkey.data);
                pubkey.data = NULL;
            }
            SECITEM_AllocItem(NULL, &pubkey, EC_GetPointSize(current_ecparams));
            if (pubkey.data == NULL) {
                goto loser;
            }
            pubkey.data[0] = EC_POINT_FORM_UNCOMPRESSED;
            continue;
        }
        /* QeCAVSx = ... */
        if (strncmp(buf, "QeCAVSx", 7) == 0) {
            fputs(buf, ecdhresp);
            i = 7;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(&pubkey.data[1], len, &buf[i]);
            continue;
        }
        /* QeCAVSy = ... */
        if (strncmp(buf, "QeCAVSy", 7) == 0) {
            fputs(buf, ecdhresp);
            i = 7;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(&pubkey.data[1 + len], len, &buf[i]);
            if (current_ecparams == NULL) {
                fprintf(stderr, "no curve defined\n");
                goto loser;
            }
            /* validate CAVS public key */
            if (EC_ValidatePublicKey(current_ecparams, &pubkey) != SECSuccess) {
                fprintf(stderr, "BAD key detected\n");
                goto loser;
            }

            /* generate ECC key pair */
            if (EC_NewKey(current_ecparams, &ecpriv) != SECSuccess) {
                fprintf(stderr, "Failed to generate new key\n");
                goto loser;
            }
            /* validate UIT generated public key */
            if (EC_ValidatePublicKey(current_ecparams, &ecpriv->publicValue) !=
                SECSuccess) {
                fprintf(stderr, "generate key did not validate\n");
                goto loser;
            }
            /* output UIT public key */
            uit_len = ecpriv->publicValue.len;
            if (uit_len % 2 == 0) {
                fprintf(stderr, "generate key had invalid public value len\n");
                goto loser;
            }
            uit_len = (uit_len - 1) / 2;
            if (ecpriv->publicValue.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
                fprintf(stderr, "generate key was compressed\n");
                goto loser;
            }
            fputs("deIUT = ", ecdhresp);
            to_hex_str(buf, ecpriv->privateValue.data, ecpriv->privateValue.len);
            fputs(buf, ecdhresp);
            fputc('\n', ecdhresp);
            fputs("QeIUTx = ", ecdhresp);
            to_hex_str(buf, &ecpriv->publicValue.data[1], uit_len);
            fputs(buf, ecdhresp);
            fputc('\n', ecdhresp);
            fputs("QeIUTy = ", ecdhresp);
            to_hex_str(buf, &ecpriv->publicValue.data[1 + uit_len], uit_len);
            fputs(buf, ecdhresp);
            fputc('\n', ecdhresp);
            /* ECDH */
            if (ECDH_Derive(&pubkey, current_ecparams, &ecpriv->privateValue,
                            PR_FALSE, &ZZ) != SECSuccess) {
                fprintf(stderr, "Derive failed\n");
                goto loser;
            }
            /* output hash of ZZ */
            if (fips_hashBuf(hash, hashBuf, ZZ.data, ZZ.len) != SECSuccess) {
                fprintf(stderr, "hash of derived key failed\n");
                goto loser;
            }
            SECITEM_FreeItem(&ZZ, PR_FALSE);
            fputs("HashZZ = ", ecdhresp);
            to_hex_str(buf, hashBuf, fips_hashLen(hash));
            fputs(buf, ecdhresp);
            fputc('\n', ecdhresp);
            fputc('\n', ecdhresp);
            PORT_FreeArena(ecpriv->ecParams.arena, PR_TRUE);
            ecpriv = NULL;
            continue;
        }
    }
loser:
    if (ecpriv != NULL) {
        PORT_FreeArena(ecpriv->ecParams.arena, PR_TRUE);
    }
    for (i = 0; i < MAX_ECC_PARAMS; i++) {
        if (ecparams[i] != NULL) {
            PORT_FreeArena(ecparams[i]->arena, PR_FALSE);
            ecparams[i] = NULL;
        }
    }
    if (pubkey.data != NULL) {
        PORT_Free(pubkey.data);
    }
    fclose(ecdhreq);
}

/*
 * Perform the ECDH Validity Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
ecdh_verify(char *reqfn, PRBool response)
{
    char buf[256];  /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "Qx = <144 hex digits>\n".
                         */
    FILE *ecdhreq;  /* input stream from the REQUEST file */
    FILE *ecdhresp; /* output stream to the RESPONSE file */
    char curve[16]; /* "nistxddd" */
    unsigned char hashBuf[HASH_LENGTH_MAX];
    unsigned char cavsHashBuf[HASH_LENGTH_MAX];
    unsigned char private_data[MAX_ECKEY_LEN];
    ECParams *ecparams[MAX_ECC_PARAMS] = { NULL };
    ECParams *current_ecparams = NULL;
    SECItem pubkey;
    SECItem ZZ;
    SECItem private_value;
    unsigned int i;
    unsigned int len = 0;
    int current_curve = -1;
    HASH_HashType hash = HASH_AlgNULL; /* type of SHA Alg */

    ecdhreq = fopen(reqfn, "r");
    ecdhresp = stdout;
    strcpy(curve, "nist");
    pubkey.data = NULL;
    while (fgets(buf, sizeof buf, ecdhreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
            fputs(buf, ecdhresp);
            continue;
        }
        if (buf[0] == '[') {
            /* [Ex] */
            if (buf[1] == 'E' && buf[3] == ']') {
                current_curve = buf[2] - 'A';
                fputs(buf, ecdhresp);
                continue;
            }
            /* [Curve selected: x-nnn */
            if (strncmp(buf, "[Curve ", 7) == 0) {
                const char *src;
                char *dst;
                SECItem *encodedparams;

                if ((current_curve < 0) || (current_curve > MAX_ECC_PARAMS)) {
                    fprintf(stderr, "No curve type defined\n");
                    goto loser;
                }

                src = &buf[1];
                /* skip passed the colon */
                while (*src && *src != ':')
                    src++;
                if (*src != ':') {
                    fprintf(stderr,
                            "No colon in curve selected statement\n%s", buf);
                    goto loser;
                }
                src++;
                /* skip to the first non-space */
                while (*src && *src == ' ')
                    src++;
                dst = &curve[4];
                *dst++ = tolower(*src);
                src += 2; /* skip the hyphen */
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst = '\0';
                if (ecparams[current_curve] != NULL) {
                    PORT_FreeArena(ecparams[current_curve]->arena, PR_FALSE);
                    ecparams[current_curve] = NULL;
                }
                encodedparams = getECParams(curve);
                if (encodedparams == NULL) {
                    fprintf(stderr, "Unknown curve %s.\n", curve);
                    goto loser;
                }
                if (EC_DecodeParams(encodedparams, &ecparams[current_curve]) != SECSuccess) {
                    fprintf(stderr, "Curve %s not supported.\n", curve);
                    goto loser;
                }
                SECITEM_FreeItem(encodedparams, PR_TRUE);
                fputs(buf, ecdhresp);
                continue;
            }
            /* [Ex - SHAxxx] */
            if (buf[1] == 'E' && buf[3] == ' ') {
                const char *src;
                current_curve = buf[2] - 'A';
                if ((current_curve < 0) || (current_curve > 256)) {
                    fprintf(stderr, "bad curve type defined (%c)\n", buf[2]);
                    goto loser;
                }
                current_ecparams = ecparams[current_curve];
                if (current_ecparams == NULL) {
                    fprintf(stderr, "no curve defined for type %c defined\n",
                            buf[2]);
                    goto loser;
                }
                /* skip passed the colon */
                src = &buf[1];
                while (*src && *src != '-')
                    src++;
                if (*src != '-') {
                    fprintf(stderr,
                            "No data in curve selected statement\n%s", buf);
                    goto loser;
                }
                src++;
                /* skip to the first non-space */
                while (*src && *src == ' ')
                    src++;
                hash = hash_string_to_hashType(src);
                if (hash == HASH_AlgNULL) {
                    fprintf(ecdhresp, "ERROR: Unable to find SHAAlg type");
                    goto loser;
                }
                fputs(buf, ecdhresp);
                continue;
            }
            fputs(buf, ecdhresp);
            continue;
        }
        /* COUNT = ... */
        if (strncmp(buf, "COUNT", 5) == 0) {
            fputs(buf, ecdhresp);
            if (current_ecparams == NULL) {
                fprintf(stderr, "no curve defined for type %c defined\n",
                        buf[2]);
                goto loser;
            }
            len = (current_ecparams->fieldID.size + 7) >> 3;
            if (pubkey.data != NULL) {
                PORT_Free(pubkey.data);
                pubkey.data = NULL;
            }
            SECITEM_AllocItem(NULL, &pubkey, EC_GetPointSize(current_ecparams));
            if (pubkey.data == NULL) {
                goto loser;
            }
            pubkey.data[0] = EC_POINT_FORM_UNCOMPRESSED;
            continue;
        }
        /* QeCAVSx = ... */
        if (strncmp(buf, "QeCAVSx", 7) == 0) {
            fputs(buf, ecdhresp);
            i = 7;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(&pubkey.data[1], len, &buf[i]);
            continue;
        }
        /* QeCAVSy = ... */
        if (strncmp(buf, "QeCAVSy", 7) == 0) {
            fputs(buf, ecdhresp);
            i = 7;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(&pubkey.data[1 + len], len, &buf[i]);
            continue;
        }
        if (strncmp(buf, "deIUT", 5) == 0) {
            fputs(buf, ecdhresp);
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(private_data, len, &buf[i]);
            private_value.data = private_data;
            private_value.len = len;
            continue;
        }
        if (strncmp(buf, "QeIUTx", 6) == 0) {
            fputs(buf, ecdhresp);
            continue;
        }
        if (strncmp(buf, "QeIUTy", 6) == 0) {
            fputs(buf, ecdhresp);
            continue;
        }
        if ((strncmp(buf, "CAVSHashZZ", 10) == 0) ||
            (strncmp(buf, "HashZZ", 6) == 0)) {
            fputs(buf, ecdhresp);
            i = (buf[0] == 'C') ? 10 : 6;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(cavsHashBuf, fips_hashLen(hash), &buf[i]);
            if (current_ecparams == NULL) {
                fprintf(stderr, "no curve defined for type defined\n");
                goto loser;
            }
            /* validate CAVS public key */
            if (EC_ValidatePublicKey(current_ecparams, &pubkey) != SECSuccess) {
#ifdef VERBOSE_REASON
                fprintf(ecdhresp, "Result = F # key didn't validate\n");
#else
                fprintf(ecdhresp, "Result = F\n");
#endif
                continue;
            }

            /* ECDH */
            if (ECDH_Derive(&pubkey, current_ecparams, &private_value,
                            PR_FALSE, &ZZ) != SECSuccess) {
#ifdef VERBOSE_REASON
                fprintf(ecdhresp, "Result = F # derive failure\n");
#else
                fprintf(ecdhresp, "Result = F\n");
#endif
                continue;
            }
/* output  ZZ */
#ifndef MATCH_OPENSSL
            fputs("Z = ", ecdhresp);
            to_hex_str(buf, ZZ.data, ZZ.len);
            fputs(buf, ecdhresp);
            fputc('\n', ecdhresp);
#endif

            if (fips_hashBuf(hash, hashBuf, ZZ.data, ZZ.len) != SECSuccess) {
                fprintf(stderr, "hash of derived key failed\n");
                goto loser;
            }
            SECITEM_FreeItem(&ZZ, PR_FALSE);
#ifndef MATCH_NIST
            fputs("IUTHashZZ = ", ecdhresp);
            to_hex_str(buf, hashBuf, fips_hashLen(hash));
            fputs(buf, ecdhresp);
            fputc('\n', ecdhresp);
#endif
            if (memcmp(hashBuf, cavsHashBuf, fips_hashLen(hash)) != 0) {
#ifdef VERBOSE_REASON
                fprintf(ecdhresp, "Result = F # hash doesn't match\n");
#else
                fprintf(ecdhresp, "Result = F\n");
#endif
            } else {
                fprintf(ecdhresp, "Result = P\n");
            }
#ifndef MATCH_OPENSSL
            fputc('\n', ecdhresp);
#endif
            continue;
        }
    }
loser:
    for (i = 0; i < MAX_ECC_PARAMS; i++) {
        if (ecparams[i] != NULL) {
            PORT_FreeArena(ecparams[i]->arena, PR_FALSE);
            ecparams[i] = NULL;
        }
    }
    if (pubkey.data != NULL) {
        PORT_Free(pubkey.data);
    }
    fclose(ecdhreq);
}

/*
 * Perform the DH Functional Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
#define MAX_ECC_PARAMS 256
void
dh_functional(char *reqfn, PRBool response)
{
    char buf[1024]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "YephCAVS = <512 hex digits>\n".
                         */
    FILE *dhreq;    /* input stream from the REQUEST file */
    FILE *dhresp;   /* output stream to the RESPONSE file */
    unsigned char hashBuf[HASH_LENGTH_MAX];
    DSAPrivateKey *dsapriv = NULL;
    PQGParams pqg = { 0 };
    unsigned char pubkeydata[DSA_MAX_P_BITS / 8];
    SECItem pubkey;
    SECItem ZZ;
    unsigned int i, j;
    unsigned int pgySize;
    HASH_HashType hash = HASH_AlgNULL; /* type of SHA Alg */

    dhreq = fopen(reqfn, "r");
    dhresp = stdout;
    while (fgets(buf, sizeof buf, dhreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
            fputs(buf, dhresp);
            continue;
        }
        if (buf[0] == '[') {
            /* [Fx - SHAxxx] */
            if (buf[1] == 'F' && buf[3] == ' ') {
                const char *src;
                /* skip passed the colon */
                src = &buf[1];
                while (*src && *src != '-')
                    src++;
                if (*src != '-') {
                    fprintf(stderr, "No hash specified\n%s", buf);
                    goto loser;
                }
                src++;
                /* skip to the first non-space */
                while (*src && *src == ' ')
                    src++;
                hash = hash_string_to_hashType(src);
                if (hash == HASH_AlgNULL) {
                    fprintf(dhresp, "ERROR: Unable to find SHAAlg type");
                    goto loser;
                }
                /* clear the PQG parameters */
                if (pqg.prime.data) { /* P */
                    SECITEM_ZfreeItem(&pqg.prime, PR_FALSE);
                }
                if (pqg.subPrime.data) { /* Q */
                    SECITEM_ZfreeItem(&pqg.subPrime, PR_FALSE);
                }
                if (pqg.base.data) { /* G */
                    SECITEM_ZfreeItem(&pqg.base, PR_FALSE);
                }
                pgySize = DSA_MAX_P_BITS / 8; /* change if more key sizes are supported in CAVS */
                SECITEM_AllocItem(NULL, &pqg.prime, pgySize);
                SECITEM_AllocItem(NULL, &pqg.base, pgySize);
                pqg.prime.len = pqg.base.len = pgySize;

                /* set q to the max allows */
                SECITEM_AllocItem(NULL, &pqg.subPrime, DSA_MAX_Q_BITS / 8);
                pqg.subPrime.len = DSA_MAX_Q_BITS / 8;
                fputs(buf, dhresp);
                continue;
            }
            fputs(buf, dhresp);
            continue;
        }
        if (buf[0] == 'P') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.prime.len; i += 2, j++) {
                if (!isxdigit(buf[i])) {
                    pqg.prime.len = j;
                    break;
                }
                hex_to_byteval(&buf[i], &pqg.prime.data[j]);
            }

            fputs(buf, dhresp);
            continue;
        }

        /* Q = ... */
        if (buf[0] == 'Q') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.subPrime.len; i += 2, j++) {
                if (!isxdigit(buf[i])) {
                    pqg.subPrime.len = j;
                    break;
                }
                hex_to_byteval(&buf[i], &pqg.subPrime.data[j]);
            }

            fputs(buf, dhresp);
            continue;
        }

        /* G = ... */
        if (buf[0] == 'G') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.base.len; i += 2, j++) {
                if (!isxdigit(buf[i])) {
                    pqg.base.len = j;
                    break;
                }
                hex_to_byteval(&buf[i], &pqg.base.data[j]);
            }

            fputs(buf, dhresp);
            continue;
        }

        /* COUNT = ... */
        if (strncmp(buf, "COUNT", 5) == 0) {
            fputs(buf, dhresp);
            continue;
        }

        /* YephemCAVS = ... */
        if (strncmp(buf, "YephemCAVS", 10) == 0) {
            fputs(buf, dhresp);
            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(pubkeydata, pqg.prime.len, &buf[i]);
            pubkey.data = pubkeydata;
            pubkey.len = pqg.prime.len;

            /* generate FCC key pair, nist uses pqg rather then pg,
             * so use DSA to generate the key */
            if (DSA_NewKey(&pqg, &dsapriv) != SECSuccess) {
                fprintf(stderr, "Failed to generate new key\n");
                goto loser;
            }
            fputs("XephemIUT = ", dhresp);
            to_hex_str(buf, dsapriv->privateValue.data, dsapriv->privateValue.len);
            fputs(buf, dhresp);
            fputc('\n', dhresp);
            fputs("YephemIUT = ", dhresp);
            to_hex_str(buf, dsapriv->publicValue.data, dsapriv->publicValue.len);
            fputs(buf, dhresp);
            fputc('\n', dhresp);
            /* DH */
            if (DH_Derive(&pubkey, &pqg.prime, &dsapriv->privateValue,
                          &ZZ, pqg.prime.len) != SECSuccess) {
                fprintf(stderr, "Derive failed\n");
                goto loser;
            }
            /* output hash of ZZ */
            if (fips_hashBuf(hash, hashBuf, ZZ.data, ZZ.len) != SECSuccess) {
                fprintf(stderr, "hash of derived key failed\n");
                goto loser;
            }
            SECITEM_FreeItem(&ZZ, PR_FALSE);
            fputs("HashZZ = ", dhresp);
            to_hex_str(buf, hashBuf, fips_hashLen(hash));
            fputs(buf, dhresp);
            fputc('\n', dhresp);
            fputc('\n', dhresp);
            PORT_FreeArena(dsapriv->params.arena, PR_TRUE);
            dsapriv = NULL;
            continue;
        }
    }
loser:
    if (dsapriv != NULL) {
        PORT_FreeArena(dsapriv->params.arena, PR_TRUE);
    }
    fclose(dhreq);
}

/*
 * Perform the DH Validity Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
dh_verify(char *reqfn, PRBool response)
{
    char buf[1024]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "YephCAVS = <512 hex digits>\n".
                         */
    FILE *dhreq;    /* input stream from the REQUEST file */
    FILE *dhresp;   /* output stream to the RESPONSE file */
    unsigned char hashBuf[HASH_LENGTH_MAX];
    unsigned char cavsHashBuf[HASH_LENGTH_MAX];
    PQGParams pqg = { 0 };
    unsigned char pubkeydata[DSA_MAX_P_BITS / 8];
    unsigned char privkeydata[DSA_MAX_P_BITS / 8];
    SECItem pubkey;
    SECItem privkey;
    SECItem ZZ;
    unsigned int i, j;
    unsigned int pgySize;
    HASH_HashType hash = HASH_AlgNULL; /* type of SHA Alg */

    dhreq = fopen(reqfn, "r");
    dhresp = stdout;
    while (fgets(buf, sizeof buf, dhreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
            fputs(buf, dhresp);
            continue;
        }
        if (buf[0] == '[') {
            /* [Fx - SHAxxx] */
            if (buf[1] == 'F' && buf[3] == ' ') {
                const char *src;
                /* skip passed the colon */
                src = &buf[1];
                while (*src && *src != '-')
                    src++;
                if (*src != '-') {
                    fprintf(stderr, "No hash specified\n%s", buf);
                    goto loser;
                }
                src++;
                /* skip to the first non-space */
                while (*src && *src == ' ')
                    src++;
                hash = hash_string_to_hashType(src);
                if (hash == HASH_AlgNULL) {
                    fprintf(dhresp, "ERROR: Unable to find SHAAlg type");
                    goto loser;
                }
                /* clear the PQG parameters */
                if (pqg.prime.data) { /* P */
                    SECITEM_ZfreeItem(&pqg.prime, PR_FALSE);
                }
                if (pqg.subPrime.data) { /* Q */
                    SECITEM_ZfreeItem(&pqg.subPrime, PR_FALSE);
                }
                if (pqg.base.data) { /* G */
                    SECITEM_ZfreeItem(&pqg.base, PR_FALSE);
                }
                pgySize = DSA_MAX_P_BITS / 8; /* change if more key sizes are supported in CAVS */
                SECITEM_AllocItem(NULL, &pqg.prime, pgySize);
                SECITEM_AllocItem(NULL, &pqg.base, pgySize);
                pqg.prime.len = pqg.base.len = pgySize;

                /* set q to the max allows */
                SECITEM_AllocItem(NULL, &pqg.subPrime, DSA_MAX_Q_BITS / 8);
                pqg.subPrime.len = DSA_MAX_Q_BITS / 8;
                fputs(buf, dhresp);
                continue;
            }
            fputs(buf, dhresp);
            continue;
        }
        if (buf[0] == 'P') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.prime.len; i += 2, j++) {
                if (!isxdigit(buf[i])) {
                    pqg.prime.len = j;
                    break;
                }
                hex_to_byteval(&buf[i], &pqg.prime.data[j]);
            }

            fputs(buf, dhresp);
            continue;
        }

        /* Q = ... */
        if (buf[0] == 'Q') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.subPrime.len; i += 2, j++) {
                if (!isxdigit(buf[i])) {
                    pqg.subPrime.len = j;
                    break;
                }
                hex_to_byteval(&buf[i], &pqg.subPrime.data[j]);
            }

            fputs(buf, dhresp);
            continue;
        }

        /* G = ... */
        if (buf[0] == 'G') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.base.len; i += 2, j++) {
                if (!isxdigit(buf[i])) {
                    pqg.base.len = j;
                    break;
                }
                hex_to_byteval(&buf[i], &pqg.base.data[j]);
            }

            fputs(buf, dhresp);
            continue;
        }

        /* COUNT = ... */
        if (strncmp(buf, "COUNT", 5) == 0) {
            fputs(buf, dhresp);
            continue;
        }

        /* YephemCAVS = ... */
        if (strncmp(buf, "YephemCAVS", 10) == 0) {
            fputs(buf, dhresp);
            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(pubkeydata, pqg.prime.len, &buf[i]);
            pubkey.data = pubkeydata;
            pubkey.len = pqg.prime.len;
            continue;
        }
        /* XephemUIT = ... */
        if (strncmp(buf, "XephemIUT", 9) == 0) {
            fputs(buf, dhresp);
            i = 9;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(privkeydata, pqg.subPrime.len, &buf[i]);
            privkey.data = privkeydata;
            privkey.len = pqg.subPrime.len;
            continue;
        }
        /* YephemUIT = ... */
        if (strncmp(buf, "YephemIUT", 9) == 0) {
            fputs(buf, dhresp);
            continue;
        }
        /* CAVSHashZZ = ... */
        if ((strncmp(buf, "CAVSHashZZ", 10) == 0) ||
            (strncmp(buf, "HashZZ", 6) == 0)) {
            fputs(buf, dhresp);
            i = buf[0] == 'C' ? 10 : 6;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            from_hex_str(cavsHashBuf, fips_hashLen(hash), &buf[i]);
            /* do the DH operation*/
            if (DH_Derive(&pubkey, &pqg.prime, &privkey,
                          &ZZ, pqg.prime.len) != SECSuccess) {
                fprintf(stderr, "Derive failed\n");
                goto loser;
            }
/* output  ZZ */
#ifndef MATCH_OPENSSL
            fputs("Z = ", dhresp);
            to_hex_str(buf, ZZ.data, ZZ.len);
            fputs(buf, dhresp);
            fputc('\n', dhresp);
#endif
            if (fips_hashBuf(hash, hashBuf, ZZ.data, ZZ.len) != SECSuccess) {
                fprintf(stderr, "hash of derived key failed\n");
                goto loser;
            }
            SECITEM_FreeItem(&ZZ, PR_FALSE);
#ifndef MATCH_NIST
            fputs("IUTHashZZ = ", dhresp);
            to_hex_str(buf, hashBuf, fips_hashLen(hash));
            fputs(buf, dhresp);
            fputc('\n', dhresp);
#endif
            if (memcmp(hashBuf, cavsHashBuf, fips_hashLen(hash)) != 0) {
                fprintf(dhresp, "Result = F\n");
            } else {
                fprintf(dhresp, "Result = P\n");
            }
#ifndef MATCH_OPENSSL
            fputc('\n', dhresp);
#endif
            continue;
        }
    }
loser:
    fclose(dhreq);
}

PRBool
isblankline(char *b)
{
    while (isspace(*b))
        b++;
    if ((*b == '\n') || (*b == 0)) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

static int debug = 0;

/*
 * Perform the Hash_DRBG (CAVS) for the RNG algorithm
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
drbg(char *reqfn)
{
    char buf[2000]; /* test case has some very long lines, returned bits
     * as high as 800 bytes (6400 bits). That 1600 byte
     * plus a tag */
    char buf2[2000];
    FILE *rngreq;  /* input stream from the REQUEST file */
    FILE *rngresp; /* output stream to the RESPONSE file */

    unsigned int i, j;
#ifdef HANDLE_PREDICTION_RESISTANCE
    PRBool predictionResistance = PR_FALSE;
#endif
    unsigned char *nonce = NULL;
    int nonceLen = 0;
    unsigned char *personalizationString = NULL;
    int personalizationStringLen = 0;
    unsigned char *additionalInput = NULL;
    int additionalInputLen = 0;
    unsigned char *entropyInput = NULL;
    int entropyInputLen = 0;
    unsigned char *predictedreturn_bytes = NULL;
    unsigned char *return_bytes = NULL;
    int return_bytes_len = 0;
    enum { NONE,
           INSTANTIATE,
           GENERATE,
           RESEED,
           RESULT } command =
        NONE;
    PRBool genResult = PR_FALSE;
    SECStatus rv;

    rngreq = fopen(reqfn, "r");
    rngresp = stdout;
    while (fgets(buf, sizeof buf, rngreq) != NULL) {
        switch (command) {
            case INSTANTIATE:
                if (debug) {
                    fputs("# PRNGTEST_Instantiate(", rngresp);
                    to_hex_str(buf2, entropyInput, entropyInputLen);
                    fputs(buf2, rngresp);
                    fprintf(rngresp, ",%d,", entropyInputLen);
                    to_hex_str(buf2, nonce, nonceLen);
                    fputs(buf2, rngresp);
                    fprintf(rngresp, ",%d,", nonceLen);
                    to_hex_str(buf2, personalizationString,
                               personalizationStringLen);
                    fputs(buf2, rngresp);
                    fprintf(rngresp, ",%d)\n", personalizationStringLen);
                }
                rv = PRNGTEST_Instantiate(entropyInput, entropyInputLen,
                                          nonce, nonceLen,
                                          personalizationString,
                                          personalizationStringLen);
                if (rv != SECSuccess) {
                    goto loser;
                }
                break;

            case GENERATE:
            case RESULT:
                memset(return_bytes, 0, return_bytes_len);
                if (debug) {
                    fputs("# PRNGTEST_Generate(returnbytes", rngresp);
                    fprintf(rngresp, ",%d,", return_bytes_len);
                    to_hex_str(buf2, additionalInput, additionalInputLen);
                    fputs(buf2, rngresp);
                    fprintf(rngresp, ",%d)\n", additionalInputLen);
                }
                rv = PRNGTEST_Generate((PRUint8 *)return_bytes,
                                       return_bytes_len,
                                       (PRUint8 *)additionalInput,
                                       additionalInputLen);
                if (rv != SECSuccess) {
                    goto loser;
                }

                if (command == RESULT) {
                    fputs("ReturnedBits = ", rngresp);
                    to_hex_str(buf2, return_bytes, return_bytes_len);
                    fputs(buf2, rngresp);
                    fputc('\n', rngresp);
                    if (debug) {
                        fputs("# PRNGTEST_Uninstantiate()\n", rngresp);
                    }
                    rv = PRNGTEST_Uninstantiate();
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                } else if (debug) {
                    fputs("#ReturnedBits = ", rngresp);
                    to_hex_str(buf2, return_bytes, return_bytes_len);
                    fputs(buf2, rngresp);
                    fputc('\n', rngresp);
                }

                memset(additionalInput, 0, additionalInputLen);
                break;

            case RESEED:
                if (entropyInput || additionalInput) {
                    if (debug) {
                        fputs("# PRNGTEST_Reseed(", rngresp);
                        fprintf(rngresp, ",%d,", return_bytes_len);
                        to_hex_str(buf2, entropyInput, entropyInputLen);
                        fputs(buf2, rngresp);
                        fprintf(rngresp, ",%d,", entropyInputLen);
                        to_hex_str(buf2, additionalInput, additionalInputLen);
                        fputs(buf2, rngresp);
                        fprintf(rngresp, ",%d)\n", additionalInputLen);
                    }
                    rv = PRNGTEST_Reseed(entropyInput, entropyInputLen,
                                         additionalInput, additionalInputLen);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                }
                memset(entropyInput, 0, entropyInputLen);
                memset(additionalInput, 0, additionalInputLen);
                break;
            case NONE:
                break;
        }
        command = NONE;

        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
            fputs(buf, rngresp);
            continue;
        }

        /* [Hash - SHA256] */
        if (strncmp(buf, "[SHA-256]", 9) == 0) {
            fputs(buf, rngresp);
            continue;
        }

        if (strncmp(buf, "[PredictionResistance", 21) == 0) {
#ifdef HANDLE_PREDICTION_RESISTANCE
            i = 21;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            if (strncmp(buf, "False", 5) == 0) {
                predictionResistance = PR_FALSE;
            } else {
                predictionResistance = PR_TRUE;
            }
#endif

            fputs(buf, rngresp);
            continue;
        }

        if (strncmp(buf, "[ReturnedBitsLen", 16) == 0) {
            if (return_bytes) {
                PORT_ZFree(return_bytes, return_bytes_len);
                return_bytes = NULL;
            }
            if (predictedreturn_bytes) {
                PORT_ZFree(predictedreturn_bytes, return_bytes_len);
                predictedreturn_bytes = NULL;
            }
            return_bytes_len = 0;
            if (sscanf(buf, "[ReturnedBitsLen = %d]", &return_bytes_len) != 1) {
                goto loser;
            }
            return_bytes_len = return_bytes_len / 8;
            if (return_bytes_len > 0) {
                return_bytes = PORT_Alloc(return_bytes_len);
                predictedreturn_bytes = PORT_Alloc(return_bytes_len);
            }
            fputs(buf, rngresp);
            continue;
        }

        if (strncmp(buf, "[EntropyInputLen", 16) == 0) {
            if (entropyInput) {
                PORT_ZFree(entropyInput, entropyInputLen);
                entropyInput = NULL;
                entropyInputLen = 0;
            }
            if (sscanf(buf, "[EntropyInputLen = %d]", &entropyInputLen) != 1) {
                goto loser;
            }
            entropyInputLen = entropyInputLen / 8;
            if (entropyInputLen > 0) {
                entropyInput = PORT_Alloc(entropyInputLen);
            }
            fputs(buf, rngresp);
            continue;
        }

        if (strncmp(buf, "[NonceLen", 9) == 0) {
            if (nonce) {
                PORT_ZFree(nonce, nonceLen);
                nonce = NULL;
                nonceLen = 0;
            }

            if (sscanf(buf, "[NonceLen = %d]", &nonceLen) != 1) {
                goto loser;
            }
            nonceLen = nonceLen / 8;
            if (nonceLen > 0) {
                nonce = PORT_Alloc(nonceLen);
            }
            fputs(buf, rngresp);
            continue;
        }

        if (strncmp(buf, "[PersonalizationStringLen", 16) == 0) {
            if (personalizationString) {
                PORT_ZFree(personalizationString, personalizationStringLen);
                personalizationString = NULL;
                personalizationStringLen = 0;
            }

            if (sscanf(buf, "[PersonalizationStringLen = %d]", &personalizationStringLen) != 1) {
                goto loser;
            }
            personalizationStringLen = personalizationStringLen / 8;
            if (personalizationStringLen > 0) {
                personalizationString = PORT_Alloc(personalizationStringLen);
            }
            fputs(buf, rngresp);

            continue;
        }

        if (strncmp(buf, "[AdditionalInputLen", 16) == 0) {
            if (additionalInput) {
                PORT_ZFree(additionalInput, additionalInputLen);
                additionalInput = NULL;
                additionalInputLen = 0;
            }

            if (sscanf(buf, "[AdditionalInputLen = %d]", &additionalInputLen) != 1) {
                goto loser;
            }
            additionalInputLen = additionalInputLen / 8;
            if (additionalInputLen > 0) {
                additionalInput = PORT_Alloc(additionalInputLen);
            }
            fputs(buf, rngresp);
            continue;
        }

        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            if (entropyInput) {
                memset(entropyInput, 0, entropyInputLen);
            }
            if (nonce) {
                memset(nonce, 0, nonceLen);
            }
            if (personalizationString) {
                memset(personalizationString, 0, personalizationStringLen);
            }
            if (additionalInput) {
                memset(additionalInput, 0, additionalInputLen);
            }
            genResult = PR_FALSE;

            fputs(buf, rngresp);
            continue;
        }

        /* EntropyInputReseed = ... */
        if (strncmp(buf, "EntropyInputReseed", 18) == 0) {
            if (entropyInput) {
                memset(entropyInput, 0, entropyInputLen);
                i = 18;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }

                for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<entropyInputLen*/
                    hex_to_byteval(&buf[i], &entropyInput[j]);
                }
            }
            fputs(buf, rngresp);
            continue;
        }

        /* AttionalInputReseed  = ... */
        if (strncmp(buf, "AdditionalInputReseed", 21) == 0) {
            if (additionalInput) {
                memset(additionalInput, 0, additionalInputLen);
                i = 21;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<additionalInputLen*/
                    hex_to_byteval(&buf[i], &additionalInput[j]);
                }
            }
            command = RESEED;
            fputs(buf, rngresp);
            continue;
        }

        /* Entropy input = ... */
        if (strncmp(buf, "EntropyInput", 12) == 0) {
            i = 12;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<entropyInputLen*/
                hex_to_byteval(&buf[i], &entropyInput[j]);
            }
            fputs(buf, rngresp);
            continue;
        }

        /* nouce = ... */
        if (strncmp(buf, "Nonce", 5) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<nonceLen*/
                hex_to_byteval(&buf[i], &nonce[j]);
            }
            fputs(buf, rngresp);
            continue;
        }

        /* Personalization string = ... */
        if (strncmp(buf, "PersonalizationString", 21) == 0) {
            if (personalizationString) {
                i = 21;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<personalizationStringLen*/
                    hex_to_byteval(&buf[i], &personalizationString[j]);
                }
            }
            fputs(buf, rngresp);
            command = INSTANTIATE;
            continue;
        }

        /* Additional input = ... */
        if (strncmp(buf, "AdditionalInput", 15) == 0) {
            if (additionalInput) {
                i = 15;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<additionalInputLen*/
                    hex_to_byteval(&buf[i], &additionalInput[j]);
                }
            }
            if (genResult) {
                command = RESULT;
            } else {
                command = GENERATE;
                genResult = PR_TRUE; /* next time generate result */
            }
            fputs(buf, rngresp);
            continue;
        }

        /* Returned bits = ... */
        if (strncmp(buf, "ReturnedBits", 12) == 0) {
            i = 12;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) { /*j<additionalInputLen*/
                hex_to_byteval(&buf[i], &predictedreturn_bytes[j]);
            }

            if (memcmp(return_bytes,
                       predictedreturn_bytes, return_bytes_len) != 0) {
                if (debug) {
                    fprintf(rngresp, "# Generate failed:\n");
                    fputs("#   predicted=", rngresp);
                    to_hex_str(buf, predictedreturn_bytes,
                               return_bytes_len);
                    fputs(buf, rngresp);
                    fputs("\n#   actual  = ", rngresp);
                    fputs(buf2, rngresp);
                    fputc('\n', rngresp);

                } else {
                    fprintf(stderr, "Generate failed:\n");
                    fputs("   predicted=", stderr);
                    to_hex_str(buf, predictedreturn_bytes,
                               return_bytes_len);
                    fputs(buf, stderr);
                    fputs("\n   actual  = ", stderr);
                    fputs(buf2, stderr);
                    fputc('\n', stderr);
                }
            }
            memset(predictedreturn_bytes, 0, return_bytes_len);

            continue;
        }
    }
loser:
    if (predictedreturn_bytes) {
        PORT_Free(predictedreturn_bytes);
    }
    if (return_bytes) {
        PORT_Free(return_bytes);
    }
    if (additionalInput) {
        PORT_Free(additionalInput);
    }
    if (personalizationString) {
        PORT_Free(personalizationString);
    }
    if (nonce) {
        PORT_Free(nonce);
    }
    if (entropyInput) {
        PORT_Free(entropyInput);
    }
    fclose(rngreq);
}

/*
 * Perform the RNG Variable Seed Test (VST) for the RNG algorithm
 * "DSA - Generation of X", used both as specified and as a generic
 * purpose RNG.  The presence of "Q = ..." in the REQUEST file
 * indicates we are using the algorithm as specified.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
rng_vst(char *reqfn)
{
    char buf[256]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "XSeed = <128 hex digits>\n".
                         */
    FILE *rngreq;  /* input stream from the REQUEST file */
    FILE *rngresp; /* output stream to the RESPONSE file */
    unsigned int i, j;
    unsigned char Q[DSA1_SUBPRIME_LEN];
    PRBool hasQ = PR_FALSE;
    unsigned int b = 0; /* 160 <= b <= 512, b is a multiple of 8 */
    unsigned char XKey[512 / 8];
    unsigned char XSeed[512 / 8];
    unsigned char GENX[DSA1_SIGNATURE_LEN];
    unsigned char DSAX[DSA1_SUBPRIME_LEN];
    SECStatus rv;

    rngreq = fopen(reqfn, "r");
    rngresp = stdout;
    while (fgets(buf, sizeof buf, rngreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, rngresp);
            continue;
        }
        /* [Xchange - SHA1] */
        if (buf[0] == '[') {
            fputs(buf, rngresp);
            continue;
        }
        /* Q = ... */
        if (buf[0] == 'Q') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof Q; i += 2, j++) {
                hex_to_byteval(&buf[i], &Q[j]);
            }
            fputs(buf, rngresp);
            hasQ = PR_TRUE;
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            b = 0;
            memset(XKey, 0, sizeof XKey);
            memset(XSeed, 0, sizeof XSeed);
            fputs(buf, rngresp);
            continue;
        }
        /* b = ... */
        if (buf[0] == 'b') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            b = atoi(&buf[i]);
            if (b < 160 || b > 512 || b % 8 != 0) {
                goto loser;
            }
            fputs(buf, rngresp);
            continue;
        }
        /* XKey = ... */
        if (strncmp(buf, "XKey", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < b / 8; i += 2, j++) {
                hex_to_byteval(&buf[i], &XKey[j]);
            }
            fputs(buf, rngresp);
            continue;
        }
        /* XSeed = ... */
        if (strncmp(buf, "XSeed", 5) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < b / 8; i += 2, j++) {
                hex_to_byteval(&buf[i], &XSeed[j]);
            }
            fputs(buf, rngresp);

            rv = FIPS186Change_GenerateX(XKey, XSeed, GENX);
            if (rv != SECSuccess) {
                goto loser;
            }
            fputs("X = ", rngresp);
            if (hasQ) {
                rv = FIPS186Change_ReduceModQForDSA(GENX, Q, DSAX);
                if (rv != SECSuccess) {
                    goto loser;
                }
                to_hex_str(buf, DSAX, sizeof DSAX);
            } else {
                to_hex_str(buf, GENX, sizeof GENX);
            }
            fputs(buf, rngresp);
            fputc('\n', rngresp);
            continue;
        }
    }
loser:
    fclose(rngreq);
}

/*
 * Perform the RNG Monte Carlo Test (MCT) for the RNG algorithm
 * "DSA - Generation of X", used both as specified and as a generic
 * purpose RNG.  The presence of "Q = ..." in the REQUEST file
 * indicates we are using the algorithm as specified.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
rng_mct(char *reqfn)
{
    char buf[256]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "XSeed = <128 hex digits>\n".
                         */
    FILE *rngreq;  /* input stream from the REQUEST file */
    FILE *rngresp; /* output stream to the RESPONSE file */
    unsigned int i, j;
    unsigned char Q[DSA1_SUBPRIME_LEN];
    PRBool hasQ = PR_FALSE;
    unsigned int b = 0; /* 160 <= b <= 512, b is a multiple of 8 */
    unsigned char XKey[512 / 8];
    unsigned char XSeed[512 / 8];
    unsigned char GENX[2 * SHA1_LENGTH];
    unsigned char DSAX[DSA1_SUBPRIME_LEN];
    SECStatus rv;

    rngreq = fopen(reqfn, "r");
    rngresp = stdout;
    while (fgets(buf, sizeof buf, rngreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, rngresp);
            continue;
        }
        /* [Xchange - SHA1] */
        if (buf[0] == '[') {
            fputs(buf, rngresp);
            continue;
        }
        /* Q = ... */
        if (buf[0] == 'Q') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof Q; i += 2, j++) {
                hex_to_byteval(&buf[i], &Q[j]);
            }
            fputs(buf, rngresp);
            hasQ = PR_TRUE;
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            b = 0;
            memset(XKey, 0, sizeof XKey);
            memset(XSeed, 0, sizeof XSeed);
            fputs(buf, rngresp);
            continue;
        }
        /* b = ... */
        if (buf[0] == 'b') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            b = atoi(&buf[i]);
            if (b < 160 || b > 512 || b % 8 != 0) {
                goto loser;
            }
            fputs(buf, rngresp);
            continue;
        }
        /* XKey = ... */
        if (strncmp(buf, "XKey", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < b / 8; i += 2, j++) {
                hex_to_byteval(&buf[i], &XKey[j]);
            }
            fputs(buf, rngresp);
            continue;
        }
        /* XSeed = ... */
        if (strncmp(buf, "XSeed", 5) == 0) {
            unsigned int k;
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < b / 8; i += 2, j++) {
                hex_to_byteval(&buf[i], &XSeed[j]);
            }
            fputs(buf, rngresp);

            for (k = 0; k < 10000; k++) {
                rv = FIPS186Change_GenerateX(XKey, XSeed, GENX);
                if (rv != SECSuccess) {
                    goto loser;
                }
            }
            fputs("X = ", rngresp);
            if (hasQ) {
                rv = FIPS186Change_ReduceModQForDSA(GENX, Q, DSAX);
                if (rv != SECSuccess) {
                    goto loser;
                }
                to_hex_str(buf, DSAX, sizeof DSAX);
            } else {
                to_hex_str(buf, GENX, sizeof GENX);
            }
            fputs(buf, rngresp);
            fputc('\n', rngresp);
            continue;
        }
    }
loser:
    fclose(rngreq);
}

/*
 * Calculate the SHA Message Digest
 *
 * MD = Message digest
 * MDLen = length of Message Digest and SHA_Type
 * msg = message to digest
 * msgLen = length of message to digest
 */
SECStatus
sha_calcMD(unsigned char *MD, unsigned int MDLen, unsigned char *msg, unsigned int msgLen)
{
    HASH_HashType hashType = sha_get_hashType(MDLen * PR_BITS_PER_BYTE);

    return fips_hashBuf(hashType, MD, msg, msgLen);
}

/*
 * Perform the SHA Monte Carlo Test
 *
 * MDLen = length of Message Digest and SHA_Type
 * seed = input seed value
 * resp = is the output response file.
 */
SECStatus
sha_mct_test(unsigned int MDLen, unsigned char *seed, FILE *resp)
{
    int i, j;
    unsigned int msgLen = MDLen * 3;
    unsigned char MD_i3[HASH_LENGTH_MAX]; /* MD[i-3] */
    unsigned char MD_i2[HASH_LENGTH_MAX]; /* MD[i-2] */
    unsigned char MD_i1[HASH_LENGTH_MAX]; /* MD[i-1] */
    unsigned char MD_i[HASH_LENGTH_MAX];  /* MD[i] */
    unsigned char msg[HASH_LENGTH_MAX * 3];
    char buf[HASH_LENGTH_MAX * 2 + 1]; /* MAX buf MD_i as a hex string */

    for (j = 0; j < 100; j++) {
        /* MD_0 = MD_1 = MD_2 = seed */
        memcpy(MD_i3, seed, MDLen);
        memcpy(MD_i2, seed, MDLen);
        memcpy(MD_i1, seed, MDLen);

        for (i = 3; i < 1003; i++) {
            /* Mi = MD[i-3] || MD [i-2] || MD [i-1] */
            memcpy(msg, MD_i3, MDLen);
            memcpy(&msg[MDLen], MD_i2, MDLen);
            memcpy(&msg[MDLen * 2], MD_i1, MDLen);

            /* MDi = SHA(Msg) */
            if (sha_calcMD(MD_i, MDLen,
                           msg, msgLen) != SECSuccess) {
                return SECFailure;
            }

            /* save MD[i-3] MD[i-2]  MD[i-1] */
            memcpy(MD_i3, MD_i2, MDLen);
            memcpy(MD_i2, MD_i1, MDLen);
            memcpy(MD_i1, MD_i, MDLen);
        }

        /* seed = MD_i */
        memcpy(seed, MD_i, MDLen);

        snprintf(buf, sizeof(buf), "COUNT = %d\n", j);
        fputs(buf, resp);

        /* output MD_i */
        fputs("MD = ", resp);
        to_hex_str(buf, MD_i, MDLen);
        fputs(buf, resp);
        fputc('\n', resp);
    }

    return SECSuccess;
}

/*
 * Perform the SHA Tests.
 *
 * reqfn is the pathname of the input REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
sha_test(char *reqfn)
{
    unsigned int i, j;
    unsigned int MDlen = 0;              /* the length of the Message Digest in Bytes  */
    unsigned int msgLen = 0;             /* the length of the input Message in Bytes */
    unsigned char *msg = NULL;           /* holds the message to digest.*/
    size_t bufSize = 256 * 128;          /*MAX buffer size */
    char *buf = NULL;                    /* holds one line from the input REQUEST file.*/
    unsigned char seed[HASH_LENGTH_MAX]; /* max size of seed 64 bytes */
    unsigned char MD[HASH_LENGTH_MAX];   /* message digest */

    FILE *req = NULL; /* input stream from the REQUEST file */
    FILE *resp;       /* output stream to the RESPONSE file */

    buf = PORT_ZAlloc(bufSize);
    if (buf == NULL) {
        goto loser;
    }

    /* zeroize the variables for the test with this data set */
    memset(seed, 0, sizeof seed);

    req = fopen(reqfn, "r");
    resp = stdout;
    while (fgets(buf, bufSize, req) != NULL) {

        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, resp);
            continue;
        }
        /* [L = Length of the Message Digest and sha_type */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "L ", 1) == 0) {
                i = 2;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                MDlen = atoi(&buf[i]);
                fputs(buf, resp);
                continue;
            }
        }
        /* Len = Length of the Input Message Length  ... */
        if (strncmp(buf, "Len", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            if (msg) {
                PORT_ZFree(msg, msgLen);
                msg = NULL;
            }
            msgLen = atoi(&buf[i]); /* in bits */
            if (msgLen % 8 != 0) {
                fprintf(stderr, "SHA tests are incorrectly configured for "
                                "BIT oriented implementations\n");
                goto loser;
            }
            msgLen = msgLen / 8; /* convert to bytes */
            fputs(buf, resp);
            msg = PORT_ZAlloc(msgLen);
            if (msg == NULL && msgLen != 0) {
                goto loser;
            }
            continue;
        }
        /* MSG = ... */
        if (strncmp(buf, "Msg", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < msgLen; i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            fputs(buf, resp);
            /* calculate the Message Digest */
            memset(MD, 0, sizeof MD);
            if (sha_calcMD(MD, MDlen,
                           msg, msgLen) != SECSuccess) {
                goto loser;
            }

            fputs("MD = ", resp);
            to_hex_str(buf, MD, MDlen);
            fputs(buf, resp);
            fputc('\n', resp);

            continue;
        }
        /* Seed = ... */
        if (strncmp(buf, "Seed", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < sizeof seed; i += 2, j++) {
                hex_to_byteval(&buf[i], &seed[j]);
            }

            fputs(buf, resp);
            fputc('\n', resp);

            /* do the Monte Carlo test */
            if (sha_mct_test(MDlen, seed, resp) != SECSuccess) {
                goto loser;
            }

            continue;
        }
    }
loser:
    if (req) {
        fclose(req);
    }
    if (buf) {
        PORT_ZFree(buf, bufSize);
    }
    if (msg) {
        PORT_ZFree(msg, msgLen);
    }
}

/****************************************************/
/* HMAC SHA-X calc                                  */
/* hmac_computed - the computed HMAC                */
/* hmac_length - the length of the computed HMAC    */
/* secret_key - secret key to HMAC                  */
/* secret_key_length - length of secret key,        */
/* message - message to HMAC                        */
/* message_length - length ofthe message            */
/****************************************************/
static SECStatus
hmac_calc(unsigned char *hmac_computed,
          const unsigned int hmac_length,
          const unsigned char *secret_key,
          const unsigned int secret_key_length,
          const unsigned char *message,
          const unsigned int message_length,
          const HASH_HashType hashAlg)
{
    SECStatus hmac_status = SECFailure;
    HMACContext *cx = NULL;
    SECHashObject *hashObj = NULL;
    unsigned int bytes_hashed = 0;

    hashObj = (SECHashObject *)HASH_GetRawHashObject(hashAlg);

    if (!hashObj)
        return (SECFailure);

    cx = HMAC_Create(hashObj, secret_key,
                     secret_key_length,
                     PR_TRUE); /* PR_TRUE for in FIPS mode */

    if (cx == NULL)
        return (SECFailure);

    HMAC_Begin(cx);
    HMAC_Update(cx, message, message_length);
    hmac_status = HMAC_Finish(cx, hmac_computed, &bytes_hashed,
                              hmac_length);

    HMAC_Destroy(cx, PR_TRUE);

    return (hmac_status);
}

/*
 * Perform the HMAC Tests.
 *
 * reqfn is the pathname of the input REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
hmac_test(char *reqfn)
{
    unsigned int i, j;
    size_t bufSize = 400;                        /* MAX buffer size */
    char *buf = NULL;                            /* holds one line from the input REQUEST file.*/
    unsigned int keyLen = 0;                     /* Key Length */
    unsigned char key[200];                      /* key MAX size = 184 */
    unsigned int msgLen = 128;                   /* the length of the input  */
                                                 /*  Message is always 128 Bytes */
    unsigned char *msg = NULL;                   /* holds the message to digest.*/
    unsigned int HMACLen = 0;                    /* the length of the HMAC Bytes  */
    unsigned int TLen = 0;                       /* the length of the requested */
                                                 /* truncated HMAC Bytes */
    unsigned char HMAC[HASH_LENGTH_MAX];         /* computed HMAC */
    unsigned char expectedHMAC[HASH_LENGTH_MAX]; /* for .fax files that have */
                                                 /* supplied known answer */
    HASH_HashType hash_alg = HASH_AlgNULL;       /* HMAC type */

    FILE *req = NULL; /* input stream from the REQUEST file */
    FILE *resp;       /* output stream to the RESPONSE file */

    buf = PORT_ZAlloc(bufSize);
    if (buf == NULL) {
        goto loser;
    }
    msg = PORT_ZAlloc(msgLen);
    if (msg == NULL) {
        goto loser;
    }

    req = fopen(reqfn, "r");
    resp = stdout;
    while (fgets(buf, bufSize, req) != NULL) {
        if (strncmp(buf, "Mac", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            memset(expectedHMAC, 0, HASH_LENGTH_MAX);
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &expectedHMAC[j]);
            }
            if (memcmp(HMAC, expectedHMAC, TLen) != 0) {
                fprintf(stderr, "Generate failed:\n");
                fputs("   expected=", stderr);
                to_hex_str(buf, expectedHMAC,
                           TLen);
                fputs(buf, stderr);
                fputs("\n   generated=", stderr);
                to_hex_str(buf, HMAC,
                           TLen);
                fputs(buf, stderr);
                fputc('\n', stderr);
            }
        }

        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, resp);
            continue;
        }
        /* [L = Length of the MAC and HASH_type */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "L ", 1) == 0) {
                i = 2;
                while (isspace(buf[i]) || buf[i] == '=') {
                    i++;
                }
                /* HMACLen will get reused for Tlen */
                HMACLen = atoi(&buf[i]);
                hash_alg = sha_get_hashType(HMACLen * PR_BITS_PER_BYTE);
                if (hash_alg == HASH_AlgNULL) {
                    goto loser;
                }
                fputs(buf, resp);
                continue;
            }
        }
        /* Count = test iteration number*/
        if (strncmp(buf, "Count ", 5) == 0) {
            /* count can just be put into resp file */
            fputs(buf, resp);
            /* zeroize the variables for the test with this data set */
            keyLen = 0;
            TLen = 0;
            memset(key, 0, sizeof key);
            memset(msg, 0, msgLen);
            memset(HMAC, 0, sizeof HMAC);
            continue;
        }
        /* KLen = Length of the Input Secret Key ... */
        if (strncmp(buf, "Klen", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            keyLen = atoi(&buf[i]); /* in bytes */
            fputs(buf, resp);
            continue;
        }
        /* key = the secret key for the key to MAC */
        if (strncmp(buf, "Key", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < keyLen; i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            fputs(buf, resp);
        }
        /* TLen = Length of the calculated HMAC */
        if (strncmp(buf, "Tlen", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            TLen = atoi(&buf[i]); /* in bytes */
            fputs(buf, resp);
            continue;
        }
        /* MSG = to HMAC always 128 bytes for these tests */
        if (strncmp(buf, "Msg", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < msgLen; i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            fputs(buf, resp);
            /* calculate the HMAC and output */
            if (hmac_calc(HMAC, HMACLen, key, keyLen,
                          msg, msgLen, hash_alg) != SECSuccess) {
                goto loser;
            }
            fputs("Mac = ", resp);
            to_hex_str(buf, HMAC, TLen);
            fputs(buf, resp);
            fputc('\n', resp);
            continue;
        }
    }
loser:
    if (req) {
        fclose(req);
    }
    if (buf) {
        PORT_ZFree(buf, bufSize);
    }
    if (msg) {
        PORT_ZFree(msg, msgLen);
    }
}

/*
 * Perform the DSA Key Pair Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
dsa_keypair_test(char *reqfn)
{
    char buf[800]; /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * 800 to hold (384 public key (x2 for HEX) + 1'\n'
                         */
    FILE *dsareq;  /* input stream from the REQUEST file */
    FILE *dsaresp; /* output stream to the RESPONSE file */
    int count;
    int N;
    int L;
    int i;
    PQGParams *pqg = NULL;
    PQGVerify *vfy = NULL;
    PRBool use_dsa1 = PR_FALSE;
    int keySizeIndex; /* index for valid key sizes */

    dsareq = fopen(reqfn, "r");
    dsaresp = stdout;
    while (fgets(buf, sizeof buf, dsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, dsaresp);
            continue;
        }

        /* [Mod = x] */
        if (buf[0] == '[') {
            if (pqg != NULL) {
                PQG_DestroyParams(pqg);
                pqg = NULL;
            }
            if (vfy != NULL) {
                PQG_DestroyVerify(vfy);
                vfy = NULL;
            }

            if (sscanf(buf, "[mod = L=%d, N=%d]", &L, &N) != 2) {
                use_dsa1 = PR_TRUE;
                if (sscanf(buf, "[mod = %d]", &L) != 1) {
                    goto loser;
                }
            }
            fputs(buf, dsaresp);
            fputc('\n', dsaresp);

            if (use_dsa1) {
                /*************************************************************
                 * PQG_ParamGenSeedLen doesn't take a key size, it takes an
                 * index that points to a valid key size.
                 */
                keySizeIndex = PQG_PBITS_TO_INDEX(L);
                if (keySizeIndex == -1 || L < 512 || L > 1024) {
                    fprintf(dsaresp,
                            "DSA key size must be a multiple of 64 between 512 "
                            "and 1024, inclusive");
                    goto loser;
                }

                /* Generate the parameters P, Q, and G */
                if (PQG_ParamGenSeedLen(keySizeIndex, PQG_TEST_SEED_BYTES,
                                        &pqg, &vfy) !=
                    SECSuccess) {
                    fprintf(dsaresp,
                            "ERROR: Unable to generate PQG parameters");
                    goto loser;
                }
            } else {
                if (PQG_ParamGenV2(L, N, N, &pqg, &vfy) != SECSuccess) {
                    fprintf(dsaresp,
                            "ERROR: Unable to generate PQG parameters");
                    goto loser;
                }
            }

            /* output P, Q, and G */
            to_hex_str(buf, pqg->prime.data, pqg->prime.len);
            fprintf(dsaresp, "P = %s\n", buf);
            to_hex_str(buf, pqg->subPrime.data, pqg->subPrime.len);
            fprintf(dsaresp, "Q = %s\n", buf);
            to_hex_str(buf, pqg->base.data, pqg->base.len);
            fprintf(dsaresp, "G = %s\n\n", buf);
            continue;
        }
        /* N = ...*/
        if (buf[0] == 'N') {

            if (sscanf(buf, "N = %d", &count) != 1) {
                goto loser;
            }
            /* Generate a DSA key, and output the key pair for N times */
            for (i = 0; i < count; i++) {
                DSAPrivateKey *dsakey = NULL;
                if (DSA_NewKey(pqg, &dsakey) != SECSuccess) {
                    fprintf(dsaresp, "ERROR: Unable to generate DSA key");
                    goto loser;
                }
                to_hex_str(buf, dsakey->privateValue.data,
                           dsakey->privateValue.len);
                fprintf(dsaresp, "X = %s\n", buf);
                to_hex_str(buf, dsakey->publicValue.data,
                           dsakey->publicValue.len);
                fprintf(dsaresp, "Y = %s\n\n", buf);
                PORT_FreeArena(dsakey->params.arena, PR_TRUE);
                dsakey = NULL;
            }
            continue;
        }
    }
loser:
    fclose(dsareq);
}

/*
 * pqg generation type
 */
typedef enum {
    FIPS186_1, /* Generate/Verify P,Q & G  according to FIPS 186-1 */
    A_1_2_1,   /* Generate Provable P & Q */
    A_1_1_3,   /* Verify Probable P & Q */
    A_1_2_2,   /* Verify Provable P & Q */
    A_2_1,     /* Generate Unverifiable G */
    A_2_2,     /* Assure Unverifiable G */
    A_2_3,     /* Generate Verifiable G */
    A_2_4      /* Verify Verifiable G */
} dsa_pqg_type;

/*
 * Perform the DSA Domain Parameter Validation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
dsa_pqgver_test(char *reqfn)
{
    char buf[800]; /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * 800 to hold (384 public key (x2 for HEX) + P = ...
                         */
    FILE *dsareq;  /* input stream from the REQUEST file */
    FILE *dsaresp; /* output stream to the RESPONSE file */
    int N;
    int L;
    unsigned int i, j;
    PQGParams pqg;
    PQGVerify vfy;
    unsigned int pghSize = 0; /* size for p, g, and h */
    dsa_pqg_type type = FIPS186_1;

    dsareq = fopen(reqfn, "r");
    dsaresp = stdout;
    memset(&pqg, 0, sizeof(pqg));
    memset(&vfy, 0, sizeof(vfy));

    while (fgets(buf, sizeof buf, dsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, dsaresp);
            continue;
        }

        /* [A.xxxxx ] */
        if (buf[0] == '[' && buf[1] == 'A') {

            if (strncmp(&buf[1], "A.1.1.3", 7) == 0) {
                type = A_1_1_3;
            } else if (strncmp(&buf[1], "A.2.2", 5) == 0) {
                type = A_2_2;
            } else if (strncmp(&buf[1], "A.2.4", 5) == 0) {
                type = A_2_4;
            } else if (strncmp(&buf[1], "A.1.2.2", 7) == 0) {
                type = A_1_2_2;
                /* validate our output from PQGGEN */
            } else if (strncmp(&buf[1], "A.1.1.2", 7) == 0) {
                type = A_2_4; /* validate PQ and G together */
            } else {
                fprintf(stderr, "Unknown dsa ver test %s\n", &buf[1]);
                exit(1);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* [Mod = x] */
        if (buf[0] == '[') {

            if (type == FIPS186_1) {
                N = 160;
                if (sscanf(buf, "[mod = %d]", &L) != 1) {
                    goto loser;
                }
            } else if (sscanf(buf, "[mod = L=%d, N=%d", &L, &N) != 2) {
                goto loser;
            }

            if (pqg.prime.data) { /* P */
                SECITEM_ZfreeItem(&pqg.prime, PR_FALSE);
            }
            if (pqg.subPrime.data) { /* Q */
                SECITEM_ZfreeItem(&pqg.subPrime, PR_FALSE);
            }
            if (pqg.base.data) { /* G */
                SECITEM_ZfreeItem(&pqg.base, PR_FALSE);
            }
            if (vfy.seed.data) { /* seed */
                SECITEM_ZfreeItem(&vfy.seed, PR_FALSE);
            }
            if (vfy.h.data) { /* H */
                SECITEM_ZfreeItem(&vfy.h, PR_FALSE);
            }

            fputs(buf, dsaresp);

            /*calculate the size of p, g, and h then allocate items  */
            pghSize = L / 8;

            pqg.base.data = vfy.h.data = NULL;
            vfy.seed.len = pqg.base.len = vfy.h.len = 0;
            SECITEM_AllocItem(NULL, &pqg.prime, pghSize);
            SECITEM_AllocItem(NULL, &vfy.seed, pghSize * 3);
            if (type == A_2_2) {
                SECITEM_AllocItem(NULL, &vfy.h, pghSize);
                vfy.h.len = pghSize;
            } else if (type == A_2_4) {
                SECITEM_AllocItem(NULL, &vfy.h, 1);
                vfy.h.len = 1;
            }
            pqg.prime.len = pghSize;
            /* q is always N bits */
            SECITEM_AllocItem(NULL, &pqg.subPrime, N / 8);
            pqg.subPrime.len = N / 8;
            vfy.counter = -1;

            continue;
        }
        /* P = ... */
        if (buf[0] == 'P') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.prime.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pqg.prime.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* Q = ... */
        if (buf[0] == 'Q') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.subPrime.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pqg.subPrime.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* G = ... */
        if (buf[0] == 'G') {
            i = 1;
            if (pqg.base.data) {
                SECITEM_ZfreeItem(&pqg.base, PR_FALSE);
            }
            SECITEM_AllocItem(NULL, &pqg.base, pghSize);
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pqg.base.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pqg.base.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* Seed = ...  or domain_parameter_seed = ... */
        if (strncmp(buf, "Seed", 4) == 0) {
            i = 4;
        } else if (strncmp(buf, "domain_parameter_seed", 21) == 0) {
            i = 21;
        } else if (strncmp(buf, "firstseed", 9) == 0) {
            i = 9;
        } else {
            i = 0;
        }
        if (i) {
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &vfy.seed.data[j]);
            }
            vfy.seed.len = j;

            fputs(buf, dsaresp);
            if (type == A_2_4) {
                SECStatus result;

                /* Verify the Parameters */
                SECStatus rv = PQG_VerifyParams(&pqg, &vfy, &result);
                if (rv != SECSuccess) {
                    goto loser;
                }
                if (result == SECSuccess) {
                    fprintf(dsaresp, "Result = P\n");
                } else {
                    fprintf(dsaresp, "Result = F\n");
                }
            }
            continue;
        }
        if ((strncmp(buf, "pseed", 5) == 0) ||
            (strncmp(buf, "qseed", 5) == 0)) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = vfy.seed.len; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &vfy.seed.data[j]);
            }
            vfy.seed.len = j;
            fputs(buf, dsaresp);

            continue;
        }
        if (strncmp(buf, "index", 4) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            hex_to_byteval(&buf[i], &vfy.h.data[0]);
            vfy.h.len = 1;
            fputs(buf, dsaresp);
        }

        /* c = ...  or counter=*/
        if (buf[0] == 'c') {
            if (strncmp(buf, "counter", 7) == 0) {
                if (sscanf(buf, "counter = %u", &vfy.counter) != 1) {
                    goto loser;
                }
            } else {
                if (sscanf(buf, "c = %u", &vfy.counter) != 1) {
                    goto loser;
                }
            }

            fputs(buf, dsaresp);
            if (type == A_1_1_3) {
                SECStatus result;
                /* only verify P and Q, we have everything now. do it */
                SECStatus rv = PQG_VerifyParams(&pqg, &vfy, &result);
                if (rv != SECSuccess) {
                    goto loser;
                }
                if (result == SECSuccess) {
                    fprintf(dsaresp, "Result = P\n");
                } else {
                    fprintf(dsaresp, "Result = F\n");
                }
                fprintf(dsaresp, "\n");
            }
            continue;
        }
        if (strncmp(buf, "pgen_counter", 12) == 0) {
            if (sscanf(buf, "pgen_counter = %u", &vfy.counter) != 1) {
                goto loser;
            }
            fputs(buf, dsaresp);
            continue;
        }
        if (strncmp(buf, "qgen_counter", 12) == 0) {
            fputs(buf, dsaresp);
            if (type == A_1_2_2) {
                SECStatus result;
                /* only verify P and Q, we have everything now. do it */
                SECStatus rv = PQG_VerifyParams(&pqg, &vfy, &result);
                if (rv != SECSuccess) {
                    goto loser;
                }
                if (result == SECSuccess) {
                    fprintf(dsaresp, "Result = P\n");
                } else {
                    fprintf(dsaresp, "Result = F\n");
                }
                fprintf(dsaresp, "\n");
            }
            continue;
        }
        /* H = ... */
        if (buf[0] == 'H') {
            SECStatus rv, result = SECFailure;

            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &vfy.h.data[j]);
            }
            vfy.h.len = j;
            fputs(buf, dsaresp);

            /* this should be a byte value. Remove the leading zeros. If
             * it doesn't reduce to a byte, PQG_VerifyParams will catch it
            if (type == A_2_2) {
                data_save = vfy.h.data;
                while(vfy.h.data[0] && (vfy.h.len > 1)) {
                        vfy.h.data++;
                        vfy.h.len--;
                }
            } */

            /* Verify the Parameters */
            rv = PQG_VerifyParams(&pqg, &vfy, &result);
            if (rv != SECSuccess) {
                goto loser;
            }
            if (result == SECSuccess) {
                fprintf(dsaresp, "Result = P\n");
            } else {
                fprintf(dsaresp, "Result = F\n");
            }
            fprintf(dsaresp, "\n");
            continue;
        }
    }
loser:
    fclose(dsareq);
    if (pqg.prime.data) { /* P */
        SECITEM_ZfreeItem(&pqg.prime, PR_FALSE);
    }
    if (pqg.subPrime.data) { /* Q */
        SECITEM_ZfreeItem(&pqg.subPrime, PR_FALSE);
    }
    if (pqg.base.data) { /* G */
        SECITEM_ZfreeItem(&pqg.base, PR_FALSE);
    }
    if (vfy.seed.data) { /* seed */
        SECITEM_ZfreeItem(&vfy.seed, PR_FALSE);
    }
    if (vfy.h.data) { /* H */
        SECITEM_ZfreeItem(&vfy.h, PR_FALSE);
    }
}

/*
 * Perform the DSA Public Key Validation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
dsa_pqggen_test(char *reqfn)
{
    char buf[800]; /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * 800 to hold seed = (384 public key (x2 for HEX)
                         */
    FILE *dsareq;  /* input stream from the REQUEST file */
    FILE *dsaresp; /* output stream to the RESPONSE file */
    int count;     /* number of times to generate parameters */
    int N;
    int L;
    int i;
    unsigned int j;
    int output_g = 1;
    PQGParams *pqg = NULL;
    PQGVerify *vfy = NULL;
    unsigned int keySizeIndex = 0;
    dsa_pqg_type type = FIPS186_1;

    dsareq = fopen(reqfn, "r");
    dsaresp = stdout;
    while (fgets(buf, sizeof buf, dsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, dsaresp);
            continue;
        }

        /* [A.xxxxx ] */
        if (buf[0] == '[' && buf[1] == 'A') {
            if (strncmp(&buf[1], "A.1.1.2", 7) == 0) {
                fprintf(stderr, "NSS does Generate Probablistic Primes\n");
                exit(1);
            } else if (strncmp(&buf[1], "A.2.1", 5) == 0) {
                type = A_1_2_1;
                output_g = 1;
                exit(1);
            } else if (strncmp(&buf[1], "A.2.3", 5) == 0) {
                fprintf(stderr, "NSS only Generates G with P&Q\n");
                exit(1);
            } else if (strncmp(&buf[1], "A.1.2.1", 7) == 0) {
                type = A_1_2_1;
                output_g = 0;
            } else {
                fprintf(stderr, "Unknown dsa pqggen test %s\n", &buf[1]);
                exit(1);
            }
            fputs(buf, dsaresp);
            continue;
        }

        /* [Mod = ... ] */
        if (buf[0] == '[') {

            if (type == FIPS186_1) {
                N = 160;
                if (sscanf(buf, "[mod = %d]", &L) != 1) {
                    goto loser;
                }
            } else if (sscanf(buf, "[mod = L=%d, N=%d", &L, &N) != 2) {
                goto loser;
            }

            fputs(buf, dsaresp);
            fputc('\n', dsaresp);

            if (type == FIPS186_1) {
                /************************************************************
                 * PQG_ParamGenSeedLen doesn't take a key size, it takes an
                 * index that points to a valid key size.
                 */
                keySizeIndex = PQG_PBITS_TO_INDEX(L);
                if (keySizeIndex == -1 || L < 512 || L > 1024) {
                    fprintf(dsaresp,
                            "DSA key size must be a multiple of 64 between 512 "
                            "and 1024, inclusive");
                    goto loser;
                }
            }
            continue;
        }
        /* N = ... */
        if (buf[0] == 'N') {
            if (strncmp(buf, "Num", 3) == 0) {
                if (sscanf(buf, "Num = %d", &count) != 1) {
                    goto loser;
                }
            } else if (sscanf(buf, "N = %d", &count) != 1) {
                goto loser;
            }
            for (i = 0; i < count; i++) {
                SECStatus rv;

                if (type == FIPS186_1) {
                    rv = PQG_ParamGenSeedLen(keySizeIndex, PQG_TEST_SEED_BYTES,
                                             &pqg, &vfy);
                } else {
                    rv = PQG_ParamGenV2(L, N, N, &pqg, &vfy);
                }
                if (rv != SECSuccess) {
                    fprintf(dsaresp,
                            "ERROR: Unable to generate PQG parameters");
                    goto loser;
                }
                to_hex_str(buf, pqg->prime.data, pqg->prime.len);
                fprintf(dsaresp, "P = %s\n", buf);
                to_hex_str(buf, pqg->subPrime.data, pqg->subPrime.len);
                fprintf(dsaresp, "Q = %s\n", buf);
                if (output_g) {
                    to_hex_str(buf, pqg->base.data, pqg->base.len);
                    fprintf(dsaresp, "G = %s\n", buf);
                }
                if (type == FIPS186_1) {
                    to_hex_str(buf, vfy->seed.data, vfy->seed.len);
                    fprintf(dsaresp, "Seed = %s\n", buf);
                    fprintf(dsaresp, "c = %d\n", vfy->counter);
                    to_hex_str(buf, vfy->h.data, vfy->h.len);
                    fputs("H = ", dsaresp);
                    for (j = vfy->h.len; j < pqg->prime.len; j++) {
                        fprintf(dsaresp, "00");
                    }
                    fprintf(dsaresp, "%s\n", buf);
                } else {
                    unsigned int seedlen = vfy->seed.len / 2;
                    unsigned int pgen_counter = vfy->counter >> 16;
                    unsigned int qgen_counter = vfy->counter & 0xffff;
                    /*fprintf(dsaresp, "index = %02x\n", vfy->h.data[0]); */
                    to_hex_str(buf, vfy->seed.data, seedlen);
                    fprintf(dsaresp, "pseed = %s\n", buf);
                    to_hex_str(buf, vfy->seed.data + seedlen, seedlen);
                    fprintf(dsaresp, "qseed = %s\n", buf);
                    fprintf(dsaresp, "pgen_counter = %d\n", pgen_counter);
                    fprintf(dsaresp, "qgen_counter = %d\n", qgen_counter);
                    if (output_g) {
                        to_hex_str(buf, vfy->seed.data, vfy->seed.len);
                        fprintf(dsaresp, "domain_parameter_seed = %s\n", buf);
                        fprintf(dsaresp, "index = %02x\n", vfy->h.data[0]);
                    }
                }
                fputc('\n', dsaresp);
                if (pqg != NULL) {
                    PQG_DestroyParams(pqg);
                    pqg = NULL;
                }
                if (vfy != NULL) {
                    PQG_DestroyVerify(vfy);
                    vfy = NULL;
                }
            }

            continue;
        }
    }
loser:
    fclose(dsareq);
    if (pqg != NULL) {
        PQG_DestroyParams(pqg);
    }
    if (vfy != NULL) {
        PQG_DestroyVerify(vfy);
    }
}

/*
 * Perform the DSA Signature Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
dsa_siggen_test(char *reqfn)
{
    char buf[800]; /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * max for Msg = ....
                         */
    FILE *dsareq;  /* input stream from the REQUEST file */
    FILE *dsaresp; /* output stream to the RESPONSE file */
    int modulus;
    int L;
    int N;
    int i, j;
    PRBool use_dsa1 = PR_FALSE;
    PQGParams *pqg = NULL;
    PQGVerify *vfy = NULL;
    DSAPrivateKey *dsakey = NULL;
    int keySizeIndex;                       /* index for valid key sizes */
    unsigned char hashBuf[HASH_LENGTH_MAX]; /* SHA-x hash (160-512 bits) */
    unsigned char sig[DSA_MAX_SIGNATURE_LEN];
    SECItem digest, signature;
    HASH_HashType hashType = HASH_AlgNULL;
    int hashNum = 0;

    dsareq = fopen(reqfn, "r");
    dsaresp = stdout;

    while (fgets(buf, sizeof buf, dsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, dsaresp);
            continue;
        }

        /* [Mod = x] */
        if (buf[0] == '[') {
            if (pqg != NULL) {
                PQG_DestroyParams(pqg);
                pqg = NULL;
            }
            if (vfy != NULL) {
                PQG_DestroyVerify(vfy);
                vfy = NULL;
            }
            if (dsakey != NULL) {
                PORT_FreeArena(dsakey->params.arena, PR_TRUE);
                dsakey = NULL;
            }

            if (sscanf(buf, "[mod = L=%d,  N=%d, SHA-%d]", &L, &N,
                       &hashNum) != 3) {
                use_dsa1 = PR_TRUE;
                hashNum = 1;
                if (sscanf(buf, "[mod = %d]", &modulus) != 1) {
                    goto loser;
                }
            }
            fputs(buf, dsaresp);
            fputc('\n', dsaresp);

            /****************************************************************
            * PQG_ParamGenSeedLen doesn't take a key size, it takes an index
            * that points to a valid key size.
            */
            if (use_dsa1) {
                keySizeIndex = PQG_PBITS_TO_INDEX(modulus);
                if (keySizeIndex == -1 || modulus < 512 || modulus > 1024) {
                    fprintf(dsaresp,
                            "DSA key size must be a multiple of 64 between 512 "
                            "and 1024, inclusive");
                    goto loser;
                }
                /* Generate PQG and output PQG */
                if (PQG_ParamGenSeedLen(keySizeIndex, PQG_TEST_SEED_BYTES,
                                        &pqg, &vfy) !=
                    SECSuccess) {
                    fprintf(dsaresp,
                            "ERROR: Unable to generate PQG parameters");
                    goto loser;
                }
            } else {
                if (PQG_ParamGenV2(L, N, N, &pqg, &vfy) != SECSuccess) {
                    fprintf(dsaresp,
                            "ERROR: Unable to generate PQG parameters");
                    goto loser;
                }
            }
            to_hex_str(buf, pqg->prime.data, pqg->prime.len);
            fprintf(dsaresp, "P = %s\n", buf);
            to_hex_str(buf, pqg->subPrime.data, pqg->subPrime.len);
            fprintf(dsaresp, "Q = %s\n", buf);
            to_hex_str(buf, pqg->base.data, pqg->base.len);
            fprintf(dsaresp, "G = %s\n", buf);

            /* create DSA Key */
            if (DSA_NewKey(pqg, &dsakey) != SECSuccess) {
                fprintf(dsaresp, "ERROR: Unable to generate DSA key");
                goto loser;
            }

            hashType = sha_get_hashType(hashNum);
            if (hashType == HASH_AlgNULL) {
                fprintf(dsaresp, "ERROR: invalid hash (SHA-%d)", hashNum);
                goto loser;
            }
            continue;
        }

        /* Msg = ... */
        if (strncmp(buf, "Msg", 3) == 0) {
            unsigned char msg[128]; /* MAX msg 128 */
            unsigned int len = 0;

            if (hashType == HASH_AlgNULL) {
                fprintf(dsaresp, "ERROR: Hash Alg not set");
                goto loser;
            }

            memset(hashBuf, 0, sizeof hashBuf);
            memset(sig, 0, sizeof sig);

            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            if (fips_hashBuf(hashType, hashBuf, msg, j) != SECSuccess) {
                fprintf(dsaresp, "ERROR: Unable to generate SHA% digest",
                        hashNum);
                goto loser;
            }

            digest.type = siBuffer;
            digest.data = hashBuf;
            digest.len = fips_hashLen(hashType);
            signature.type = siBuffer;
            signature.data = sig;
            signature.len = sizeof sig;

            if (DSA_SignDigest(dsakey, &signature, &digest) != SECSuccess) {
                fprintf(dsaresp, "ERROR: Unable to generate DSA signature");
                goto loser;
            }
            len = signature.len;
            if (len % 2 != 0) {
                goto loser;
            }
            len = len / 2;

            /* output the orginal Msg, and generated Y, R, and S */
            fputs(buf, dsaresp);
            to_hex_str(buf, dsakey->publicValue.data,
                       dsakey->publicValue.len);
            fprintf(dsaresp, "Y = %s\n", buf);
            to_hex_str(buf, &signature.data[0], len);
            fprintf(dsaresp, "R = %s\n", buf);
            to_hex_str(buf, &signature.data[len], len);
            fprintf(dsaresp, "S = %s\n", buf);
            fputc('\n', dsaresp);
            continue;
        }
    }
loser:
    fclose(dsareq);
    if (pqg != NULL) {
        PQG_DestroyParams(pqg);
        pqg = NULL;
    }
    if (vfy != NULL) {
        PQG_DestroyVerify(vfy);
        vfy = NULL;
    }
    if (dsakey) {
        PORT_FreeArena(dsakey->params.arena, PR_TRUE);
        dsakey = NULL;
    }
}

/*
 * Perform the DSA Signature Verification Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
dsa_sigver_test(char *reqfn)
{
    char buf[800]; /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * max for Msg = ....
                         */
    FILE *dsareq;  /* input stream from the REQUEST file */
    FILE *dsaresp; /* output stream to the RESPONSE file */
    int L;
    int N;
    unsigned int i, j;
    SECItem digest, signature;
    DSAPublicKey pubkey;
    unsigned int pgySize;                   /* size for p, g, and y */
    unsigned char hashBuf[HASH_LENGTH_MAX]; /* SHA-x hash (160-512 bits) */
    unsigned char sig[DSA_MAX_SIGNATURE_LEN];
    HASH_HashType hashType = HASH_AlgNULL;
    int hashNum = 0;

    dsareq = fopen(reqfn, "r");
    dsaresp = stdout;
    memset(&pubkey, 0, sizeof(pubkey));

    while (fgets(buf, sizeof buf, dsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, dsaresp);
            continue;
        }

        /* [Mod = x] */
        if (buf[0] == '[') {

            if (sscanf(buf, "[mod = L=%d,  N=%d, SHA-%d]", &L, &N,
                       &hashNum) != 3) {
                N = 160;
                hashNum = 1;
                if (sscanf(buf, "[mod = %d]", &L) != 1) {
                    goto loser;
                }
            }

            if (pubkey.params.prime.data) { /* P */
                SECITEM_ZfreeItem(&pubkey.params.prime, PR_FALSE);
            }
            if (pubkey.params.subPrime.data) { /* Q */
                SECITEM_ZfreeItem(&pubkey.params.subPrime, PR_FALSE);
            }
            if (pubkey.params.base.data) { /* G */
                SECITEM_ZfreeItem(&pubkey.params.base, PR_FALSE);
            }
            if (pubkey.publicValue.data) { /* Y */
                SECITEM_ZfreeItem(&pubkey.publicValue, PR_FALSE);
            }
            fputs(buf, dsaresp);

            /* calculate the size of p, g, and y then allocate items */
            pgySize = L / 8;
            SECITEM_AllocItem(NULL, &pubkey.params.prime, pgySize);
            SECITEM_AllocItem(NULL, &pubkey.params.base, pgySize);
            SECITEM_AllocItem(NULL, &pubkey.publicValue, pgySize);
            pubkey.params.prime.len = pubkey.params.base.len = pgySize;
            pubkey.publicValue.len = pgySize;

            /* q always N/8 bytes */
            SECITEM_AllocItem(NULL, &pubkey.params.subPrime, N / 8);
            pubkey.params.subPrime.len = N / 8;

            hashType = sha_get_hashType(hashNum);
            if (hashType == HASH_AlgNULL) {
                fprintf(dsaresp, "ERROR: invalid hash (SHA-%d)", hashNum);
                goto loser;
            }

            continue;
        }
        /* P = ... */
        if (buf[0] == 'P') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            memset(pubkey.params.prime.data, 0, pubkey.params.prime.len);
            for (j = 0; j < pubkey.params.prime.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pubkey.params.prime.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* Q = ... */
        if (buf[0] == 'Q') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            memset(pubkey.params.subPrime.data, 0, pubkey.params.subPrime.len);
            for (j = 0; j < pubkey.params.subPrime.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pubkey.params.subPrime.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* G = ... */
        if (buf[0] == 'G') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            memset(pubkey.params.base.data, 0, pubkey.params.base.len);
            for (j = 0; j < pubkey.params.base.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pubkey.params.base.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* Msg = ... */
        if (strncmp(buf, "Msg", 3) == 0) {
            unsigned char msg[128]; /* MAX msg 128 */
            memset(hashBuf, 0, sizeof hashBuf);

            if (hashType == HASH_AlgNULL) {
                fprintf(dsaresp, "ERROR: Hash Alg not set");
                goto loser;
            }

            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            if (fips_hashBuf(hashType, hashBuf, msg, j) != SECSuccess) {
                fprintf(dsaresp, "ERROR: Unable to generate SHA-%d digest",
                        hashNum);
                goto loser;
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* Y = ... */
        if (buf[0] == 'Y') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            memset(pubkey.publicValue.data, 0, pubkey.params.subPrime.len);
            for (j = 0; j < pubkey.publicValue.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pubkey.publicValue.data[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* R = ... */
        if (buf[0] == 'R') {
            memset(sig, 0, sizeof sig);
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pubkey.params.subPrime.len; i += 2, j++) {
                hex_to_byteval(&buf[i], &sig[j]);
            }

            fputs(buf, dsaresp);
            continue;
        }

        /* S = ... */
        if (buf[0] == 'S') {
            if (hashType == HASH_AlgNULL) {
                fprintf(dsaresp, "ERROR: Hash Alg not set");
                goto loser;
            }

            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = pubkey.params.subPrime.len;
                 j < pubkey.params.subPrime.len * 2; i += 2, j++) {
                hex_to_byteval(&buf[i], &sig[j]);
            }
            fputs(buf, dsaresp);

            digest.type = siBuffer;
            digest.data = hashBuf;
            digest.len = fips_hashLen(hashType);
            signature.type = siBuffer;
            signature.data = sig;
            signature.len = pubkey.params.subPrime.len * 2;

            if (DSA_VerifyDigest(&pubkey, &signature, &digest) == SECSuccess) {
                fprintf(dsaresp, "Result = P\n");
            } else {
                fprintf(dsaresp, "Result = F\n");
            }
            fprintf(dsaresp, "\n");
            continue;
        }
    }
loser:
    fclose(dsareq);
    if (pubkey.params.prime.data) { /* P */
        SECITEM_ZfreeItem(&pubkey.params.prime, PR_FALSE);
    }
    if (pubkey.params.subPrime.data) { /* Q */
        SECITEM_ZfreeItem(&pubkey.params.subPrime, PR_FALSE);
    }
    if (pubkey.params.base.data) { /* G */
        SECITEM_ZfreeItem(&pubkey.params.base, PR_FALSE);
    }
    if (pubkey.publicValue.data) { /* Y */
        SECITEM_ZfreeItem(&pubkey.publicValue, PR_FALSE);
    }
}

static void
pad(unsigned char *buf, int pad_len, unsigned char *src, int src_len)
{
    int offset = 0;
    /* this shouldn't happen, fail right away rather than produce bad output */
    if (pad_len < src_len) {
        fprintf(stderr, "data bigger than expected! %d > %d\n", src_len, pad_len);
        exit(1);
    }

    offset = pad_len - src_len;
    memset(buf, 0, offset);
    memcpy(buf + offset, src, src_len);
    return;
}

/*
 * Perform the DSA Key Pair Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
rsa_keypair_test(char *reqfn)
{
    char buf[800];           /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * 800 to hold (384 public key (x2 for HEX) + 1'\n'
                         */
    unsigned char buf2[400]; /* can't need more then 1/2 buf length */
    FILE *rsareq;            /* input stream from the REQUEST file */
    FILE *rsaresp;           /* output stream to the RESPONSE file */
    int count;
    int i;
    int keySize = 1; /* key size in bits*/
    int len = 0;     /* key size in bytes */
    int len2 = 0;    /* key size in bytes/2 (prime size) */
    SECItem e;
    unsigned char default_e[] = { 0x1, 0x0, 0x1 };

    e.data = default_e;
    e.len = sizeof(default_e);

    rsareq = fopen(reqfn, "r");
    rsaresp = stdout;
    while (fgets(buf, sizeof buf, rsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, rsaresp);
            continue;
        }

        /* [Mod = x] */
        if (buf[0] == '[') {
            if (buf[1] == 'm') {
                if (sscanf(buf, "[mod = %d]", &keySize) != 1) {
                    goto loser;
                }
                len = keySize / 8;
                len2 = keySize / 16;
            }
            fputs(buf, rsaresp);
            continue;
        }
        /* N = ...*/
        if (buf[0] == 'N') {

            if (sscanf(buf, "N = %d", &count) != 1) {
                goto loser;
            }

            /* Generate a DSA key, and output the key pair for N times */
            for (i = 0; i < count; i++) {
                RSAPrivateKey *rsakey = NULL;
                if ((rsakey = RSA_NewKey(keySize, &e)) == NULL) {
                    fprintf(rsaresp, "ERROR: Unable to generate RSA key");
                    goto loser;
                }
                pad(buf2, len, rsakey->publicExponent.data,
                    rsakey->publicExponent.len);
                to_hex_str(buf, buf2, len);
                fprintf(rsaresp, "e = %s\n", buf);
                pad(buf2, len2, rsakey->prime1.data,
                    rsakey->prime1.len);
                to_hex_str(buf, buf2, len2);
                fprintf(rsaresp, "p = %s\n", buf);
                pad(buf2, len2, rsakey->prime2.data,
                    rsakey->prime2.len);
                to_hex_str(buf, buf2, len2);
                fprintf(rsaresp, "q = %s\n", buf);
                pad(buf2, len, rsakey->modulus.data,
                    rsakey->modulus.len);
                to_hex_str(buf, buf2, len);
                fprintf(rsaresp, "n = %s\n", buf);
                pad(buf2, len, rsakey->privateExponent.data,
                    rsakey->privateExponent.len);
                to_hex_str(buf, buf2, len);
                fprintf(rsaresp, "d = %s\n", buf);
                fprintf(rsaresp, "\n");
                PORT_FreeArena(rsakey->arena, PR_TRUE);
                rsakey = NULL;
            }
            continue;
        }
    }
loser:
    fclose(rsareq);
}

/*
 * Perform the RSA Signature Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
rsa_siggen_test(char *reqfn)
{
    char buf[2 * RSA_MAX_TEST_MODULUS_BYTES + 1];
    /* buf holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * 2x for HEX output + 1 for \n
                         */
    FILE *rsareq;  /* input stream from the REQUEST file */
    FILE *rsaresp; /* output stream to the RESPONSE file */
    int i, j;
    unsigned char sha[HASH_LENGTH_MAX];  /* SHA digest */
    unsigned int shaLength = 0;          /* length of SHA */
    HASH_HashType shaAlg = HASH_AlgNULL; /* type of SHA Alg */
    SECOidTag shaOid = SEC_OID_UNKNOWN;
    int modulus; /* the Modulus size */
    int publicExponent = DEFAULT_RSA_PUBLIC_EXPONENT;
    SECItem pe = { 0, 0, 0 };
    unsigned char pubEx[4];
    int peCount = 0;

    RSAPrivateKey *rsaBlapiPrivKey = NULL;  /* holds RSA private and
                                              * public keys */
    RSAPublicKey *rsaBlapiPublicKey = NULL; /* hold RSA public key */

    rsareq = fopen(reqfn, "r");
    rsaresp = stdout;

    /* calculate the exponent */
    for (i = 0; i < 4; i++) {
        if (peCount || (publicExponent &
                        ((unsigned long)0xff000000L >> (i *
                                                        8)))) {
            pubEx[peCount] =
                (unsigned char)((publicExponent >> (3 - i) * 8) & 0xff);
            peCount++;
        }
    }
    pe.len = peCount;
    pe.data = &pubEx[0];
    pe.type = siBuffer;

    while (fgets(buf, sizeof buf, rsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, rsaresp);
            continue;
        }

        /* [mod = ...] */
        if (buf[0] == '[') {

            if (sscanf(buf, "[mod = %d]", &modulus) != 1) {
                goto loser;
            }
            if (modulus > RSA_MAX_TEST_MODULUS_BITS) {
                fprintf(rsaresp, "ERROR: modulus greater than test maximum\n");
                goto loser;
            }

            fputs(buf, rsaresp);

            if (rsaBlapiPrivKey != NULL) {
                PORT_FreeArena(rsaBlapiPrivKey->arena, PR_TRUE);
                rsaBlapiPrivKey = NULL;
                rsaBlapiPublicKey = NULL;
            }

            rsaBlapiPrivKey = RSA_NewKey(modulus, &pe);
            if (rsaBlapiPrivKey == NULL) {
                fprintf(rsaresp, "Error unable to create RSA key\n");
                goto loser;
            }

            to_hex_str(buf, rsaBlapiPrivKey->modulus.data,
                       rsaBlapiPrivKey->modulus.len);
            fprintf(rsaresp, "\nn = %s\n\n", buf);
            to_hex_str(buf, rsaBlapiPrivKey->publicExponent.data,
                       rsaBlapiPrivKey->publicExponent.len);
            fprintf(rsaresp, "e = %s\n", buf);
            /* convert private key to public key.  Memory
             * is freed with private key's arena  */
            rsaBlapiPublicKey = (RSAPublicKey *)PORT_ArenaAlloc(
                rsaBlapiPrivKey->arena,
                sizeof(RSAPublicKey));

            rsaBlapiPublicKey->modulus.len = rsaBlapiPrivKey->modulus.len;
            rsaBlapiPublicKey->modulus.data = rsaBlapiPrivKey->modulus.data;
            rsaBlapiPublicKey->publicExponent.len =
                rsaBlapiPrivKey->publicExponent.len;
            rsaBlapiPublicKey->publicExponent.data =
                rsaBlapiPrivKey->publicExponent.data;
            continue;
        }

        /* SHAAlg = ... */
        if (strncmp(buf, "SHAAlg", 6) == 0) {
            i = 6;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            /* set the SHA Algorithm */
            shaAlg = hash_string_to_hashType(&buf[i]);
            if (shaAlg == HASH_AlgNULL) {
                fprintf(rsaresp, "ERROR: Unable to find SHAAlg type");
                goto loser;
            }
            fputs(buf, rsaresp);
            continue;
        }
        /* Msg = ... */
        if (strncmp(buf, "Msg", 3) == 0) {

            unsigned char msg[128]; /* MAX msg 128 */
            unsigned int rsa_bytes_signed;
            unsigned char rsa_computed_signature[RSA_MAX_TEST_MODULUS_BYTES];
            SECStatus rv = SECFailure;
            NSSLOWKEYPublicKey *rsa_public_key;
            NSSLOWKEYPrivateKey *rsa_private_key;
            NSSLOWKEYPrivateKey low_RSA_private_key = { NULL,
                                                        NSSLOWKEYRSAKey };
            NSSLOWKEYPublicKey low_RSA_public_key = { NULL,
                                                      NSSLOWKEYRSAKey };

            low_RSA_private_key.u.rsa = *rsaBlapiPrivKey;
            low_RSA_public_key.u.rsa = *rsaBlapiPublicKey;

            rsa_private_key = &low_RSA_private_key;
            rsa_public_key = &low_RSA_public_key;

            memset(sha, 0, sizeof sha);
            memset(msg, 0, sizeof msg);
            rsa_bytes_signed = 0;
            memset(rsa_computed_signature, 0, sizeof rsa_computed_signature);

            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]) && j < sizeof(msg); i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }
            shaLength = fips_hashLen(shaAlg);
            if (fips_hashBuf(shaAlg, sha, msg, j) != SECSuccess) {
                if (shaLength == 0) {
                    fprintf(rsaresp, "ERROR: SHAAlg not defined.");
                }
                fprintf(rsaresp, "ERROR: Unable to generate SHA%x",
                        shaLength == 160 ? 1 : shaLength);
                goto loser;
            }
            shaOid = fips_hashOid(shaAlg);

            /* Perform RSA signature with the RSA private key. */
            rv = RSA_HashSign(shaOid,
                              rsa_private_key,
                              rsa_computed_signature,
                              &rsa_bytes_signed,
                              nsslowkey_PrivateModulusLen(rsa_private_key),
                              sha,
                              shaLength);

            if (rv != SECSuccess) {
                fprintf(rsaresp, "ERROR: RSA_HashSign failed");
                goto loser;
            }

            /* Output the signature */
            fputs(buf, rsaresp);
            to_hex_str(buf, rsa_computed_signature, rsa_bytes_signed);
            fprintf(rsaresp, "S = %s\n", buf);

            /* Perform RSA verification with the RSA public key. */
            rv = RSA_HashCheckSign(shaOid,
                                   rsa_public_key,
                                   rsa_computed_signature,
                                   rsa_bytes_signed,
                                   sha,
                                   shaLength);
            if (rv != SECSuccess) {
                fprintf(rsaresp, "ERROR: RSA_HashCheckSign failed");
                goto loser;
            }
            continue;
        }
    }
loser:
    fclose(rsareq);

    if (rsaBlapiPrivKey != NULL) {
        /* frees private and public key */
        PORT_FreeArena(rsaBlapiPrivKey->arena, PR_TRUE);
        rsaBlapiPrivKey = NULL;
        rsaBlapiPublicKey = NULL;
    }
}
/*
 * Perform the RSA Signature Verification Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
rsa_sigver_test(char *reqfn)
{
    char buf[2 * RSA_MAX_TEST_MODULUS_BYTES + 7];
    /* buf holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * s = 2x for HEX output + 1 for \n
                         */
    FILE *rsareq;  /* input stream from the REQUEST file */
    FILE *rsaresp; /* output stream to the RESPONSE file */
    int i, j;
    unsigned char sha[HASH_LENGTH_MAX]; /* SHA digest */
    unsigned int shaLength = 0;         /* actual length of the digest */
    HASH_HashType shaAlg = HASH_AlgNULL;
    SECOidTag shaOid = SEC_OID_UNKNOWN;
    int modulus = 0;                  /* the Modulus size */
    unsigned char signature[513];     /* largest signature size + '\n' */
    unsigned int signatureLength = 0; /* actual length of the signature */
    PRBool keyvalid = PR_TRUE;

    RSAPublicKey rsaBlapiPublicKey; /* hold RSA public key */

    rsareq = fopen(reqfn, "r");
    rsaresp = stdout;
    memset(&rsaBlapiPublicKey, 0, sizeof(RSAPublicKey));

    while (fgets(buf, sizeof buf, rsareq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, rsaresp);
            continue;
        }

        /* [Mod = ...] */
        if (buf[0] == '[') {
            unsigned int flen; /* length in bytes of the field size */

            if (rsaBlapiPublicKey.modulus.data) { /* n */
                SECITEM_ZfreeItem(&rsaBlapiPublicKey.modulus, PR_FALSE);
            }
            if (sscanf(buf, "[mod = %d]", &modulus) != 1) {
                goto loser;
            }

            if (modulus > RSA_MAX_TEST_MODULUS_BITS) {
                fprintf(rsaresp, "ERROR: modulus greater than test maximum\n");
                goto loser;
            }

            fputs(buf, rsaresp);

            signatureLength = flen = modulus / 8;

            SECITEM_AllocItem(NULL, &rsaBlapiPublicKey.modulus, flen);
            if (rsaBlapiPublicKey.modulus.data == NULL) {
                goto loser;
            }
            continue;
        }

        /* n = ... modulus */
        if (buf[0] == 'n') {
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            keyvalid = from_hex_str(&rsaBlapiPublicKey.modulus.data[0],
                                    rsaBlapiPublicKey.modulus.len,
                                    &buf[i]);

            if (!keyvalid) {
                fprintf(rsaresp, "ERROR: rsa_sigver n not valid.\n");
                goto loser;
            }
            fputs(buf, rsaresp);
            continue;
        }

        /* SHAAlg = ... */
        if (strncmp(buf, "SHAAlg", 6) == 0) {
            i = 6;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            /* set the SHA Algorithm */
            shaAlg = hash_string_to_hashType(&buf[i]);
            if (shaAlg == HASH_AlgNULL) {
                fprintf(rsaresp, "ERROR: Unable to find SHAAlg type");
                goto loser;
            }
            fputs(buf, rsaresp);
            continue;
        }

        /* e = ... public Key */
        if (buf[0] == 'e') {
            unsigned char data[RSA_MAX_TEST_EXPONENT_BYTES];
            unsigned char t;

            memset(data, 0, sizeof data);

            if (rsaBlapiPublicKey.publicExponent.data) { /* e */
                SECITEM_ZfreeItem(&rsaBlapiPublicKey.publicExponent, PR_FALSE);
            }

            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            /* skip leading zero's */
            while (isxdigit(buf[i])) {
                hex_to_byteval(&buf[i], &t);
                if (t == 0) {
                    i += 2;
                } else
                    break;
            }

            /* get the exponent */
            for (j = 0; isxdigit(buf[i]) && j < sizeof data; i += 2, j++) {
                hex_to_byteval(&buf[i], &data[j]);
            }

            if (j == 0) {
                j = 1;
            } /* to handle 1 byte length exponents */

            SECITEM_AllocItem(NULL, &rsaBlapiPublicKey.publicExponent, j);
            if (rsaBlapiPublicKey.publicExponent.data == NULL) {
                goto loser;
            }

            for (i = 0; i < j; i++) {
                rsaBlapiPublicKey.publicExponent.data[i] = data[i];
            }

            fputs(buf, rsaresp);
            continue;
        }

        /* Msg = ... */
        if (strncmp(buf, "Msg", 3) == 0) {
            unsigned char msg[128]; /* MAX msg 128 */

            memset(sha, 0, sizeof sha);
            memset(msg, 0, sizeof msg);

            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }

            for (j = 0; isxdigit(buf[i]) && j < sizeof msg; i += 2, j++) {
                hex_to_byteval(&buf[i], &msg[j]);
            }

            shaLength = fips_hashLen(shaAlg);
            if (fips_hashBuf(shaAlg, sha, msg, j) != SECSuccess) {
                if (shaLength == 0) {
                    fprintf(rsaresp, "ERROR: SHAAlg not defined.");
                }
                fprintf(rsaresp, "ERROR: Unable to generate SHA%x",
                        shaLength == 160 ? 1 : shaLength);
                goto loser;
            }

            fputs(buf, rsaresp);
            continue;
        }

        /* S = ... */
        if (buf[0] == 'S') {
            SECStatus rv = SECFailure;
            NSSLOWKEYPublicKey *rsa_public_key;
            NSSLOWKEYPublicKey low_RSA_public_key = { NULL,
                                                      NSSLOWKEYRSAKey };

            /* convert to a low RSA public key */
            low_RSA_public_key.u.rsa = rsaBlapiPublicKey;
            rsa_public_key = &low_RSA_public_key;

            memset(signature, 0, sizeof(signature));
            i = 1;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }

            for (j = 0; isxdigit(buf[i]) && j < sizeof signature; i += 2, j++) {
                hex_to_byteval(&buf[i], &signature[j]);
            }

            signatureLength = j;
            fputs(buf, rsaresp);

            shaOid = fips_hashOid(shaAlg);

            /* Perform RSA verification with the RSA public key. */
            rv = RSA_HashCheckSign(shaOid,
                                   rsa_public_key,
                                   signature,
                                   signatureLength,
                                   sha,
                                   shaLength);
            if (rv == SECSuccess) {
                fputs("Result = P\n", rsaresp);
            } else {
                fputs("Result = F\n", rsaresp);
            }
            continue;
        }
    }
loser:
    fclose(rsareq);
    if (rsaBlapiPublicKey.modulus.data) { /* n */
        SECITEM_ZfreeItem(&rsaBlapiPublicKey.modulus, PR_FALSE);
    }
    if (rsaBlapiPublicKey.publicExponent.data) { /* e */
        SECITEM_ZfreeItem(&rsaBlapiPublicKey.publicExponent, PR_FALSE);
    }
}

void
tls(char *reqfn)
{
    char buf[256]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "XSeed = <128 hex digits>\n".
                         */
    unsigned char *pms = NULL;
    int pms_len;
    unsigned char *master_secret = NULL;
    unsigned char *key_block = NULL;
    int key_block_len;
    unsigned char serverHello_random[SSL3_RANDOM_LENGTH];
    unsigned char clientHello_random[SSL3_RANDOM_LENGTH];
    unsigned char server_random[SSL3_RANDOM_LENGTH];
    unsigned char client_random[SSL3_RANDOM_LENGTH];
    FILE *tlsreq = NULL; /* input stream from the REQUEST file */
    FILE *tlsresp;       /* output stream to the RESPONSE file */
    unsigned int i, j;
    CK_SLOT_ID slotList[10];
    CK_SLOT_ID slotID;
    CK_ULONG slotListCount = sizeof(slotList) / sizeof(slotList[0]);
    CK_ULONG count;
    static const CK_C_INITIALIZE_ARGS pk11args = {
        NULL, NULL, NULL, NULL, CKF_LIBRARY_CANT_CREATE_OS_THREADS,
        (void *)"flags=readOnly,noCertDB,noModDB", NULL
    };
    static CK_OBJECT_CLASS ck_secret = CKO_SECRET_KEY;
    static CK_KEY_TYPE ck_generic = CKK_GENERIC_SECRET;
    static CK_BBOOL ck_true = CK_TRUE;
    static CK_ULONG one = 1;
    CK_ATTRIBUTE create_template[] = {
        { CKA_VALUE, NULL, 0 },
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
    };
    CK_ULONG create_template_count =
        sizeof(create_template) / sizeof(create_template[0]);
    CK_ATTRIBUTE derive_template[] = {
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
        { CKA_VALUE_LEN, &one, sizeof(one) },
    };
    CK_ULONG derive_template_count =
        sizeof(derive_template) / sizeof(derive_template[0]);
    CK_ATTRIBUTE master_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE kb1_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE kb2_template = { CKA_VALUE, NULL, 0 };

    CK_MECHANISM master_mech = { CKM_TLS_MASTER_KEY_DERIVE, NULL, 0 };
    CK_MECHANISM key_block_mech = { CKM_TLS_KEY_AND_MAC_DERIVE, NULL, 0 };
    CK_TLS12_MASTER_KEY_DERIVE_PARAMS master_params;
    CK_TLS12_KEY_MAT_PARAMS key_block_params;
    CK_SSL3_KEY_MAT_OUT key_material;
    CK_RV crv;

    /* set up PKCS #11 parameters */
    master_params.prfHashMechanism = CKM_SHA256;
    master_params.pVersion = NULL;
    master_params.RandomInfo.pClientRandom = clientHello_random;
    master_params.RandomInfo.ulClientRandomLen = sizeof(clientHello_random);
    master_params.RandomInfo.pServerRandom = serverHello_random;
    master_params.RandomInfo.ulServerRandomLen = sizeof(serverHello_random);
    master_mech.pParameter = (void *)&master_params;
    master_mech.ulParameterLen = sizeof(master_params);
    key_block_params.prfHashMechanism = CKM_SHA256;
    key_block_params.ulMacSizeInBits = 0;
    key_block_params.ulKeySizeInBits = 0;
    key_block_params.ulIVSizeInBits = 0;
    key_block_params.bIsExport = PR_FALSE; /* ignored anyway for TLS mech */
    key_block_params.RandomInfo.pClientRandom = client_random;
    key_block_params.RandomInfo.ulClientRandomLen = sizeof(client_random);
    key_block_params.RandomInfo.pServerRandom = server_random;
    key_block_params.RandomInfo.ulServerRandomLen = sizeof(server_random);
    key_block_params.pReturnedKeyMaterial = &key_material;
    key_block_mech.pParameter = (void *)&key_block_params;
    key_block_mech.ulParameterLen = sizeof(key_block_params);

    crv = NSC_Initialize((CK_VOID_PTR)&pk11args);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_Initialize failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    count = slotListCount;
    crv = NSC_GetSlotList(PR_TRUE, slotList, &count);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_GetSlotList failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    if ((count > slotListCount) || count < 1) {
        fprintf(stderr,
                "NSC_GetSlotList returned too many or too few slots: %d slots max=%d min=1\n",
                (int)count, (int)slotListCount);
        goto loser;
    }
    slotID = slotList[0];
    tlsreq = fopen(reqfn, "r");
    tlsresp = stdout;
    while (fgets(buf, sizeof buf, tlsreq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, tlsresp);
            continue;
        }
        /* [Xchange - SHA1] */
        if (buf[0] == '[') {
            if (strncmp(buf, "[TLS", 4) == 0) {
                if (buf[7] == '0') {
                    /* CK_SSL3_MASTER_KEY_DERIVE_PARAMS is a subset of
                     * CK_TLS12_MASTER_KEY_DERIVE_PARAMS and
                     * CK_SSL3_KEY_MAT_PARAMS is a subset of
                     * CK_TLS12_KEY_MAT_PARAMS. The latter params have
                     * an extra prfHashMechanism field at the end. */
                    master_mech.mechanism = CKM_TLS_MASTER_KEY_DERIVE;
                    key_block_mech.mechanism = CKM_TLS_KEY_AND_MAC_DERIVE;
                    master_mech.ulParameterLen = sizeof(CK_SSL3_MASTER_KEY_DERIVE_PARAMS);
                    key_block_mech.ulParameterLen = sizeof(CK_SSL3_KEY_MAT_PARAMS);
                } else if (buf[7] == '2') {
                    if (strncmp(&buf[10], "SHA-1", 5) == 0) {
                        master_params.prfHashMechanism = CKM_SHA_1;
                        key_block_params.prfHashMechanism = CKM_SHA_1;
                    } else if (strncmp(&buf[10], "SHA-224", 7) == 0) {
                        master_params.prfHashMechanism = CKM_SHA224;
                        key_block_params.prfHashMechanism = CKM_SHA224;
                    } else if (strncmp(&buf[10], "SHA-256", 7) == 0) {
                        master_params.prfHashMechanism = CKM_SHA256;
                        key_block_params.prfHashMechanism = CKM_SHA256;
                    } else if (strncmp(&buf[10], "SHA-384", 7) == 0) {
                        master_params.prfHashMechanism = CKM_SHA384;
                        key_block_params.prfHashMechanism = CKM_SHA384;
                    } else if (strncmp(&buf[10], "SHA-512", 7) == 0) {
                        master_params.prfHashMechanism = CKM_SHA512;
                        key_block_params.prfHashMechanism = CKM_SHA512;
                    } else {
                        fprintf(tlsresp, "ERROR: Unable to find prf Hash type");
                        goto loser;
                    }
                    master_mech.mechanism = CKM_TLS12_MASTER_KEY_DERIVE;
                    key_block_mech.mechanism = CKM_TLS12_KEY_AND_MAC_DERIVE;
                    master_mech.ulParameterLen = sizeof(master_params);
                    key_block_mech.ulParameterLen = sizeof(key_block_params);
                } else {
                    fprintf(stderr, "Unknown TLS type %x\n",
                            (unsigned int)buf[0]);
                    goto loser;
                }
            }
            if (strncmp(buf, "[pre-master", 11) == 0) {
                if (sscanf(buf, "[pre-master secret length = %d]",
                           &pms_len) != 1) {
                    goto loser;
                }
                pms_len = pms_len / 8;
                pms = malloc(pms_len);
                master_secret = malloc(pms_len);
                create_template[0].pValue = pms;
                create_template[0].ulValueLen = pms_len;
                master_template.pValue = master_secret;
                master_template.ulValueLen = pms_len;
            }
            if (strncmp(buf, "[key", 4) == 0) {
                if (sscanf(buf, "[key block length = %d]", &key_block_len) != 1) {
                    goto loser;
                }
                key_block_params.ulKeySizeInBits = 8;
                key_block_params.ulIVSizeInBits = key_block_len / 2 - 8;
                key_block_len = key_block_len / 8;
                key_block = malloc(key_block_len);
                kb1_template.pValue = &key_block[0];
                kb1_template.ulValueLen = 1;
                kb2_template.pValue = &key_block[1];
                kb2_template.ulValueLen = 1;
                key_material.pIVClient = &key_block[2];
                key_material.pIVServer = &key_block[2 + key_block_len / 2 - 1];
            }
            fputs(buf, tlsresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(pms, 0, pms_len);
            memset(master_secret, 0, pms_len);
            memset(key_block, 0, key_block_len);
            fputs(buf, tlsresp);
            continue;
        }
        /* pre_master_secret = ... */
        if (strncmp(buf, "pre_master_secret", 17) == 0) {
            i = 17;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < pms_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &pms[j]);
            }
            fputs(buf, tlsresp);
            continue;
        }
        /* serverHello_random = ... */
        if (strncmp(buf, "serverHello_random", 18) == 0) {
            i = 18;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < SSL3_RANDOM_LENGTH; i += 2, j++) {
                hex_to_byteval(&buf[i], &serverHello_random[j]);
            }
            fputs(buf, tlsresp);
            continue;
        }
        /* clientHello_random = ... */
        if (strncmp(buf, "clientHello_random", 18) == 0) {
            i = 18;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < SSL3_RANDOM_LENGTH; i += 2, j++) {
                hex_to_byteval(&buf[i], &clientHello_random[j]);
            }
            fputs(buf, tlsresp);
            continue;
        }
        /* server_random = ... */
        if (strncmp(buf, "server_random", 13) == 0) {
            i = 13;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < SSL3_RANDOM_LENGTH; i += 2, j++) {
                hex_to_byteval(&buf[i], &server_random[j]);
            }
            fputs(buf, tlsresp);
            continue;
        }
        /* client_random = ... */
        if (strncmp(buf, "client_random", 13) == 0) {
            CK_SESSION_HANDLE session;
            CK_OBJECT_HANDLE pms_handle;
            CK_OBJECT_HANDLE master_handle;
            CK_OBJECT_HANDLE fake_handle;
            i = 13;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < SSL3_RANDOM_LENGTH; i += 2, j++) {
                hex_to_byteval(&buf[i], &client_random[j]);
            }
            fputs(buf, tlsresp);
            crv = NSC_OpenSession(slotID, 0, NULL, NULL, &session);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_OpenSession failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_CreateObject(session, create_template,
                                   create_template_count, &pms_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_DeriveKey(session, &master_mech, pms_handle,
                                derive_template, derive_template_count - 1,
                                &master_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(master) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_GetAttributeValue(session, master_handle,
                                        &master_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("master_secret = ", tlsresp);
            to_hex_str(buf, master_secret, pms_len);
            fputs(buf, tlsresp);
            fputc('\n', tlsresp);
            crv = NSC_DeriveKey(session, &key_block_mech, master_handle,
                                derive_template, derive_template_count, &fake_handle);
            if (crv != CKR_OK) {
                fprintf(stderr,
                        "NSC_DeriveKey(keyblock) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_GetAttributeValue(session, key_material.hClientKey,
                                        &kb1_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_GetAttributeValue(session, key_material.hServerKey,
                                        &kb2_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("key_block = ", tlsresp);
            to_hex_str(buf, key_block, key_block_len);
            fputs(buf, tlsresp);
            fputc('\n', tlsresp);
            crv = NSC_CloseSession(session);
            continue;
        }
    }
loser:
    NSC_Finalize(NULL);
    if (pms)
        free(pms);
    if (master_secret)
        free(master_secret);
    if (key_block)
        free(key_block);
    if (tlsreq)
        fclose(tlsreq);
}

void
ikev1(char *reqfn)
{
    char buf[4096]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "g^xy = <2048 hex digits>\n".
                         */
    unsigned char *gxy = NULL;
    int gxy_len;
    unsigned char *Ni = NULL;
    int Ni_len;
    unsigned char *Nr = NULL;
    int Nr_len;
    unsigned char CKYi[8];
    int CKYi_len;
    unsigned char CKYr[8];
    int CKYr_len;
    unsigned int i, j;
    FILE *ikereq = NULL; /* input stream from the REQUEST file */
    FILE *ikeresp;       /* output stream to the RESPONSE file */

    CK_SLOT_ID slotList[10];
    CK_SLOT_ID slotID;
    CK_ULONG slotListCount = sizeof(slotList) / sizeof(slotList[0]);
    CK_ULONG count;
    static const CK_C_INITIALIZE_ARGS pk11args = {
        NULL, NULL, NULL, NULL, CKF_LIBRARY_CANT_CREATE_OS_THREADS,
        (void *)"flags=readOnly,noCertDB,noModDB", NULL
    };
    static CK_OBJECT_CLASS ck_secret = CKO_SECRET_KEY;
    static CK_KEY_TYPE ck_generic = CKK_GENERIC_SECRET;
    static CK_BBOOL ck_true = CK_TRUE;
    static CK_ULONG keyLen = 1;
    CK_ATTRIBUTE gxy_template[] = {
        { CKA_VALUE, NULL, 0 }, /* must be first */
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
    };
    CK_ULONG gxy_template_count =
        sizeof(gxy_template) / sizeof(gxy_template[0]);
    CK_ATTRIBUTE derive_template[] = {
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
        { CKA_VALUE_LEN, &keyLen, sizeof(keyLen) }, /* must be last */
    };
    CK_ULONG derive_template_count =
        sizeof(derive_template) / sizeof(derive_template[0]);
    CK_ATTRIBUTE skeyid_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE skeyid_d_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE skeyid_a_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE skeyid_e_template = { CKA_VALUE, NULL, 0 };
    unsigned char skeyid_secret[HASH_LENGTH_MAX];
    unsigned char skeyid_d_secret[HASH_LENGTH_MAX];
    unsigned char skeyid_a_secret[HASH_LENGTH_MAX];
    unsigned char skeyid_e_secret[HASH_LENGTH_MAX];

    CK_MECHANISM ike_mech = { CKM_NSS_IKE_PRF_DERIVE, NULL, 0 };
    CK_MECHANISM ike1_mech = { CKM_NSS_IKE1_PRF_DERIVE, NULL, 0 };
    CK_NSS_IKE_PRF_DERIVE_PARAMS ike_prf;
    CK_NSS_IKE1_PRF_DERIVE_PARAMS ike1_prf;
    CK_RV crv;

    /* set up PKCS #11 parameters */
    ike_prf.bDataAsKey = PR_TRUE;
    ike_prf.bRekey = PR_FALSE;
    ike_prf.hNewKey = CK_INVALID_HANDLE;
    CKYi_len = sizeof(CKYi);
    CKYr_len = sizeof(CKYr);
    ike1_prf.pCKYi = CKYi;
    ike1_prf.ulCKYiLen = CKYi_len;
    ike1_prf.pCKYr = CKYr;
    ike1_prf.ulCKYrLen = CKYr_len;
    ike_mech.pParameter = &ike_prf;
    ike_mech.ulParameterLen = sizeof(ike_prf);
    ike1_mech.pParameter = &ike1_prf;
    ike1_mech.ulParameterLen = sizeof(ike1_prf);
    skeyid_template.pValue = skeyid_secret;
    skeyid_template.ulValueLen = HASH_LENGTH_MAX;
    skeyid_d_template.pValue = skeyid_d_secret;
    skeyid_d_template.ulValueLen = HASH_LENGTH_MAX;
    skeyid_a_template.pValue = skeyid_a_secret;
    skeyid_a_template.ulValueLen = HASH_LENGTH_MAX;
    skeyid_e_template.pValue = skeyid_e_secret;
    skeyid_e_template.ulValueLen = HASH_LENGTH_MAX;

    crv = NSC_Initialize((CK_VOID_PTR)&pk11args);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_Initialize failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    count = slotListCount;
    crv = NSC_GetSlotList(PR_TRUE, slotList, &count);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_GetSlotList failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    if ((count > slotListCount) || count < 1) {
        fprintf(stderr,
                "NSC_GetSlotList returned too many or too few slots: %d slots max=%d min=1\n",
                (int)count, (int)slotListCount);
        goto loser;
    }
    slotID = slotList[0];
    ikereq = fopen(reqfn, "r");
    ikeresp = stdout;
    while (fgets(buf, sizeof buf, ikereq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ikeresp);
            continue;
        }
        /* [.....] */
        if (buf[0] == '[') {
            if (strncmp(buf, "[SHA-1]", 7) == 0) {
                ike_prf.prfMechanism = CKM_SHA_1_HMAC;
                ike1_prf.prfMechanism = CKM_SHA_1_HMAC;
            }
            if (strncmp(buf, "[SHA-224]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA224_HMAC;
                ike1_prf.prfMechanism = CKM_SHA224_HMAC;
            }
            if (strncmp(buf, "[SHA-256]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA256_HMAC;
                ike1_prf.prfMechanism = CKM_SHA256_HMAC;
            }
            if (strncmp(buf, "[SHA-384]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA384_HMAC;
                ike1_prf.prfMechanism = CKM_SHA384_HMAC;
            }
            if (strncmp(buf, "[SHA-512]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA512_HMAC;
                ike1_prf.prfMechanism = CKM_SHA512_HMAC;
            }
            if (strncmp(buf, "[AES-XCBC", 9) == 0) {
                ike_prf.prfMechanism = CKM_AES_XCBC_MAC;
                ike1_prf.prfMechanism = CKM_AES_XCBC_MAC;
            }
            if (strncmp(buf, "[g^xy", 5) == 0) {
                if (sscanf(buf, "[g^xy length = %d]",
                           &gxy_len) != 1) {
                    goto loser;
                }
                gxy_len = gxy_len / 8;
                if (gxy)
                    free(gxy);
                gxy = malloc(gxy_len);
                gxy_template[0].pValue = gxy;
                gxy_template[0].ulValueLen = gxy_len;
            }
            if (strncmp(buf, "[Ni", 3) == 0) {
                if (sscanf(buf, "[Ni length = %d]", &Ni_len) != 1) {
                    goto loser;
                }
                Ni_len = Ni_len / 8;
                if (Ni)
                    free(Ni);
                Ni = malloc(Ni_len);
                ike_prf.pNi = Ni;
                ike_prf.ulNiLen = Ni_len;
            }
            if (strncmp(buf, "[Nr", 3) == 0) {
                if (sscanf(buf, "[Nr length = %d]", &Nr_len) != 1) {
                    goto loser;
                }
                Nr_len = Nr_len / 8;
                if (Nr)
                    free(Nr);
                Nr = malloc(Nr_len);
                ike_prf.pNr = Nr;
                ike_prf.ulNrLen = Nr_len;
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(gxy, 0, gxy_len);
            memset(Ni, 0, Ni_len);
            memset(Nr, 0, Nr_len);
            memset(CKYi, 0, CKYi_len);
            memset(CKYr, 0, CKYr_len);
            fputs(buf, ikeresp);
            continue;
        }
        /* Ni = ... */
        if (strncmp(buf, "Ni", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < Ni_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &Ni[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* Nr = ... */
        if (strncmp(buf, "Nr", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < Nr_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &Nr[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* CKYi = ... */
        if (strncmp(buf, "CKY_I", 5) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < CKYi_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &CKYi[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* CKYr = ... */
        if (strncmp(buf, "CKY_R", 5) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < CKYr_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &CKYr[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* g^xy = ... */
        if (strncmp(buf, "g^xy", 4) == 0) {
            CK_SESSION_HANDLE session;
            CK_OBJECT_HANDLE gxy_handle;
            CK_OBJECT_HANDLE skeyid_handle;
            CK_OBJECT_HANDLE skeyid_d_handle;
            CK_OBJECT_HANDLE skeyid_a_handle;
            CK_OBJECT_HANDLE skeyid_e_handle;
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < gxy_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &gxy[j]);
            }
            fputs(buf, ikeresp);
            crv = NSC_OpenSession(slotID, 0, NULL, NULL, &session);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_OpenSession failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_CreateObject(session, gxy_template,
                                   gxy_template_count, &gxy_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            /* get the skeyid key */
            crv = NSC_DeriveKey(session, &ike_mech, gxy_handle,
                                derive_template, derive_template_count - 1,
                                &skeyid_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            skeyid_template.ulValueLen = HASH_LENGTH_MAX;
            crv = NSC_GetAttributeValue(session, skeyid_handle,
                                        &skeyid_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            /* use the length of the skeyid to set the target length of all the
             * other keys */
            keyLen = skeyid_template.ulValueLen;
            ike1_prf.hKeygxy = gxy_handle;
            ike1_prf.bHasPrevKey = PR_FALSE;
            ike1_prf.keyNumber = 0;
            crv = NSC_DeriveKey(session, &ike1_mech, skeyid_handle,
                                derive_template, derive_template_count,
                                &skeyid_d_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid_d) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }

            ike1_prf.hKeygxy = gxy_handle;
            ike1_prf.bHasPrevKey = CK_TRUE;
            ike1_prf.hPrevKey = skeyid_d_handle;
            ike1_prf.keyNumber = 1;
            crv = NSC_DeriveKey(session, &ike1_mech, skeyid_handle,
                                derive_template, derive_template_count,
                                &skeyid_a_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid_a) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            ike1_prf.hKeygxy = gxy_handle;
            ike1_prf.bHasPrevKey = CK_TRUE;
            ike1_prf.hPrevKey = skeyid_a_handle;
            ike1_prf.keyNumber = 2;
            crv = NSC_DeriveKey(session, &ike1_mech, skeyid_handle,
                                derive_template, derive_template_count,
                                &skeyid_e_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid_e) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID = ", ikeresp);
            to_hex_str(buf, skeyid_secret, keyLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            skeyid_d_template.ulValueLen = keyLen;
            crv = NSC_GetAttributeValue(session, skeyid_d_handle,
                                        &skeyid_d_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid_d) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID_d = ", ikeresp);
            to_hex_str(buf, skeyid_d_secret, skeyid_d_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            skeyid_a_template.ulValueLen = keyLen;
            crv = NSC_GetAttributeValue(session, skeyid_a_handle,
                                        &skeyid_a_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid_a) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID_a = ", ikeresp);
            to_hex_str(buf, skeyid_a_secret, skeyid_a_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            skeyid_e_template.ulValueLen = keyLen;
            crv = NSC_GetAttributeValue(session, skeyid_e_handle,
                                        &skeyid_e_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid_e) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID_e = ", ikeresp);
            to_hex_str(buf, skeyid_e_secret, skeyid_e_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            crv = NSC_CloseSession(session);
            continue;
        }
    }
loser:
    NSC_Finalize(NULL);
    if (gxy)
        free(gxy);
    if (Ni)
        free(Ni);
    if (Nr)
        free(Nr);
    if (ikereq)
        fclose(ikereq);
}

void
ikev1_psk(char *reqfn)
{
    char buf[4096]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "g^xy = <2048 hex digits>\n".
                         */
    unsigned char *gxy = NULL;
    int gxy_len;
    unsigned char *Ni = NULL;
    int Ni_len;
    unsigned char *Nr = NULL;
    int Nr_len;
    unsigned char CKYi[8];
    int CKYi_len;
    unsigned char CKYr[8];
    int CKYr_len;
    unsigned char *psk = NULL;
    int psk_len;
    unsigned int i, j;
    FILE *ikereq = NULL; /* input stream from the REQUEST file */
    FILE *ikeresp;       /* output stream to the RESPONSE file */

    CK_SLOT_ID slotList[10];
    CK_SLOT_ID slotID;
    CK_ULONG slotListCount = sizeof(slotList) / sizeof(slotList[0]);
    CK_ULONG count;
    static const CK_C_INITIALIZE_ARGS pk11args = {
        NULL, NULL, NULL, NULL, CKF_LIBRARY_CANT_CREATE_OS_THREADS,
        (void *)"flags=readOnly,noCertDB,noModDB", NULL
    };
    static CK_OBJECT_CLASS ck_secret = CKO_SECRET_KEY;
    static CK_KEY_TYPE ck_generic = CKK_GENERIC_SECRET;
    static CK_BBOOL ck_true = CK_TRUE;
    static CK_ULONG keyLen = 1;
    CK_ATTRIBUTE gxy_template[] = {
        { CKA_VALUE, NULL, 0 }, /* must be first */
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
    };
    CK_ULONG gxy_template_count =
        sizeof(gxy_template) / sizeof(gxy_template[0]);
    CK_ATTRIBUTE psk_template[] = {
        { CKA_VALUE, NULL, 0 }, /* must be first */
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
    };
    CK_ULONG psk_template_count =
        sizeof(psk_template) / sizeof(psk_template[0]);
    CK_ATTRIBUTE derive_template[] = {
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
        { CKA_VALUE_LEN, &keyLen, sizeof(keyLen) }, /* must be last */
    };
    CK_ULONG derive_template_count =
        sizeof(derive_template) / sizeof(derive_template[0]);
    CK_ATTRIBUTE skeyid_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE skeyid_d_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE skeyid_a_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE skeyid_e_template = { CKA_VALUE, NULL, 0 };
    unsigned char skeyid_secret[HASH_LENGTH_MAX];
    unsigned char skeyid_d_secret[HASH_LENGTH_MAX];
    unsigned char skeyid_a_secret[HASH_LENGTH_MAX];
    unsigned char skeyid_e_secret[HASH_LENGTH_MAX];

    CK_MECHANISM ike_mech = { CKM_NSS_IKE_PRF_DERIVE, NULL, 0 };
    CK_MECHANISM ike1_mech = { CKM_NSS_IKE1_PRF_DERIVE, NULL, 0 };
    CK_NSS_IKE_PRF_DERIVE_PARAMS ike_prf;
    CK_NSS_IKE1_PRF_DERIVE_PARAMS ike1_prf;
    CK_RV crv;

    /* set up PKCS #11 parameters */
    ike_prf.bDataAsKey = PR_FALSE;
    ike_prf.bRekey = PR_FALSE;
    ike_prf.hNewKey = CK_INVALID_HANDLE;
    CKYi_len = 8;
    CKYr_len = 8;
    ike1_prf.pCKYi = CKYi;
    ike1_prf.ulCKYiLen = CKYi_len;
    ike1_prf.pCKYr = CKYr;
    ike1_prf.ulCKYrLen = CKYr_len;
    ike_mech.pParameter = &ike_prf;
    ike_mech.ulParameterLen = sizeof(ike_prf);
    ike1_mech.pParameter = &ike1_prf;
    ike1_mech.ulParameterLen = sizeof(ike1_prf);
    skeyid_template.pValue = skeyid_secret;
    skeyid_template.ulValueLen = HASH_LENGTH_MAX;
    skeyid_d_template.pValue = skeyid_d_secret;
    skeyid_d_template.ulValueLen = HASH_LENGTH_MAX;
    skeyid_a_template.pValue = skeyid_a_secret;
    skeyid_a_template.ulValueLen = HASH_LENGTH_MAX;
    skeyid_e_template.pValue = skeyid_e_secret;
    skeyid_e_template.ulValueLen = HASH_LENGTH_MAX;

    crv = NSC_Initialize((CK_VOID_PTR)&pk11args);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_Initialize failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    count = slotListCount;
    crv = NSC_GetSlotList(PR_TRUE, slotList, &count);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_GetSlotList failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    if ((count > slotListCount) || count < 1) {
        fprintf(stderr,
                "NSC_GetSlotList returned too many or too few slots: %d slots max=%d min=1\n",
                (int)count, (int)slotListCount);
        goto loser;
    }
    slotID = slotList[0];
    ikereq = fopen(reqfn, "r");
    ikeresp = stdout;
    while (fgets(buf, sizeof buf, ikereq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ikeresp);
            continue;
        }
        /* [.....] */
        if (buf[0] == '[') {
            if (strncmp(buf, "[SHA-1]", 7) == 0) {
                ike_prf.prfMechanism = CKM_SHA_1_HMAC;
                ike1_prf.prfMechanism = CKM_SHA_1_HMAC;
            }
            if (strncmp(buf, "[SHA-224]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA224_HMAC;
                ike1_prf.prfMechanism = CKM_SHA224_HMAC;
            }
            if (strncmp(buf, "[SHA-256]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA256_HMAC;
                ike1_prf.prfMechanism = CKM_SHA256_HMAC;
            }
            if (strncmp(buf, "[SHA-384]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA384_HMAC;
                ike1_prf.prfMechanism = CKM_SHA384_HMAC;
            }
            if (strncmp(buf, "[SHA-512]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA512_HMAC;
                ike1_prf.prfMechanism = CKM_SHA512_HMAC;
            }
            if (strncmp(buf, "[AES-XCBC", 9) == 0) {
                ike_prf.prfMechanism = CKM_AES_XCBC_MAC;
                ike1_prf.prfMechanism = CKM_AES_XCBC_MAC;
            }
            if (strncmp(buf, "[g^xy", 5) == 0) {
                if (sscanf(buf, "[g^xy length = %d]",
                           &gxy_len) != 1) {
                    goto loser;
                }
                gxy_len = gxy_len / 8;
                if (gxy)
                    free(gxy);
                gxy = malloc(gxy_len);
                gxy_template[0].pValue = gxy;
                gxy_template[0].ulValueLen = gxy_len;
            }
            if (strncmp(buf, "[pre-shared-key", 15) == 0) {
                if (sscanf(buf, "[pre-shared-key length = %d]",
                           &psk_len) != 1) {
                    goto loser;
                }
                psk_len = psk_len / 8;
                if (psk)
                    free(psk);
                psk = malloc(psk_len);
                psk_template[0].pValue = psk;
                psk_template[0].ulValueLen = psk_len;
            }
            if (strncmp(buf, "[Ni", 3) == 0) {
                if (sscanf(buf, "[Ni length = %d]", &Ni_len) != 1) {
                    goto loser;
                }
                Ni_len = Ni_len / 8;
                if (Ni)
                    free(Ni);
                Ni = malloc(Ni_len);
                ike_prf.pNi = Ni;
                ike_prf.ulNiLen = Ni_len;
            }
            if (strncmp(buf, "[Nr", 3) == 0) {
                if (sscanf(buf, "[Nr length = %d]", &Nr_len) != 1) {
                    goto loser;
                }
                Nr_len = Nr_len / 8;
                if (Nr)
                    free(Nr);
                Nr = malloc(Nr_len);
                ike_prf.pNr = Nr;
                ike_prf.ulNrLen = Nr_len;
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(gxy, 0, gxy_len);
            memset(Ni, 0, Ni_len);
            memset(Nr, 0, Nr_len);
            memset(CKYi, 0, CKYi_len);
            memset(CKYr, 0, CKYr_len);
            fputs(buf, ikeresp);
            continue;
        }
        /* Ni = ... */
        if (strncmp(buf, "Ni", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < Ni_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &Ni[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* Nr = ... */
        if (strncmp(buf, "Nr", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < Nr_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &Nr[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* CKYi = ... */
        if (strncmp(buf, "CKY_I", 5) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < CKYi_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &CKYi[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* CKYr = ... */
        if (strncmp(buf, "CKY_R", 5) == 0) {
            i = 5;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < CKYr_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &CKYr[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* g^xy = ... */
        if (strncmp(buf, "g^xy", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < gxy_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &gxy[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* pre-shared-key = ... */
        if (strncmp(buf, "pre-shared-key", 14) == 0) {
            CK_SESSION_HANDLE session;
            CK_OBJECT_HANDLE gxy_handle;
            CK_OBJECT_HANDLE psk_handle;
            CK_OBJECT_HANDLE skeyid_handle;
            CK_OBJECT_HANDLE skeyid_d_handle;
            CK_OBJECT_HANDLE skeyid_a_handle;
            CK_OBJECT_HANDLE skeyid_e_handle;
            i = 14;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < psk_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &psk[j]);
            }
            fputs(buf, ikeresp);
            crv = NSC_OpenSession(slotID, 0, NULL, NULL, &session);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_OpenSession failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_CreateObject(session, psk_template,
                                   psk_template_count, &psk_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject(psk) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_CreateObject(session, gxy_template,
                                   gxy_template_count, &gxy_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject(gxy) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            /* get the skeyid key */
            crv = NSC_DeriveKey(session, &ike_mech, psk_handle,
                                derive_template, derive_template_count - 1,
                                &skeyid_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            skeyid_template.ulValueLen = HASH_LENGTH_MAX;
            crv = NSC_GetAttributeValue(session, skeyid_handle,
                                        &skeyid_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            /* use the length of the skeyid to set the target length of all the
             * other keys */
            keyLen = skeyid_template.ulValueLen;
            ike1_prf.hKeygxy = gxy_handle;
            ike1_prf.bHasPrevKey = PR_FALSE;
            ike1_prf.keyNumber = 0;
            crv = NSC_DeriveKey(session, &ike1_mech, skeyid_handle,
                                derive_template, derive_template_count,
                                &skeyid_d_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid_d) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }

            ike1_prf.hKeygxy = gxy_handle;
            ike1_prf.bHasPrevKey = CK_TRUE;
            ike1_prf.hPrevKey = skeyid_d_handle;
            ike1_prf.keyNumber = 1;
            crv = NSC_DeriveKey(session, &ike1_mech, skeyid_handle,
                                derive_template, derive_template_count,
                                &skeyid_a_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid_a) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            ike1_prf.hKeygxy = gxy_handle;
            ike1_prf.bHasPrevKey = CK_TRUE;
            ike1_prf.hPrevKey = skeyid_a_handle;
            ike1_prf.keyNumber = 2;
            crv = NSC_DeriveKey(session, &ike1_mech, skeyid_handle,
                                derive_template, derive_template_count,
                                &skeyid_e_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid_e) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID = ", ikeresp);
            to_hex_str(buf, skeyid_secret, keyLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            skeyid_d_template.ulValueLen = keyLen;
            crv = NSC_GetAttributeValue(session, skeyid_d_handle,
                                        &skeyid_d_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid_d) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID_d = ", ikeresp);
            to_hex_str(buf, skeyid_d_secret, skeyid_d_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            skeyid_a_template.ulValueLen = keyLen;
            crv = NSC_GetAttributeValue(session, skeyid_a_handle,
                                        &skeyid_a_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid_a) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID_a = ", ikeresp);
            to_hex_str(buf, skeyid_a_secret, skeyid_a_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            skeyid_e_template.ulValueLen = keyLen;
            crv = NSC_GetAttributeValue(session, skeyid_e_handle,
                                        &skeyid_e_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid_e) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYID_e = ", ikeresp);
            to_hex_str(buf, skeyid_e_secret, skeyid_e_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            crv = NSC_CloseSession(session);
            continue;
        }
    }
loser:
    NSC_Finalize(NULL);
    if (psk)
        free(psk);
    if (gxy)
        free(gxy);
    if (Ni)
        free(Ni);
    if (Nr)
        free(Nr);
    if (ikereq)
        fclose(ikereq);
}

void
ikev2(char *reqfn)
{
    char buf[4096]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "g^xy = <2048 hex digits>\n".
                         */
    unsigned char *gir = NULL;
    unsigned char *gir_new = NULL;
    int gir_len;
    unsigned char *Ni = NULL;
    int Ni_len;
    unsigned char *Nr = NULL;
    int Nr_len;
    unsigned char *SPIi = NULL;
    int SPIi_len = 8;
    unsigned char *SPIr = NULL;
    int SPIr_len = 8;
    unsigned char *DKM = NULL;
    int DKM_len;
    unsigned char *DKM_child = NULL;
    int DKM_child_len;
    unsigned char *seed_data = NULL;
    int seed_data_len = 0;
    unsigned int i, j;
    FILE *ikereq = NULL; /* input stream from the REQUEST file */
    FILE *ikeresp;       /* output stream to the RESPONSE file */

    CK_SLOT_ID slotList[10];
    CK_SLOT_ID slotID;
    CK_ULONG slotListCount = sizeof(slotList) / sizeof(slotList[0]);
    CK_ULONG count;
    static const CK_C_INITIALIZE_ARGS pk11args = {
        NULL, NULL, NULL, NULL, CKF_LIBRARY_CANT_CREATE_OS_THREADS,
        (void *)"flags=readOnly,noCertDB,noModDB", NULL
    };
    static CK_OBJECT_CLASS ck_secret = CKO_SECRET_KEY;
    static CK_KEY_TYPE ck_generic = CKK_GENERIC_SECRET;
    static CK_BBOOL ck_true = CK_TRUE;
    static CK_ULONG keyLen = 1;
    CK_ATTRIBUTE gir_template[] = {
        { CKA_VALUE, NULL, 0 },
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
    };
    CK_ULONG gir_template_count =
        sizeof(gir_template) / sizeof(gir_template[0]);
    CK_ATTRIBUTE gir_new_template[] = {
        { CKA_VALUE, NULL, 0 },
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
    };
    CK_ULONG gir_new_template_count =
        sizeof(gir_new_template) / sizeof(gir_new_template[0]);
    CK_ATTRIBUTE derive_template[] = {
        { CKA_CLASS, &ck_secret, sizeof(ck_secret) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
        { CKA_VALUE_LEN, &keyLen, sizeof(keyLen) },
    };
    CK_ULONG derive_template_count =
        sizeof(derive_template) / sizeof(derive_template[0]);
    CK_ATTRIBUTE skeyseed_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE dkm_template = { CKA_VALUE, NULL, 0 };
    CK_ATTRIBUTE dkm_child_template = { CKA_VALUE, NULL, 0 };
    unsigned char skeyseed_secret[HASH_LENGTH_MAX];

    CK_MECHANISM ike_mech = { CKM_NSS_IKE_PRF_DERIVE, NULL, 0 };
    CK_MECHANISM ike2_mech = { CKM_NSS_IKE_PRF_PLUS_DERIVE, NULL, 0 };
    CK_MECHANISM subset_mech = { CKM_EXTRACT_KEY_FROM_KEY, NULL, 0 };
    CK_NSS_IKE_PRF_DERIVE_PARAMS ike_prf;
    CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS ike2_prf;
    CK_EXTRACT_PARAMS subset_params;
    CK_RV crv;

    /* set up PKCS #11 parameters */
    ike_mech.pParameter = &ike_prf;
    ike_mech.ulParameterLen = sizeof(ike_prf);
    ike2_mech.pParameter = &ike2_prf;
    ike2_mech.ulParameterLen = sizeof(ike2_prf);
    subset_mech.pParameter = &subset_params;
    subset_mech.ulParameterLen = sizeof(subset_params);
    subset_params = 0;
    skeyseed_template.pValue = skeyseed_secret;
    skeyseed_template.ulValueLen = HASH_LENGTH_MAX;

    crv = NSC_Initialize((CK_VOID_PTR)&pk11args);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_Initialize failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    count = slotListCount;
    crv = NSC_GetSlotList(PR_TRUE, slotList, &count);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_GetSlotList failed crv=0x%x\n", (unsigned int)crv);
        goto loser;
    }
    if ((count > slotListCount) || count < 1) {
        fprintf(stderr,
                "NSC_GetSlotList returned too many or too few slots: %d slots max=%d min=1\n",
                (int)count, (int)slotListCount);
        goto loser;
    }
    slotID = slotList[0];
    ikereq = fopen(reqfn, "r");
    ikeresp = stdout;
    while (fgets(buf, sizeof buf, ikereq) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            fputs(buf, ikeresp);
            continue;
        }
        /* [.....] */
        if (buf[0] == '[') {
            if (strncmp(buf, "[SHA-1]", 7) == 0) {
                ike_prf.prfMechanism = CKM_SHA_1_HMAC;
                ike2_prf.prfMechanism = CKM_SHA_1_HMAC;
            }
            if (strncmp(buf, "[SHA-224]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA224_HMAC;
                ike2_prf.prfMechanism = CKM_SHA224_HMAC;
            }
            if (strncmp(buf, "[SHA-256]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA256_HMAC;
                ike2_prf.prfMechanism = CKM_SHA256_HMAC;
            }
            if (strncmp(buf, "[SHA-384]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA384_HMAC;
                ike2_prf.prfMechanism = CKM_SHA384_HMAC;
            }
            if (strncmp(buf, "[SHA-512]", 9) == 0) {
                ike_prf.prfMechanism = CKM_SHA512_HMAC;
                ike2_prf.prfMechanism = CKM_SHA512_HMAC;
            }
            if (strncmp(buf, "[AES-XCBC", 9) == 0) {
                ike_prf.prfMechanism = CKM_AES_XCBC_MAC;
                ike2_prf.prfMechanism = CKM_AES_XCBC_MAC;
            }
            if (strncmp(buf, "[g^ir", 5) == 0) {
                if (sscanf(buf, "[g^ir length = %d]",
                           &gir_len) != 1) {
                    goto loser;
                }
                gir_len = gir_len / 8;
                if (gir)
                    free(gir);
                if (gir_new)
                    free(gir_new);
                gir = malloc(gir_len);
                gir_new = malloc(gir_len);
                gir_template[0].pValue = gir;
                gir_template[0].ulValueLen = gir_len;
                gir_new_template[0].pValue = gir_new;
                gir_new_template[0].ulValueLen = gir_len;
            }
            if (strncmp(buf, "[Ni", 3) == 0) {
                if (sscanf(buf, "[Ni length = %d]", &Ni_len) != 1) {
                    goto loser;
                }
                Ni_len = Ni_len / 8;
            }
            if (strncmp(buf, "[Nr", 3) == 0) {
                if (sscanf(buf, "[Nr length = %d]", &Nr_len) != 1) {
                    goto loser;
                }
                Nr_len = Nr_len / 8;
            }
            if (strncmp(buf, "[DKM", 4) == 0) {
                if (sscanf(buf, "[DKM length = %d]",
                           &DKM_len) != 1) {
                    goto loser;
                }
                DKM_len = DKM_len / 8;
                if (DKM)
                    free(DKM);
                DKM = malloc(DKM_len);
                dkm_template.pValue = DKM;
                dkm_template.ulValueLen = DKM_len;
            }
            if (strncmp(buf, "[Child SA DKM", 13) == 0) {
                if (sscanf(buf, "[Child SA DKM length = %d]",
                           &DKM_child_len) != 1) {
                    goto loser;
                }
                DKM_child_len = DKM_child_len / 8;
                if (DKM_child)
                    free(DKM_child);
                DKM_child = malloc(DKM_child_len);
                dkm_child_template.pValue = DKM_child;
                dkm_child_template.ulValueLen = DKM_child_len;
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* "COUNT = x" begins a new data set */
        if (strncmp(buf, "COUNT", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            int new_seed_len = Ni_len + Nr_len + SPIi_len + SPIr_len;
            if (seed_data_len != new_seed_len) {
                if (seed_data)
                    free(seed_data);
                seed_data_len = new_seed_len;
                seed_data = malloc(seed_data_len);
                Ni = seed_data;
                Nr = &seed_data[Ni_len];
                SPIi = &seed_data[Ni_len + Nr_len];
                SPIr = &seed_data[new_seed_len - SPIr_len];
                ike_prf.pNi = Ni;
                ike_prf.ulNiLen = Ni_len;
                ike_prf.pNr = Nr;
                ike_prf.ulNrLen = Nr_len;
                ike2_prf.pSeedData = seed_data;
            }
            memset(gir, 0, gir_len);
            memset(gir_new, 0, gir_len);
            memset(seed_data, 0, seed_data_len);
            fputs(buf, ikeresp);
            continue;
        }
        /* Ni = ... */
        if (strncmp(buf, "Ni", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < Ni_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &Ni[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* Nr = ... */
        if (strncmp(buf, "Nr", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < Nr_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &Nr[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* g^ir (new) = ... */
        if (strncmp(buf, "g^ir (new)", 10) == 0) {
            i = 10;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < gir_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &gir_new[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* g^ir = ... */
        if (strncmp(buf, "g^ir", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < gir_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &gir[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* SPIi = ... */
        if (strncmp(buf, "SPIi", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < SPIi_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &SPIi[j]);
            }
            fputs(buf, ikeresp);
            continue;
        }
        /* SPIr = ... */
        if (strncmp(buf, "SPIr", 4) == 0) {
            CK_SESSION_HANDLE session;
            CK_OBJECT_HANDLE gir_handle;
            CK_OBJECT_HANDLE gir_new_handle;
            CK_OBJECT_HANDLE skeyseed_handle;
            CK_OBJECT_HANDLE sk_d_handle;
            CK_OBJECT_HANDLE skeyseed_new_handle;
            CK_OBJECT_HANDLE dkm_handle;
            CK_OBJECT_HANDLE dkm_child_handle;
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; j < SPIr_len; i += 2, j++) {
                hex_to_byteval(&buf[i], &SPIr[j]);
            }
            fputs(buf, ikeresp);
            crv = NSC_OpenSession(slotID, 0, NULL, NULL, &session);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_OpenSession failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_CreateObject(session, gir_template,
                                   gir_template_count, &gir_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject (g^ir) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_CreateObject(session, gir_new_template,
                                   gir_new_template_count, &gir_new_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject (g^ir new) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            /* get the SKEYSEED key */
            ike_prf.bDataAsKey = CK_TRUE;
            ike_prf.bRekey = CK_FALSE;
            ike_prf.hNewKey = CK_INVALID_HANDLE;
            crv = NSC_DeriveKey(session, &ike_mech, gir_handle,
                                derive_template, derive_template_count - 1,
                                &skeyseed_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            skeyseed_template.ulValueLen = HASH_LENGTH_MAX;
            crv = NSC_GetAttributeValue(session, skeyseed_handle,
                                        &skeyseed_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYSEED = ", ikeresp);
            to_hex_str(buf, skeyseed_secret, skeyseed_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            /* get DKM */
            keyLen = DKM_len;
            ike2_prf.bHasSeedKey = CK_FALSE;
            ike2_prf.hSeedKey = CK_INVALID_HANDLE;
            ike2_prf.ulSeedDataLen = seed_data_len;
            crv = NSC_DeriveKey(session, &ike2_mech, skeyseed_handle,
                                derive_template, derive_template_count,
                                &dkm_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(DKM) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_GetAttributeValue(session, dkm_handle,
                                        &dkm_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(DKM) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("DKM = ", ikeresp);
            to_hex_str(buf, DKM, DKM_len);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            /* get the sk_d from the DKM */
            keyLen = skeyseed_template.ulValueLen;
            crv = NSC_DeriveKey(session, &subset_mech, dkm_handle,
                                derive_template, derive_template_count,
                                &sk_d_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(sk_d) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }

            /* get DKM child */
            keyLen = DKM_child_len;
            ike2_prf.bHasSeedKey = CK_FALSE;
            ike2_prf.hSeedKey = CK_INVALID_HANDLE;
            ike2_prf.ulSeedDataLen = Ni_len + Nr_len;
            crv = NSC_DeriveKey(session, &ike2_mech, sk_d_handle,
                                derive_template, derive_template_count,
                                &dkm_child_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(DKM Child SA) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_GetAttributeValue(session, dkm_child_handle,
                                        &dkm_child_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(DKM Child SA) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("DKM(Child SA) = ", ikeresp);
            to_hex_str(buf, DKM_child, DKM_child_len);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            /* get DKM child D-H*/
            keyLen = DKM_child_len;
            ike2_prf.bHasSeedKey = CK_TRUE;
            ike2_prf.hSeedKey = gir_new_handle;
            ike2_prf.ulSeedDataLen = Ni_len + Nr_len;
            crv = NSC_DeriveKey(session, &ike2_mech, sk_d_handle,
                                derive_template, derive_template_count,
                                &dkm_child_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(DKM Child SA D-H) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            crv = NSC_GetAttributeValue(session, dkm_child_handle,
                                        &dkm_child_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(DKM Child SA D-H) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("DKM(Child SA D-H) = ", ikeresp);
            to_hex_str(buf, DKM_child, DKM_child_len);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            /* get SKEYSEED(rekey) */
            ike_prf.bDataAsKey = CK_FALSE;
            ike_prf.bRekey = CK_TRUE;
            ike_prf.hNewKey = gir_new_handle;
            crv = NSC_DeriveKey(session, &ike_mech, sk_d_handle,
                                derive_template, derive_template_count - 1,
                                &skeyseed_new_handle);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(skeyid rekey) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            skeyseed_template.ulValueLen = HASH_LENGTH_MAX;
            crv = NSC_GetAttributeValue(session, skeyseed_new_handle,
                                        &skeyseed_template, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(skeyid) failed crv=0x%x\n",
                        (unsigned int)crv);
                goto loser;
            }
            fputs("SKEYSEED(rekey) = ", ikeresp);
            to_hex_str(buf, skeyseed_secret, skeyseed_template.ulValueLen);
            fputs(buf, ikeresp);
            fputc('\n', ikeresp);

            crv = NSC_CloseSession(session);
            continue;
        }
    }
loser:
    NSC_Finalize(NULL);
    if (gir)
        free(gir);
    if (gir_new)
        free(gir_new);
    if (seed_data)
        free(seed_data);
    if (DKM)
        free(DKM);
    if (DKM_child)
        free(DKM_child);
    if (ikereq)
        fclose(ikereq);
}

void
kbkdf(char *path)
{
    /* == Parser data == */
    char buf[610]; /* holds one line from the input REQUEST file. Needs to
                    * be large enough to hold the longest line:
                    * "KO = <600 hex digits>\n". */
    CK_ULONG L;
    unsigned char KI[64];
    unsigned int KI_len = 64;
    unsigned char KO[300];
    unsigned int KO_len = 300;
    /* This is used only with feedback mode. */
    unsigned char IV[64];
    unsigned int IV_len = 64;
    /* These are only used in counter mode with counter location as
     * MIDDLE_FIXED. */
    unsigned char BeforeFixedInputData[50];
    unsigned int BeforeFixedInputData_len = 50;
    unsigned char AfterFixedInputData[10];
    unsigned int AfterFixedInputData_len = 10;
    /* These are used with every KDF type. */
    unsigned char FixedInputData[60];
    unsigned int FixedInputData_len = 60;

    /* Counter locations:
     *
     * 0: not used
     * 1: beginning
     * 2: middle
     * 3: end */
    int ctr_location = 0;
    CK_ULONG counter_bitlen = 0;

    size_t buf_offset;
    size_t offset;

    FILE *kbkdf_req = NULL;
    FILE *kbkdf_resp = NULL;

    /* == PKCS#11 data == */
    CK_RV crv;

    CK_SLOT_ID slotList[10];
    CK_SLOT_ID slotID;
    CK_ULONG slotListCount = sizeof(slotList) / sizeof(slotList[0]);
    CK_ULONG slotCount = 0;

    CK_MECHANISM kdf = { 0 };

    CK_MECHANISM_TYPE prf_mech = 0;
    CK_BBOOL ck_true = CK_TRUE;

    /* We never need more than 3 data parameters. */
    CK_PRF_DATA_PARAM dataParams[3];
    CK_ULONG dataParams_len = 3;

    CK_SP800_108_COUNTER_FORMAT iterator = { CK_FALSE, 0 };

    CK_SP800_108_KDF_PARAMS kdfParams = { 0 };
    CK_SP800_108_FEEDBACK_KDF_PARAMS feedbackParams = { 0 };

    CK_OBJECT_CLASS ck_secret_key = CKO_SECRET_KEY;
    CK_KEY_TYPE ck_generic = CKK_GENERIC_SECRET;

    CK_ATTRIBUTE prf_template[] = {
        { CKA_VALUE, &KI, sizeof(KI) },
        { CKA_CLASS, &ck_secret_key, sizeof(ck_secret_key) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) }
    };
    CK_ULONG prf_template_count = sizeof(prf_template) / sizeof(prf_template[0]);

    CK_ATTRIBUTE derive_template[] = {
        { CKA_CLASS, &ck_secret_key, sizeof(ck_secret_key) },
        { CKA_KEY_TYPE, &ck_generic, sizeof(ck_generic) },
        { CKA_DERIVE, &ck_true, sizeof(ck_true) },
        { CKA_VALUE_LEN, &L, sizeof(L) }
    };
    CK_ULONG derive_template_count = sizeof(derive_template) / sizeof(derive_template[0]);

    CK_ATTRIBUTE output_key = { CKA_VALUE, KO, KO_len };

    const CK_C_INITIALIZE_ARGS pk11args = {
        NULL, NULL, NULL, NULL, CKF_LIBRARY_CANT_CREATE_OS_THREADS,
        (void *)"flags=readOnly,noCertDB,noModDB", NULL
    };

    /* == Start up PKCS#11 == */
    crv = NSC_Initialize((CK_VOID_PTR)&pk11args);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_Initialize failed crv=0x%x\n", (unsigned int)crv);
        goto done;
    }

    slotCount = slotListCount;
    crv = NSC_GetSlotList(PR_TRUE, slotList, &slotCount);
    if (crv != CKR_OK) {
        fprintf(stderr, "NSC_GetSlotList failed crv=0x%x\n", (unsigned int)crv);
        goto done;
    }
    if ((slotCount > slotListCount) || slotCount < 1) {
        fprintf(stderr,
                "NSC_GetSlotList returned too many or too few slots: %d slots max=%d min=1\n",
                (int)slotCount, (int)slotListCount);
        goto done;
    }
    slotID = slotList[0];

    /* == Start parsing the file == */
    kbkdf_req = fopen(path, "r");
    kbkdf_resp = stdout;

    while (fgets(buf, sizeof buf, kbkdf_req) != NULL) {
        /* If we have a comment, check if it tells us the type of KDF to use.
         * This differs per-file, so we have to parse it. */
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r') {
            if (strncmp(buf, "# KDF Mode Supported: Counter Mode", 34) == 0) {
                kdf.mechanism = CKM_SP800_108_COUNTER_KDF;
            }
            if (strncmp(buf, "# KDF Mode Supported: Feedback Mode", 35) == 0) {
                kdf.mechanism = CKM_SP800_108_FEEDBACK_KDF;
            }
            if (strncmp(buf, "# KDF Mode Supported: DblPipeline Mode", 38) == 0) {
                kdf.mechanism = CKM_SP800_108_DOUBLE_PIPELINE_KDF;
            }

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* [....] - context directive */
        if (buf[0] == '[') {
            /* PRF begins each new section. */
            if (strncmp(buf, "[PRF=CMAC_AES128]", 17) == 0) {
                prf_mech = CKM_AES_CMAC;
                KI_len = 16;
            } else if (strncmp(buf, "[PRF=CMAC_AES192]", 17) == 0) {
                prf_mech = CKM_AES_CMAC;
                KI_len = 24;
            } else if (strncmp(buf, "[PRF=CMAC_AES256]", 17) == 0) {
                prf_mech = CKM_AES_CMAC;
                KI_len = 32;
            } else if (strncmp(buf, "[PRF=HMAC_SHA1]", 15) == 0) {
                prf_mech = CKM_SHA_1_HMAC;
                KI_len = 20;
            } else if (strncmp(buf, "[PRF=HMAC_SHA224]", 17) == 0) {
                prf_mech = CKM_SHA224_HMAC;
                KI_len = 28;
            } else if (strncmp(buf, "[PRF=HMAC_SHA256]", 17) == 0) {
                prf_mech = CKM_SHA256_HMAC;
                KI_len = 32;
            } else if (strncmp(buf, "[PRF=HMAC_SHA384]", 17) == 0) {
                prf_mech = CKM_SHA384_HMAC;
                KI_len = 48;
            } else if (strncmp(buf, "[PRF=HMAC_SHA512]", 17) == 0) {
                prf_mech = CKM_SHA512_HMAC;
                KI_len = 64;
            } else if (strncmp(buf, "[PRF=", 5) == 0) {
                fprintf(stderr, "Invalid or unsupported PRF mechanism: %s\n", buf);
                goto done;
            }

            /* Then comes counter, if present. */
            if (strncmp(buf, "[CTRLOCATION=BEFORE_FIXED]", 26) == 0 ||
                strncmp(buf, "[CTRLOCATION=BEFORE_ITER]", 24) == 0) {
                ctr_location = 1;
            }
            if (strncmp(buf, "[CTRLOCATION=MIDDLE_FIXED]", 26) == 0 ||
                strncmp(buf, "[CTRLOCATION=AFTER_ITER]", 24) == 0) {
                ctr_location = 2;
            }
            if (strncmp(buf, "[CTRLOCATION=AFTER_FIXED]", 25) == 0) {
                ctr_location = 3;
            }

            /* If counter is present, then we need to know its size. */
            if (strncmp(buf, "[RLEN=", 6) == 0) {
                if (sscanf(buf, "[RLEN=%lu_BITS]", &counter_bitlen) != 1) {
                    goto done;
                }
            }

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* Each test contains a counter, an output length L, an input key KI,
         * maybe an initialization vector IV, one of a couple of fixed data
         * buffers, and finally the output key KO. */

        /* First comes COUNT. */
        if (strncmp(buf, "COUNT=", 6) == 0) {
            /* Clear all out data fields on each test. */
            memset(KI, 0, sizeof KI);
            memset(KO, 0, sizeof KO);
            memset(IV, 0, sizeof IV);
            memset(BeforeFixedInputData, 0, sizeof BeforeFixedInputData);
            memset(AfterFixedInputData, 0, sizeof AfterFixedInputData);
            memset(FixedInputData, 0, sizeof FixedInputData);

            /* Then reset lengths except KI: it was determined by PRF
             * selection above. */
            KO_len = 0;
            IV_len = 0;
            BeforeFixedInputData_len = 0;
            AfterFixedInputData_len = 0;
            FixedInputData_len = 0;

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* Then comes L. */
        if (strncmp(buf, "L = ", 4) == 0) {
            if (sscanf(buf, "L = %lu", &L) != 1) {
                goto done;
            }

            if ((L % 8) != 0) {
                fprintf(stderr, "Assumption that L was length in bits incorrect: %lu - %s", L, buf);
                fprintf(stderr, "Note that NSS only supports byte-aligned outputs and not bit-aligned outputs.\n");
                goto done;
            }

            L = L / 8;

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* Then comes KI. */
        if (strncmp(buf, "KI = ", 5) == 0) {
            buf_offset = 5;

            for (offset = 0; offset < KI_len; offset++, buf_offset += 2) {
                hex_to_byteval(buf + buf_offset, KI + offset);
            }

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* Then comes IVlen and IV, if present. */
        if (strncmp(buf, "IVlen = ", 8) == 0) {
            if (sscanf(buf, "IVlen = %u", &IV_len) != 1) {
                goto done;
            }

            if ((IV_len % 8) != 0) {
                fprintf(stderr, "Assumption that IV_len was length in bits incorrect: %u - %s. ", IV_len, buf);
                fprintf(stderr, "Note that NSS only supports byte-aligned inputs and not bit-aligned inputs.\n");
                goto done;
            }

            /* Need the IV length in bytes, not bits. */
            IV_len = IV_len / 8;

            fputs(buf, kbkdf_resp);
            continue;
        }
        if (strncmp(buf, "IV = ", 5) == 0) {
            buf_offset = 5;

            for (offset = 0; offset < IV_len; offset++, buf_offset += 2) {
                hex_to_byteval(buf + buf_offset, IV + offset);
            }

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* We might have DataBeforeCtr and DataAfterCtr if present. */
        if (strncmp(buf, "DataBeforeCtrLen = ", 19) == 0) {
            if (sscanf(buf, "DataBeforeCtrLen = %u", &BeforeFixedInputData_len) != 1) {
                goto done;
            }

            fputs(buf, kbkdf_resp);
            continue;
        }
        if (strncmp(buf, "DataBeforeCtrData = ", 20) == 0) {
            buf_offset = 20;

            for (offset = 0; offset < BeforeFixedInputData_len; offset++, buf_offset += 2) {
                hex_to_byteval(buf + buf_offset, BeforeFixedInputData + offset);
            }

            fputs(buf, kbkdf_resp);
            continue;
        }
        if (strncmp(buf, "DataAfterCtrLen = ", 18) == 0) {
            if (sscanf(buf, "DataAfterCtrLen = %u", &AfterFixedInputData_len) != 1) {
                goto done;
            }

            fputs(buf, kbkdf_resp);
            continue;
        }
        if (strncmp(buf, "DataAfterCtrData = ", 19) == 0) {
            buf_offset = 19;

            for (offset = 0; offset < AfterFixedInputData_len; offset++, buf_offset += 2) {
                hex_to_byteval(buf + buf_offset, AfterFixedInputData + offset);
            }

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* Otherwise, we might have FixedInputData, if present. */
        if (strncmp(buf, "FixedInputDataByteLen = ", 24) == 0) {
            if (sscanf(buf, "FixedInputDataByteLen = %u", &FixedInputData_len) != 1) {
                goto done;
            }

            fputs(buf, kbkdf_resp);
            continue;
        }
        if (strncmp(buf, "FixedInputData = ", 17) == 0) {
            buf_offset = 17;

            for (offset = 0; offset < FixedInputData_len; offset++, buf_offset += 2) {
                hex_to_byteval(buf + buf_offset, FixedInputData + offset);
            }

            fputs(buf, kbkdf_resp);
            continue;
        }

        /* Finally, run the KBKDF calculation when KO is passed. */
        if (strncmp(buf, "KO = ", 5) == 0) {
            CK_SESSION_HANDLE session;
            CK_OBJECT_HANDLE prf_key;
            CK_OBJECT_HANDLE derived_key;

            /* Open the session. */
            crv = NSC_OpenSession(slotID, 0, NULL, NULL, &session);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_OpenSession failed crv=0x%x\n", (unsigned int)crv);
                goto done;
            }

            /* Create the PRF key object. */
            prf_template[0].ulValueLen = KI_len;
            crv = NSC_CreateObject(session, prf_template, prf_template_count, &prf_key);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_CreateObject (prf_key) failed crv=0x%x\n", (unsigned int)crv);
                goto done;
            }

            /* Set up the KDF parameters. */
            if (kdf.mechanism == CKM_SP800_108_COUNTER_KDF) {
                /* Counter operates in one of three ways: counter before fixed
                 * input data, counter between fixed input data, and counter
                 * after fixed input data. In all cases, we have an iterator.
                 */
                iterator.ulWidthInBits = counter_bitlen;

                if (ctr_location == 0 || ctr_location > 3) {
                    fprintf(stderr, "Expected ctr_location != 0 for Counter Mode KDF but got 0.\n");
                    goto done;
                } else if (ctr_location == 1) {
                    /* Counter before */
                    dataParams[0].type = CK_SP800_108_ITERATION_VARIABLE;
                    dataParams[0].pValue = &iterator;
                    dataParams[0].ulValueLen = sizeof(iterator);

                    dataParams[1].type = CK_SP800_108_BYTE_ARRAY;
                    dataParams[1].pValue = FixedInputData;
                    dataParams[1].ulValueLen = FixedInputData_len;

                    dataParams_len = 2;
                } else if (ctr_location == 2) {
                    /* Counter between */
                    dataParams[0].type = CK_SP800_108_BYTE_ARRAY;
                    dataParams[0].pValue = BeforeFixedInputData;
                    dataParams[0].ulValueLen = BeforeFixedInputData_len;

                    dataParams[1].type = CK_SP800_108_ITERATION_VARIABLE;
                    dataParams[1].pValue = &iterator;
                    dataParams[1].ulValueLen = sizeof(iterator);

                    dataParams[2].type = CK_SP800_108_BYTE_ARRAY;
                    dataParams[2].pValue = AfterFixedInputData;
                    dataParams[2].ulValueLen = AfterFixedInputData_len;

                    dataParams_len = 3;
                } else {
                    /* Counter after */
                    dataParams[0].type = CK_SP800_108_BYTE_ARRAY;
                    dataParams[0].pValue = FixedInputData;
                    dataParams[0].ulValueLen = FixedInputData_len;

                    dataParams[1].type = CK_SP800_108_ITERATION_VARIABLE;
                    dataParams[1].pValue = &iterator;
                    dataParams[1].ulValueLen = sizeof(iterator);

                    dataParams_len = 2;
                }
            } else if (kdf.mechanism == CKM_SP800_108_FEEDBACK_KDF || kdf.mechanism == CKM_SP800_108_DOUBLE_PIPELINE_KDF) {
                /* When counter_bitlen != 0, we have an optional counter. */
                if (counter_bitlen != 0) {
                    iterator.ulWidthInBits = counter_bitlen;

                    if (ctr_location == 0 || ctr_location > 3) {
                        fprintf(stderr, "Expected ctr_location != 0 for Counter Mode KDF but got 0.\n");
                        goto done;
                    } else if (ctr_location == 1) {
                        /* Counter before */
                        dataParams[0].type = CK_SP800_108_OPTIONAL_COUNTER;
                        dataParams[0].pValue = &iterator;
                        dataParams[0].ulValueLen = sizeof(iterator);

                        dataParams[1].type = CK_SP800_108_ITERATION_VARIABLE;
                        dataParams[1].pValue = NULL;
                        dataParams[1].ulValueLen = 0;

                        dataParams[2].type = CK_SP800_108_BYTE_ARRAY;
                        dataParams[2].pValue = FixedInputData;
                        dataParams[2].ulValueLen = FixedInputData_len;

                        dataParams_len = 3;
                    } else if (ctr_location == 2) {
                        /* Counter between */
                        dataParams[0].type = CK_SP800_108_ITERATION_VARIABLE;
                        dataParams[0].pValue = NULL;
                        dataParams[0].ulValueLen = 0;

                        dataParams[1].type = CK_SP800_108_OPTIONAL_COUNTER;
                        dataParams[1].pValue = &iterator;
                        dataParams[1].ulValueLen = sizeof(iterator);

                        dataParams[2].type = CK_SP800_108_BYTE_ARRAY;
                        dataParams[2].pValue = FixedInputData;
                        dataParams[2].ulValueLen = FixedInputData_len;

                        dataParams_len = 3;
                    } else {
                        /* Counter after */
                        dataParams[0].type = CK_SP800_108_ITERATION_VARIABLE;
                        dataParams[0].pValue = NULL;
                        dataParams[0].ulValueLen = 0;

                        dataParams[1].type = CK_SP800_108_BYTE_ARRAY;
                        dataParams[1].pValue = FixedInputData;
                        dataParams[1].ulValueLen = FixedInputData_len;

                        dataParams[2].type = CK_SP800_108_OPTIONAL_COUNTER;
                        dataParams[2].pValue = &iterator;
                        dataParams[2].ulValueLen = sizeof(iterator);

                        dataParams_len = 3;
                    }
                } else {
                    dataParams[0].type = CK_SP800_108_ITERATION_VARIABLE;
                    dataParams[0].pValue = NULL;
                    dataParams[0].ulValueLen = 0;

                    dataParams[1].type = CK_SP800_108_BYTE_ARRAY;
                    dataParams[1].pValue = FixedInputData;
                    dataParams[1].ulValueLen = FixedInputData_len;

                    dataParams_len = 2;
                }
            }

            if (kdf.mechanism != CKM_SP800_108_FEEDBACK_KDF) {
                kdfParams.prfType = prf_mech;
                kdfParams.ulNumberOfDataParams = dataParams_len;
                kdfParams.pDataParams = dataParams;

                kdf.pParameter = &kdfParams;
                kdf.ulParameterLen = sizeof(kdfParams);
            } else {
                feedbackParams.prfType = prf_mech;
                feedbackParams.ulNumberOfDataParams = dataParams_len;
                feedbackParams.pDataParams = dataParams;
                feedbackParams.ulIVLen = IV_len;
                if (IV_len == 0) {
                    feedbackParams.pIV = NULL;
                } else {
                    feedbackParams.pIV = IV;
                }

                kdf.pParameter = &feedbackParams;
                kdf.ulParameterLen = sizeof(feedbackParams);
            }

            crv = NSC_DeriveKey(session, &kdf, prf_key, derive_template, derive_template_count, &derived_key);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_DeriveKey(derived_key) failed crv=0x%x\n", (unsigned int)crv);
                goto done;
            }

            crv = NSC_GetAttributeValue(session, derived_key, &output_key, 1);
            if (crv != CKR_OK) {
                fprintf(stderr, "NSC_GetAttribute(derived_value) failed crv=0x%x\n", (unsigned int)crv);
                goto done;
            }

            fputs("KO = ", kbkdf_resp);
            to_hex_str(buf, KO, output_key.ulValueLen);
            fputs(buf, kbkdf_resp);
            fputs("\r\n", kbkdf_resp);

            continue;
        }
    }

done:
    if (kbkdf_req != NULL) {
        fclose(kbkdf_req);
    }
    if (kbkdf_resp != stdout && kbkdf_resp != NULL) {
        fclose(kbkdf_resp);
    }

    return;
}

int
main(int argc, char **argv)
{
    if (argc < 2)
        exit(-1);

    RNG_RNGInit();
    SECOID_Init();

    /*************/
    /*   TDEA    */
    /*************/
    if (strcmp(argv[1], "tdea") == 0) {
        /* argv[2]=kat|mmt|mct argv[3]=ecb|cbc argv[4]=<test name>.req */
        if (strcmp(argv[2], "kat") == 0) {
            /* Known Answer Test (KAT) */
            tdea_kat_mmt(argv[4]);
        } else if (strcmp(argv[2], "mmt") == 0) {
            /* Multi-block Message Test (MMT) */
            tdea_kat_mmt(argv[4]);
        } else if (strcmp(argv[2], "mct") == 0) {
            /* Monte Carlo Test (MCT) */
            if (strcmp(argv[3], "ecb") == 0) {
                /* ECB mode */
                tdea_mct(NSS_DES_EDE3, argv[4]);
            } else if (strcmp(argv[3], "cbc") == 0) {
                /* CBC mode */
                tdea_mct(NSS_DES_EDE3_CBC, argv[4]);
            }
        }
        /*************/
        /*   AES     */
        /*************/
    } else if (strcmp(argv[1], "aes") == 0) {
        /* argv[2]=kat|mmt|mct argv[3]=ecb|cbc argv[4]=<test name>.req */
        if (strcmp(argv[2], "kat") == 0) {
            /* Known Answer Test (KAT) */
            aes_kat_mmt(argv[4]);
        } else if (strcmp(argv[2], "mmt") == 0) {
            /* Multi-block Message Test (MMT) */
            aes_kat_mmt(argv[4]);
        } else if (strcmp(argv[2], "gcm") == 0) {
            if (strcmp(argv[3], "decrypt") == 0) {
                aes_gcm(argv[4], 0);
            } else if (strcmp(argv[3], "encrypt_extiv") == 0) {
                aes_gcm(argv[4], 1);
            } else if (strcmp(argv[3], "encrypt_intiv") == 0) {
                aes_gcm(argv[4], 2);
            }
        } else if (strcmp(argv[2], "mct") == 0) {
            /* Monte Carlo Test (MCT) */
            if (strcmp(argv[3], "ecb") == 0) {
                /* ECB mode */
                aes_ecb_mct(argv[4]);
            } else if (strcmp(argv[3], "cbc") == 0) {
                /* CBC mode */
                aes_cbc_mct(argv[4]);
            }
        }
        /*************/
        /*   SHA     */
        /*************/
    } else if (strcmp(argv[1], "sha") == 0) {
        sha_test(argv[2]);
        /*************/
        /*   RSA     */
        /*************/
    } else if (strcmp(argv[1], "rsa") == 0) {
        /* argv[2]=siggen|sigver */
        /* argv[3]=<test name>.req */
        if (strcmp(argv[2], "siggen") == 0) {
            /* Signature Generation Test */
            rsa_siggen_test(argv[3]);
        } else if (strcmp(argv[2], "sigver") == 0) {
            /* Signature Verification Test */
            rsa_sigver_test(argv[3]);
        } else if (strcmp(argv[2], "keypair") == 0) {
            /* Key Pair Generation Test */
            rsa_keypair_test(argv[3]);
        }
        /*************/
        /*   HMAC    */
        /*************/
    } else if (strcmp(argv[1], "hmac") == 0) {
        hmac_test(argv[2]);
        /*************/
        /*   DSA     */
        /*************/
    } else if (strcmp(argv[1], "dsa") == 0) {
        /* argv[2]=keypair|pqggen|pqgver|siggen|sigver */
        /* argv[3]=<test name>.req */
        if (strcmp(argv[2], "keypair") == 0) {
            /* Key Pair Generation Test */
            dsa_keypair_test(argv[3]);
        } else if (strcmp(argv[2], "pqggen") == 0) {
            /* Domain Parameter Generation Test */
            dsa_pqggen_test(argv[3]);
        } else if (strcmp(argv[2], "pqgver") == 0) {
            /* Domain Parameter Validation Test */
            dsa_pqgver_test(argv[3]);
        } else if (strcmp(argv[2], "siggen") == 0) {
            /* Signature Generation Test */
            dsa_siggen_test(argv[3]);
        } else if (strcmp(argv[2], "sigver") == 0) {
            /* Signature Verification Test */
            dsa_sigver_test(argv[3]);
        }
        /*************/
        /*   ECDSA   */
        /*************/
    } else if (strcmp(argv[1], "ecdsa") == 0) {
        /* argv[2]=keypair|pkv|siggen|sigver argv[3]=<test name>.req */
        if (strcmp(argv[2], "keypair") == 0) {
            /* Key Pair Generation Test */
            ecdsa_keypair_test(argv[3]);
        } else if (strcmp(argv[2], "pkv") == 0) {
            /* Public Key Validation Test */
            ecdsa_pkv_test(argv[3]);
        } else if (strcmp(argv[2], "siggen") == 0) {
            /* Signature Generation Test */
            ecdsa_siggen_test(argv[3]);
        } else if (strcmp(argv[2], "sigver") == 0) {
            /* Signature Verification Test */
            ecdsa_sigver_test(argv[3]);
        }
        /*************/
        /*   ECDH   */
        /*************/
    } else if (strcmp(argv[1], "ecdh") == 0) {
        /* argv[2]={init|resp}-{func|verify} argv[3]=<test name>.req */
        if (strcmp(argv[2], "init-func") == 0) {
            ecdh_functional(argv[3], 0);
        } else if (strcmp(argv[2], "resp-func") == 0) {
            ecdh_functional(argv[3], 1);
        } else if (strcmp(argv[2], "init-verify") == 0) {
            ecdh_verify(argv[3], 0);
        } else if (strcmp(argv[2], "resp-verify") == 0) {
            ecdh_verify(argv[3], 1);
        }
        /*************/
        /*   DH   */
        /*************/
    } else if (strcmp(argv[1], "dh") == 0) {
        /* argv[2]={init|resp}-{func|verify} argv[3]=<test name>.req */
        if (strcmp(argv[2], "init-func") == 0) {
            dh_functional(argv[3], 0);
        } else if (strcmp(argv[2], "resp-func") == 0) {
            dh_functional(argv[3], 1);
        } else if (strcmp(argv[2], "init-verify") == 0) {
            dh_verify(argv[3], 0);
        } else if (strcmp(argv[2], "resp-verify") == 0) {
            dh_verify(argv[3], 1);
        }
        /*************/
        /*   RNG     */
        /*************/
    } else if (strcmp(argv[1], "rng") == 0) {
        /* argv[2]=vst|mct argv[3]=<test name>.req */
        if (strcmp(argv[2], "vst") == 0) {
            /* Variable Seed Test */
            rng_vst(argv[3]);
        } else if (strcmp(argv[2], "mct") == 0) {
            /* Monte Carlo Test */
            rng_mct(argv[3]);
        }
    } else if (strcmp(argv[1], "drbg") == 0) {
        /* Variable Seed Test */
        drbg(argv[2]);
    } else if (strcmp(argv[1], "ddrbg") == 0) {
        debug = 1;
        drbg(argv[2]);
    } else if (strcmp(argv[1], "tls") == 0) {
        tls(argv[2]);
    } else if (strcmp(argv[1], "ikev1") == 0) {
        ikev1(argv[2]);
    } else if (strcmp(argv[1], "ikev1-psk") == 0) {
        ikev1_psk(argv[2]);
    } else if (strcmp(argv[1], "ikev2") == 0) {
        ikev2(argv[2]);
    } else if (strcmp(argv[1], "kbkdf") == 0) {
        kbkdf(argv[2]);
    }
    return 0;
}
