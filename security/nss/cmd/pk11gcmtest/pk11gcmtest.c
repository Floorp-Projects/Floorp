/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "pk11pub.h"
#include "secerr.h"
#include "nss.h"

static SECStatus
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

static SECStatus
aes_encrypt_buf(
    const unsigned char *key, unsigned int keysize,
    const unsigned char *iv, unsigned int ivsize,
    unsigned char *output, unsigned int *outputlen, unsigned int maxoutputlen,
    const unsigned char *input, unsigned int inputlen,
    const unsigned char *aad, unsigned int aadlen, unsigned int tagsize)
{
    SECStatus rv = SECFailure;
    SECItem key_item;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *symKey = NULL;
    CK_NSS_GCM_PARAMS gcm_params;
    SECItem param;

    /* Import key into NSS. */
    key_item.type = siBuffer;
    key_item.data = (unsigned char *)key; /* const cast */
    key_item.len = keysize;
    slot = PK11_GetInternalSlot();
    symKey = PK11_ImportSymKey(slot, CKM_AES_GCM, PK11_OriginUnwrap,
                               CKA_ENCRYPT, &key_item, NULL);
    PK11_FreeSlot(slot);
    slot = NULL;
    if (!symKey) {
        fprintf(stderr, "PK11_ImportSymKey failed\n");
        goto loser;
    }

    gcm_params.pIv = (unsigned char *)iv; /* const cast */
    gcm_params.ulIvLen = ivsize;
    gcm_params.pAAD = (unsigned char *)aad; /* const cast */
    gcm_params.ulAADLen = aadlen;
    gcm_params.ulTagBits = tagsize * 8;

    param.type = siBuffer;
    param.data = (unsigned char *)&gcm_params;
    param.len = sizeof(gcm_params);

    if (PK11_Encrypt(symKey, CKM_AES_GCM, &param,
                     output, outputlen, maxoutputlen,
                     input, inputlen) != SECSuccess) {
        fprintf(stderr, "PK11_Encrypt failed\n");
        goto loser;
    }

    rv = SECSuccess;

loser:
    if (symKey != NULL) {
        PK11_FreeSymKey(symKey);
    }
    return rv;
}

static SECStatus
aes_decrypt_buf(
    const unsigned char *key, unsigned int keysize,
    const unsigned char *iv, unsigned int ivsize,
    unsigned char *output, unsigned int *outputlen, unsigned int maxoutputlen,
    const unsigned char *input, unsigned int inputlen,
    const unsigned char *aad, unsigned int aadlen,
    const unsigned char *tag, unsigned int tagsize)
{
    SECStatus rv = SECFailure;
    unsigned char concatenated[11 * 16]; /* 1 to 11 blocks */
    SECItem key_item;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *symKey = NULL;
    CK_NSS_GCM_PARAMS gcm_params;
    SECItem param;

    if (inputlen + tagsize > sizeof(concatenated)) {
        fprintf(stderr, "aes_decrypt_buf: local buffer too small\n");
        goto loser;
    }
    memcpy(concatenated, input, inputlen);
    memcpy(concatenated + inputlen, tag, tagsize);

    /* Import key into NSS. */
    key_item.type = siBuffer;
    key_item.data = (unsigned char *)key; /* const cast */
    key_item.len = keysize;
    slot = PK11_GetInternalSlot();
    symKey = PK11_ImportSymKey(slot, CKM_AES_GCM, PK11_OriginUnwrap,
                               CKA_DECRYPT, &key_item, NULL);
    PK11_FreeSlot(slot);
    slot = NULL;
    if (!symKey) {
        fprintf(stderr, "PK11_ImportSymKey failed\n");
        goto loser;
    }

    gcm_params.pIv = (unsigned char *)iv;
    gcm_params.ulIvLen = ivsize;
    gcm_params.pAAD = (unsigned char *)aad;
    gcm_params.ulAADLen = aadlen;
    gcm_params.ulTagBits = tagsize * 8;

    param.type = siBuffer;
    param.data = (unsigned char *)&gcm_params;
    param.len = sizeof(gcm_params);

    if (PK11_Decrypt(symKey, CKM_AES_GCM, &param,
                     output, outputlen, maxoutputlen,
                     concatenated, inputlen + tagsize) != SECSuccess) {
        goto loser;
    }

    rv = SECSuccess;

loser:
    if (symKey != NULL) {
        PK11_FreeSymKey(symKey);
    }
    return rv;
}

/*
 * Perform the AES Known Answer Test (KAT) in Galois Counter Mode (GCM).
 *
 * respfn is the pathname of the RESPONSE file.
 */
static void
aes_gcm_kat(const char *respfn)
{
    char buf[512]; /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "CIPHERTEXT = <320 hex digits>\n".
                         */
    FILE *aesresp; /* input stream from the RESPONSE file */
    int i, j;
    unsigned int test_group = 0;
    unsigned int num_tests = 0;
    PRBool is_encrypt;
    unsigned char key[32]; /* 128, 192, or 256 bits */
    unsigned int keysize = 16;
    unsigned char iv[10 * 16]; /* 1 to 10 blocks */
    unsigned int ivsize = 12;
    unsigned char plaintext[10 * 16]; /* 1 to 10 blocks */
    unsigned int plaintextlen = 0;
    unsigned char aad[10 * 16]; /* 1 to 10 blocks */
    unsigned int aadlen = 0;
    unsigned char ciphertext[10 * 16]; /* 1 to 10 blocks */
    unsigned int ciphertextlen = 0;
    unsigned char tag[16];
    unsigned int tagsize = 16;
    unsigned char output[10 * 16]; /* 1 to 10 blocks */
    unsigned int outputlen = 0;

    unsigned int expected_keylen = 0;
    unsigned int expected_ivlen = 0;
    unsigned int expected_ptlen = 0;
    unsigned int expected_aadlen = 0;
    unsigned int expected_taglen = 0;
    SECStatus rv;

    if (strstr(respfn, "Encrypt") != NULL) {
        is_encrypt = PR_TRUE;
    } else if (strstr(respfn, "Decrypt") != NULL) {
        is_encrypt = PR_FALSE;
    } else {
        fprintf(stderr, "Input file name must contain Encrypt or Decrypt\n");
        exit(1);
    }
    aesresp = fopen(respfn, "r");
    if (aesresp == NULL) {
        fprintf(stderr, "Cannot open input file %s\n", respfn);
        exit(1);
    }
    while (fgets(buf, sizeof buf, aesresp) != NULL) {
        /* a comment or blank line */
        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }
        /* [Keylen = ...], [IVlen = ...], etc. */
        if (buf[0] == '[') {
            if (strncmp(&buf[1], "Keylen = ", 9) == 0) {
                expected_keylen = atoi(&buf[10]);
            } else if (strncmp(&buf[1], "IVlen = ", 8) == 0) {
                expected_ivlen = atoi(&buf[9]);
            } else if (strncmp(&buf[1], "PTlen = ", 8) == 0) {
                expected_ptlen = atoi(&buf[9]);
            } else if (strncmp(&buf[1], "AADlen = ", 9) == 0) {
                expected_aadlen = atoi(&buf[10]);
            } else if (strncmp(&buf[1], "Taglen = ", 9) == 0) {
                expected_taglen = atoi(&buf[10]);

                test_group++;
                if (test_group > 1) {
                    /* Report num_tests for the previous test group. */
                    printf("%u tests\n", num_tests);
                }
                num_tests = 0;
                printf("Keylen = %u, IVlen = %u, PTlen = %u, AADlen = %u, "
                       "Taglen = %u: ",
                       expected_keylen, expected_ivlen,
                       expected_ptlen, expected_aadlen, expected_taglen);
                /* Convert lengths in bits to lengths in bytes. */
                PORT_Assert(expected_keylen % 8 == 0);
                expected_keylen /= 8;
                PORT_Assert(expected_ivlen % 8 == 0);
                expected_ivlen /= 8;
                PORT_Assert(expected_ptlen % 8 == 0);
                expected_ptlen /= 8;
                PORT_Assert(expected_aadlen % 8 == 0);
                expected_aadlen /= 8;
                PORT_Assert(expected_taglen % 8 == 0);
                expected_taglen /= 8;
            } else {
                fprintf(stderr, "Unexpected input line: %s\n", buf);
                exit(1);
            }
            continue;
        }
        /* "Count = x" begins a new data set */
        if (strncmp(buf, "Count", 5) == 0) {
            /* zeroize the variables for the test with this data set */
            memset(key, 0, sizeof key);
            keysize = 0;
            memset(iv, 0, sizeof iv);
            ivsize = 0;
            memset(plaintext, 0, sizeof plaintext);
            plaintextlen = 0;
            memset(aad, 0, sizeof aad);
            aadlen = 0;
            memset(ciphertext, 0, sizeof ciphertext);
            ciphertextlen = 0;
            memset(output, 0, sizeof output);
            outputlen = 0;
            num_tests++;
            continue;
        }
        /* Key = ... */
        if (strncmp(buf, "Key", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &key[j]);
            }
            keysize = j;
            if (keysize != expected_keylen) {
                fprintf(stderr, "Unexpected key length: %u vs. %u\n",
                        keysize, expected_keylen);
                exit(1);
            }
            continue;
        }
        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &iv[j]);
            }
            ivsize = j;
            if (ivsize != expected_ivlen) {
                fprintf(stderr, "Unexpected IV length: %u vs. %u\n",
                        ivsize, expected_ivlen);
                exit(1);
            }
            continue;
        }
        /* PT = ... */
        if (strncmp(buf, "PT", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &plaintext[j]);
            }
            plaintextlen = j;
            if (plaintextlen != expected_ptlen) {
                fprintf(stderr, "Unexpected PT length: %u vs. %u\n",
                        plaintextlen, expected_ptlen);
                exit(1);
            }

            if (!is_encrypt) {
                rv = aes_decrypt_buf(key, keysize, iv, ivsize,
                                     output, &outputlen, sizeof output,
                                     ciphertext, ciphertextlen, aad, aadlen, tag, tagsize);
                if (rv != SECSuccess) {
                    fprintf(stderr, "aes_decrypt_buf failed\n");
                    goto loser;
                }
                if (outputlen != plaintextlen) {
                    fprintf(stderr, "aes_decrypt_buf: wrong output size\n");
                    goto loser;
                }
                if (memcmp(output, plaintext, plaintextlen) != 0) {
                    fprintf(stderr, "aes_decrypt_buf: wrong plaintext\n");
                    goto loser;
                }
            }
            continue;
        }
        /* FAIL */
        if (strncmp(buf, "FAIL", 4) == 0) {
            plaintextlen = 0;

            PORT_Assert(!is_encrypt);
            rv = aes_decrypt_buf(key, keysize, iv, ivsize,
                                 output, &outputlen, sizeof output,
                                 ciphertext, ciphertextlen, aad, aadlen, tag, tagsize);
            if (rv != SECFailure) {
                fprintf(stderr, "aes_decrypt_buf succeeded unexpectedly\n");
                goto loser;
            }
            if (PORT_GetError() != SEC_ERROR_BAD_DATA) {
                fprintf(stderr, "aes_decrypt_buf failed with incorrect "
                                "error code\n");
                goto loser;
            }
            continue;
        }
        /* AAD = ... */
        if (strncmp(buf, "AAD", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &aad[j]);
            }
            aadlen = j;
            if (aadlen != expected_aadlen) {
                fprintf(stderr, "Unexpected AAD length: %u vs. %u\n",
                        aadlen, expected_aadlen);
                exit(1);
            }
            continue;
        }
        /* CT = ... */
        if (strncmp(buf, "CT", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &ciphertext[j]);
            }
            ciphertextlen = j;
            if (ciphertextlen != expected_ptlen) {
                fprintf(stderr, "Unexpected CT length: %u vs. %u\n",
                        ciphertextlen, expected_ptlen);
                exit(1);
            }
            continue;
        }
        /* Tag = ... */
        if (strncmp(buf, "Tag", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j = 0; isxdigit(buf[i]); i += 2, j++) {
                hex_to_byteval(&buf[i], &tag[j]);
            }
            tagsize = j;
            if (tagsize != expected_taglen) {
                fprintf(stderr, "Unexpected tag length: %u vs. %u\n",
                        tagsize, expected_taglen);
                exit(1);
            }

            if (is_encrypt) {
                rv = aes_encrypt_buf(key, keysize, iv, ivsize,
                                     output, &outputlen, sizeof output,
                                     plaintext, plaintextlen, aad, aadlen, tagsize);
                if (rv != SECSuccess) {
                    fprintf(stderr, "aes_encrypt_buf failed\n");
                    goto loser;
                }
                if (outputlen != plaintextlen + tagsize) {
                    fprintf(stderr, "aes_encrypt_buf: wrong output size\n");
                    goto loser;
                }
                if (memcmp(output, ciphertext, plaintextlen) != 0) {
                    fprintf(stderr, "aes_encrypt_buf: wrong ciphertext\n");
                    goto loser;
                }
                if (memcmp(output + plaintextlen, tag, tagsize) != 0) {
                    fprintf(stderr, "aes_encrypt_buf: wrong tag\n");
                    goto loser;
                }
            }
            continue;
        }
    }
    /* Report num_tests for the last test group. */
    printf("%u tests\n", num_tests);
    printf("%u test groups\n", test_group);
    printf("PASS\n");
loser:
    fclose(aesresp);
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        return 1;
    }

    if (NSS_NoDB_Init(NULL) != SECSuccess) {
        return 1;
    }

    /*************/
    /*   AES     */
    /*************/
    if (strcmp(argv[1], "aes") == 0) {
        /* argv[2]=kat argv[3]=gcm argv[4]=<test name>.rsp */
        if (strcmp(argv[2], "kat") == 0) {
            /* Known Answer Test (KAT) */
            aes_gcm_kat(argv[4]);
        }
    }

    if (NSS_Shutdown() != SECSuccess) {
        return 1;
    }
    return 0;
}
