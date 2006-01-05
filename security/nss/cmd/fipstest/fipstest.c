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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "secitem.h"
#include "blapi.h"
#include "nss.h"
#include "secerr.h"
#include "secoidt.h"
#include "keythi.h"
#include "ec.h"
#if 0
#include "../../lib/freebl/mpi/mpi.h"
#endif

#ifdef NSS_ENABLE_ECC
extern SECStatus
EC_DecodeParams(const SECItem *encodedParams, ECParams **ecparams);
#endif

#define ENCRYPT 1
#define DECRYPT 0
#define BYTE unsigned char

SECStatus
hex_from_2char(const unsigned char *c2, unsigned char *byteval)
{
    int i;
    unsigned char offset;
    *byteval = 0;
    for (i=0; i<2; i++) {
	if (c2[i] >= '0' && c2[i] <= '9') {
	    offset = c2[i] - '0';
	    *byteval |= offset << 4*(1-i);
	} else if (c2[i] >= 'a' && c2[i] <= 'f') {
	    offset = c2[i] - 'a';
	    *byteval |= (offset + 10) << 4*(1-i);
	} else if (c2[i] >= 'A' && c2[i] <= 'F') {
	    offset = c2[i] - 'A';
	    *byteval |= (offset + 10) << 4*(1-i);
	} else {
	    return SECFailure;
	}
    }
    return SECSuccess;
}

SECStatus
char2_from_hex(unsigned char byteval, unsigned char *c2, char a)
{
    int i;
    unsigned char offset;
    for (i=0; i<2; i++) {
	offset = (byteval >> 4*(1-i)) & 0x0f;
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
    for (i=0; i<len; i++) {
	char2_from_hex(buf[i], &str[2*i], 'a');
    }
    str[2*len] = '\0';
}

void
to_hex_str_cap(char *str, const unsigned char *buf, unsigned int len)
{
    unsigned int i;
    for (i=0; i<len; i++) {
	char2_from_hex(buf[i], &str[2*i], 'A');
    }
    str[2*len] = '\0';
}

/*
 * Convert a string of hex digits (str) to an array (buf) of len bytes.
 * Return PR_TRUE if the hex string can fit in the byte array.  Return
 * PR_FALSE if the hex string is empty or is too long.
 */
PRBool
from_hex_str(unsigned char *buf, unsigned int len, const char *str)
{
    unsigned int nxdigit;  /* number of hex digits in str */
    unsigned int i;  /* index into buf */
    unsigned int j;  /* index into str */

    /* count the hex digits */
    nxdigit = 0;
    for (nxdigit = 0; isxdigit(str[nxdigit]); nxdigit++) {
	/* empty body */
    }
    if (nxdigit == 0) {
	return PR_FALSE;
    }
    if (nxdigit > 2*len) {
	/*
	 * The input hex string is too long, but we allow it if the
	 * extra digits are leading 0's.
	 */
	for (j = 0; j < nxdigit-2*len; j++) {
	    if (str[j] != '0') {
		return PR_FALSE;
	    }
	}
	/* skip leading 0's */
	str += nxdigit-2*len;
	nxdigit = 2*len;
    }
    for (i=0, j=0; i< len; i++) {
	if (2*i < 2*len-nxdigit) {
	    /* Handle a short input as if we padded it with leading 0's. */
	    if (2*i+1 < 2*len-nxdigit) {
		buf[i] = 0;
	    } else {
		char tmp[2];
		tmp[0] = '0';
		tmp[1] = str[j];
		hex_from_2char(tmp, &buf[i]);
		j++;
	    }
	} else {
	    hex_from_2char(&str[j], &buf[i]);
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
    unsigned char doublecheck[8*20];  /* 1 to 20 blocks */
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
    unsigned char doublecheck[8*20];  /* 1 to 20 blocks */
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
    char buf[180];      /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "CIPHERTEXT = <180 hex digits>\n".
                         */
    FILE *req;       /* input stream from the REQUEST file */
    FILE *resp;      /* output stream to the RESPONSE file */
    int i, j;
    int mode;           /* NSS_DES_EDE3 (ECB) or NSS_DES_EDE3_CBC */
    int crypt = DECRYPT;    /* 1 means encrypt, 0 means decrypt */
    unsigned char key[24];              /* TDEA 3 key bundle */
    unsigned int numKeys = 0;
    unsigned char iv[8];		/* for all modes except ECB */
    unsigned char plaintext[8*20];     /* 1 to 20 blocks */
    unsigned int plaintextlen;
    unsigned char ciphertext[8*20];   /* 1 to 20 blocks */  
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
                for (j=0; isxdigit(buf[i]); i+=2,j++) {
                    hex_from_2char(&buf[i], &key[j]);
                    key[j+8] = key[j];
                    key[j+16] = key[j];
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
                for (j=0; isxdigit(buf[i]); i+=2,j++) {
                    hex_from_2char(&buf[i], &key[j]);
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
                for (j=8; isxdigit(buf[i]); i+=2,j++) {
                    hex_from_2char(&buf[i], &key[j]);
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
                for (j=16; isxdigit(buf[i]); i+=2,j++) {
                    hex_from_2char(&buf[i], &key[j]);
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
            for (j=0; j<sizeof iv; i+=2,j++) {
                hex_from_2char(&buf[i], &iv[j]);
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
            for (j=0; isxdigit(buf[i]); i+=2,j++) {
                hex_from_2char(&buf[i], &plaintext[j]);
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
            for (j=0; isxdigit(buf[i]); i+=2,j++) {
                hex_from_2char(&buf[i], &ciphertext[j]);
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
BYTE odd_parity( BYTE in)
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
    for (k=0; k<8; k++) {
        key[k] ^= text[k];
    }
    /* key2 */
    if (numKeys == 2 || numKeys == 3)  {
        /* key2 independent */
        for (k=8; k<16; k++) {
            /* key2[i+1] = KEY2[i] xor PT/CT[j-1] */
            key[k] ^= text_1[k-8];
        }
    } else {
        /* key2 == key 1 */
        for (k=8; k<16; k++) {
            /* key2[i+1] = KEY2[i] xor PT/CT[j] */
            key[k] = key[k-8];
        }
    }
    /* key3 */
    if (numKeys == 1 || numKeys == 2) {
        /* key3 == key 1 */
        for (k=16; k<24; k++) {
            /* key3[i+1] = KEY3[i] xor PT/CT[j] */
            key[k] = key[k-16];
        }
    } else {
        /* key3 independent */ 
        for (k=16; k<24; k++) {
            /* key3[i+1] = KEY3[i] xor PT/CT[j-2] */
            key[k] ^= text_2[k-16];
        }
    }
    /* set the parity bits */            
    for (k=0; k<24; k++) {
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
tdea_mct_test(int mode, unsigned char* key, unsigned int numKeys, 
              unsigned int crypt, unsigned char* inputtext, 
              unsigned int inputlength, unsigned char* iv, FILE *resp) { 

    int i, j;
    unsigned char outputtext_1[8];      /* PT/CT[j-1] */
    unsigned char outputtext_2[8];      /* PT/CT[j-2] */
    char buf[80];       /* holds one line from the input REQUEST file. */
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
    for (i=0; i<400; i++) {
        /* if i == 0 CV[0] = IV  not necessary */        
        /* record the count and key values and plainText */
        sprintf(buf, "COUNT = %d\n", i);
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
        for (j=0; j<10000; j++) {

            outputlen = 0;
            if (crypt == ENCRYPT) {
                /* inputtext == ciphertext outputtext == plaintext*/
                rv = tdea_encrypt_buf(mode, key,
                            (mode == NSS_DES_EDE3) ? NULL : iv,
                            outputtext, &outputlen, 8,
                            inputtext, 8);
            } else {
                /* inputtext == plaintext outputtext == ciphertext */
                rv = tdea_decrypt_buf(mode, key,
                            (mode == NSS_DES_EDE3) ? NULL : iv,
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
            if (j==9997) memcpy(outputtext_2, outputtext, 8);
            if (j==9998) memcpy(outputtext_1, outputtext, 8);
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
    char buf[80];    /* holds one line from the input REQUEST file. */
    FILE *req;       /* input stream from the REQUEST file */
    FILE *resp;      /* output stream to the RESPONSE file */
    unsigned int crypt = 0;    /* 1 means encrypt, 0 means decrypt */
    unsigned char key[24];              /* TDEA 3 key bundle */
    unsigned int numKeys = 0;
    unsigned char plaintext[8];        /* PT[j] */
    unsigned char ciphertext[8];       /* CT[j] */
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
            for (j=0; isxdigit(buf[i]); i+=2,j++) {
                hex_from_2char(&buf[i], &key[j]);
            }
            continue;
        }
        /* KEY2 = ... */
        if (strncmp(buf, "KEY2", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j=8; isxdigit(buf[i]); i+=2,j++) {
                hex_from_2char(&buf[i], &key[j]);
            }
            continue;
        }
        /* KEY3 = ... */
        if (strncmp(buf, "KEY3", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j=16; isxdigit(buf[i]); i+=2,j++) {
                hex_from_2char(&buf[i], &key[j]);
            }
            continue;
        }

        /* IV = ... */
        if (strncmp(buf, "IV", 2) == 0) {
            i = 2;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j=0; j<sizeof iv; i+=2,j++) {
                hex_from_2char(&buf[i], &iv[j]);
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
            for (j=0; j<sizeof plaintext; i+=2,j++) {
                hex_from_2char(&buf[i], &plaintext[j]);
            }                                     

            /* do the Monte Carlo test */
            if (mode==NSS_DES_EDE3) {
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
            for (j=0; isxdigit(buf[i]); i+=2,j++) {
                hex_from_2char(&buf[i], &ciphertext[j]);
            }
            
            /* do the Monte Carlo test */
            if (mode==NSS_DES_EDE3) {
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
    unsigned char doublecheck[10*16];  /* 1 to 10 blocks */
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
    unsigned char doublecheck[10*16];  /* 1 to 10 blocks */
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
    char buf[512];      /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "CIPHERTEXT = <320 hex digits>\n".
                         */
    FILE *aesreq;       /* input stream from the REQUEST file */
    FILE *aesresp;      /* output stream to the RESPONSE file */
    int i, j;
    int mode;           /* NSS_AES (ECB) or NSS_AES_CBC */
    int encrypt = 0;    /* 1 means encrypt, 0 means decrypt */
    unsigned char key[32];              /* 128, 192, or 256 bits */
    unsigned int keysize;
    unsigned char iv[16];		/* for all modes except ECB */
    unsigned char plaintext[10*16];     /* 1 to 10 blocks */
    unsigned int plaintextlen;
    unsigned char ciphertext[10*16];    /* 1 to 10 blocks */
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &key[j]);
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
	    for (j=0; j<sizeof iv; i+=2,j++) {
		hex_from_2char(&buf[i], &iv[j]);
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &plaintext[j]);
	    }
	    plaintextlen = j;

	    rv = aes_encrypt_buf(mode, key, keysize,
		(mode == NSS_AES) ? NULL : iv,
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &ciphertext[j]);
	    }
	    ciphertextlen = j;

	    rv = aes_decrypt_buf(mode, key, keysize,
		(mode == NSS_AES) ? NULL : iv,
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
    case 16:  /* 128-bit key */
	/* Key[i+1] = Key[i] xor CT[j] */
	for (k=0; k<16; k++) {
	    key[k] ^= ciphertext[k];
	}
	break;
    case 24:  /* 192-bit key */
	/*
	 * Key[i+1] = Key[i] xor (last 64-bits of
	 *            CT[j-1] || CT[j])
	 */
	for (k=0; k<8; k++) {
	    key[k] ^= ciphertext_1[k+8];
	}
	for (k=8; k<24; k++) {
	    key[k] ^= ciphertext[k-8];
	}
	break;
    case 32:  /* 256-bit key */
	/* Key[i+1] = Key[i] xor (CT[j-1] || CT[j]) */
	for (k=0; k<16; k++) {
	    key[k] ^= ciphertext_1[k];
	}
	for (k=16; k<32; k++) {
	    key[k] ^= ciphertext[k-16];
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
    char buf[80];       /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "KEY = <64 hex digits>\n".
                         */
    FILE *aesreq;       /* input stream from the REQUEST file */
    FILE *aesresp;      /* output stream to the RESPONSE file */
    int i, j;
    int encrypt = 0;    /* 1 means encrypt, 0 means decrypt */
    unsigned char key[32];              /* 128, 192, or 256 bits */
    unsigned int keysize;
    unsigned char plaintext[16];        /* PT[j] */
    unsigned char plaintext_1[16];      /* PT[j-1] */
    unsigned char ciphertext[16];       /* CT[j] */
    unsigned char ciphertext_1[16];     /* CT[j-1] */
    unsigned char doublecheck[16];
    unsigned int outputlen;
    AESContext *cx = NULL;	/* the operation being tested */
    AESContext *cx2 = NULL;     /* the inverse operation done in parallel
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &key[j]);
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
	    for (j=0; j<sizeof plaintext; i+=2,j++) {
		hex_from_2char(&buf[i], &plaintext[j]);
	    }

	    for (i=0; i<100; i++) {
		sprintf(buf, "COUNT = %d\n", i);
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
		for (j=0; j<1000; j++) {
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &ciphertext[j]);
	    }

	    for (i=0; i<100; i++) {
		sprintf(buf, "COUNT = %d\n", i);
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
		for (j=0; j<1000; j++) {
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
    char buf[80];       /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "KEY = <64 hex digits>\n".
                         */
    FILE *aesreq;       /* input stream from the REQUEST file */
    FILE *aesresp;      /* output stream to the RESPONSE file */
    int i, j;
    int encrypt = 0;    /* 1 means encrypt, 0 means decrypt */
    unsigned char key[32];              /* 128, 192, or 256 bits */
    unsigned int keysize;
    unsigned char iv[16];
    unsigned char plaintext[16];        /* PT[j] */
    unsigned char plaintext_1[16];      /* PT[j-1] */
    unsigned char ciphertext[16];       /* CT[j] */
    unsigned char ciphertext_1[16];     /* CT[j-1] */
    unsigned char doublecheck[16];
    unsigned int outputlen;
    AESContext *cx = NULL;	/* the operation being tested */
    AESContext *cx2 = NULL;     /* the inverse operation done in parallel
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &key[j]);
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
	    for (j=0; j<sizeof iv; i+=2,j++) {
		hex_from_2char(&buf[i], &iv[j]);
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
	    for (j=0; j<sizeof plaintext; i+=2,j++) {
		hex_from_2char(&buf[i], &plaintext[j]);
	    }

	    for (i=0; i<100; i++) {
		sprintf(buf, "COUNT = %d\n", i);
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
		for (j=0; j<1000; j++) {
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &ciphertext[j]);
	    }

	    for (i=0; i<100; i++) {
		sprintf(buf, "COUNT = %d\n", i);
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
		for (j=0; j<1000; j++) {
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

void write_compact_string(FILE *out, unsigned char *hash, unsigned int len)
{
    unsigned int i;
    int j, count = 0, last = -1, z = 0;
    long start = ftell(out);
    for (i=0; i<len; i++) {
	for (j=7; j>=0; j--) {
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

int get_next_line(FILE *req, char *key, char *val, FILE *rsp)
{
    int ignore = 0;
    char *writeto = key;
    int w = 0;
    int c;
    while ((c = fgetc(req)) != EOF) {
	if (ignore) {
	    fprintf(rsp, "%c", c);
	    if (c == '\n') return ignore;
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

void
dss_test(char *reqdir, char *rspdir)
{
    char filename[128];
    char key[24], val[1024];
    FILE *req, *rsp;
    unsigned int mod;
    SECItem digest = { 0 }, sig = { 0 };
    DSAPublicKey pubkey = { 0 };
    DSAPrivateKey privkey = { 0 };
    PQGParams params;
    PQGVerify verify;
    unsigned int i;
    int j, rv;
    goto do_pqggen;
#if 0
    /* primality test */
do_prime:
    sprintf(filename, "%s/prime.req", reqdir);
    req = fopen(filename, "r");
    sprintf(filename, "%s/prime.rsp", rspdir);
    rsp = fopen(filename, "w");
    while ((rv = get_next_line(req, key, val, rsp)) >= 0) {
	if (rv == 0) {
	    if (strcmp(key, "mod") == 0) {
		mod = atoi(val);
		fprintf(rsp, "[mod=%d]\n", mod);
	    } else if (strcmp(key, "Prime") == 0) {
		unsigned char octets[128];
		mp_int mp;
		fprintf(rsp, "Prime= %s\n", val);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, octets + i);
		}
		mp_init(&mp);
		mp_read_unsigned_octets(&mp, octets, i);
		if (mpp_pprime(&mp, 50) == MP_YES) {
		    fprintf(rsp, "result= P\n");
		} else {
		    fprintf(rsp, "result= F\n");
		}
	    }
	}
    }
    fclose(req);
    fclose(rsp);
#endif
do_pqggen:
    /* PQG Gen */
    sprintf(filename, "%s/pqg.req", reqdir);
    req = fopen(filename, "r");
    sprintf(filename, "%s/pqg.rsp", rspdir);
    rsp = fopen(filename, "w");
    while ((rv = get_next_line(req, key, val, rsp)) >= 0) {
	if (rv == 0) {
	    if (strcmp(key, "mod") == 0) {
		mod = atoi(val);
		fprintf(rsp, "[mod=%d]\n", mod);
	    } else if (strcmp(key, "N") == 0) {
		char str[264];
		unsigned int jj;
		int N = atoi(val);
		for (i=0; i<N; i++) {
		    PQGParams *pqg;
		    PQGVerify *vfy;
		    PQG_ParamGenSeedLen(PQG_PBITS_TO_INDEX(mod), 20, &pqg, &vfy);
#if 0
		    if (!(vfy->seed.data[0] & 0x80)) {
			to_hex_str(str, vfy->seed.data, vfy->seed.len);
			fprintf(stderr, "rejected %s\n", str);
			--i;
			continue;
		    }
#endif
		    to_hex_str(str, pqg->prime.data, pqg->prime.len);
		    fprintf(rsp, "P= %s\n", str);
		    to_hex_str(str, pqg->subPrime.data, pqg->subPrime.len);
		    fprintf(rsp, "Q= %s\n", str);
		    to_hex_str(str, pqg->base.data, pqg->base.len);
		    fprintf(rsp, "G= %s\n", str);
		    to_hex_str(str, vfy->seed.data, vfy->seed.len);
		    fprintf(rsp, "Seed= %s\n", str);
		    to_hex_str(str, vfy->h.data, vfy->h.len);
		    fprintf(rsp, "H= ");
		    for (jj=vfy->h.len; jj<pqg->prime.len; jj++) {
			fprintf(rsp, "00");
		    }
		    fprintf(rsp, "%s\n", str);
		    fprintf(rsp, "c= %d\n", vfy->counter);
		}
	    }
	}
    }
    fclose(req);
    fclose(rsp);
    return;
do_pqgver:
    /* PQG Verification */
    sprintf(filename, "%s/verpqg.req", reqdir);
    req = fopen(filename, "r");
    sprintf(filename, "%s/verpqg.rsp", rspdir);
    rsp = fopen(filename, "w");
    memset(&params, 0, sizeof(params));
    memset(&verify, 0, sizeof(verify));
    while ((rv = get_next_line(req, key, val, rsp)) >= 0) {
	if (rv == 0) {
	    if (strcmp(key, "mod") == 0) {
		mod = atoi(val);
		fprintf(rsp, "[mod=%d]\n", mod);
	    } else if (strcmp(key, "P") == 0) {
		if (params.prime.data) {
		    SECITEM_ZfreeItem(&params.prime, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &params.prime, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, params.prime.data + i);
		}
		fprintf(rsp, "P= %s\n", val);
	    } else if (strcmp(key, "Q") == 0) {
		if (params.subPrime.data) {
		    SECITEM_ZfreeItem(&params.subPrime, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &params.subPrime,strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, params.subPrime.data + i);
		}
		fprintf(rsp, "Q= %s\n", val);
	    } else if (strcmp(key, "G") == 0) {
		if (params.base.data) {
		    SECITEM_ZfreeItem(&params.base, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &params.base, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, params.base.data + i);
		}
		fprintf(rsp, "G= %s\n", val);
	    } else if (strcmp(key, "Seed") == 0) {
		if (verify.seed.data) {
		    SECITEM_ZfreeItem(&verify.seed, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &verify.seed, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, verify.seed.data + i);
		}
		fprintf(rsp, "Seed= %s\n", val);
	    } else if (strcmp(key, "H") == 0) {
		if (verify.h.data) {
		    SECITEM_ZfreeItem(&verify.h, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &verify.h, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, verify.h.data + i);
		}
		fprintf(rsp, "H= %s\n", val);
	    } else if (strcmp(key, "c") == 0) {
		SECStatus pqgrv, result;
		verify.counter = atoi(val);
		fprintf(rsp, "c= %d\n", verify.counter);
		pqgrv = PQG_VerifyParams(&params, &verify, &result);
		if (result == SECSuccess) {
		    fprintf(rsp, "result= P\n");
		} else {
		    fprintf(rsp, "result= F\n");
		}
	    }
	}
    }
    fclose(req);
    fclose(rsp);
    return;
do_keygen:
    /* Key Gen */
    sprintf(filename, "%s/xy.req", reqdir);
    req = fopen(filename, "r");
    sprintf(filename, "%s/xy.rsp", rspdir);
    rsp = fopen(filename, "w");
    while ((rv = get_next_line(req, key, val, rsp)) >= 0);
    for (j=0; j<=8; j++) {
	char str[264];
	PQGParams *pqg;
	PQGVerify *vfy;
	fprintf(rsp, "[mod=%d]\n", 512 + j*64);
	PQG_ParamGen(j, &pqg, &vfy);
	to_hex_str(str, pqg->prime.data, pqg->prime.len);
	fprintf(rsp, "P= %s\n", str);
	to_hex_str(str, pqg->subPrime.data, pqg->subPrime.len);
	fprintf(rsp, "Q= %s\n", str);
	to_hex_str(str, pqg->base.data, pqg->base.len);
	fprintf(rsp, "G= %s\n", str);
	for (i=0; i<10; i++) {
	    DSAPrivateKey *dsakey;
	    DSA_NewKey(pqg, &dsakey);
	    to_hex_str(str, dsakey->privateValue.data,dsakey->privateValue.len);
	    fprintf(rsp, "X= %s\n", str);
	    to_hex_str(str, dsakey->publicValue.data, dsakey->publicValue.len);
	    fprintf(rsp, "Y= %s\n", str);
	    PORT_FreeArena(dsakey->params.arena, PR_TRUE);
	    dsakey = NULL;
	}
    }
    fclose(req);
    fclose(rsp);
    return;
do_siggen:
    /* Signature Gen */
    sprintf(filename, "%s/gensig.req", reqdir);
    req = fopen(filename, "r");
    sprintf(filename, "%s/gensig.rsp", rspdir);
    rsp = fopen(filename, "w");
    while ((rv = get_next_line(req, key, val, rsp)) >= 0) {
	if (rv == 0) {
	    if (strcmp(key, "mod") == 0) {
		mod = atoi(val);
		fprintf(rsp, "[mod=%d]\n", mod);
	    } else if (strcmp(key, "P") == 0) {
		if (privkey.params.prime.data) {
		    SECITEM_ZfreeItem(&privkey.params.prime, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &privkey.params.prime, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, privkey.params.prime.data + i);
		}
		fprintf(rsp, "P= %s\n", val);
	    } else if (strcmp(key, "Q") == 0) {
		if (privkey.params.subPrime.data) {
		    SECITEM_ZfreeItem(&privkey.params.subPrime, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &privkey.params.subPrime,strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, privkey.params.subPrime.data + i);
		}
		fprintf(rsp, "Q= %s\n", val);
	    } else if (strcmp(key, "G") == 0) {
		if (privkey.params.base.data) {
		    SECITEM_ZfreeItem(&privkey.params.base, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &privkey.params.base, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, privkey.params.base.data + i);
		}
		fprintf(rsp, "G= %s\n", val);
	    } else if (strcmp(key, "X") == 0) {
		if (privkey.privateValue.data) {
		    SECITEM_ZfreeItem(&privkey.privateValue, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &privkey.privateValue, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, privkey.privateValue.data + i);
		}
		fprintf(rsp, "X= %s\n", val);
	    } else if (strcmp(key, "Msg") == 0) {
		char msg[512];
		char str[81];
		if (digest.data) {
		    SECITEM_ZfreeItem(&digest, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &digest, 20);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, msg + i);
		}
		msg[i] = '\0';
		/*SHA1_Hash(digest.data, msg);*/
		SHA1_HashBuf(digest.data, msg, i);
		fprintf(rsp, "Msg= %s\n", val);
		if (sig.data) {
		    SECITEM_ZfreeItem(&sig, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &sig, 40);
		rv = DSA_SignDigest(&privkey, &sig, &digest);
		to_hex_str(str, sig.data, sig.len);
		fprintf(rsp, "Sig= %s\n", str);
	    }
	}
    }
    fclose(req);
    fclose(rsp);
do_sigver:
    /* Signature Verification */
    sprintf(filename, "%s/versig.req", reqdir);
    req = fopen(filename, "r");
    sprintf(filename, "%s/versig.rsp", rspdir);
    rsp = fopen(filename, "w");
    while ((rv = get_next_line(req, key, val, rsp)) >= 0) {
	if (rv == 0) {
	    if (strcmp(key, "mod") == 0) {
		mod = atoi(val);
		fprintf(rsp, "[mod=%d]\n", mod);
	    } else if (strcmp(key, "P") == 0) {
		if (pubkey.params.prime.data) {
		    SECITEM_ZfreeItem(&pubkey.params.prime, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &pubkey.params.prime, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, pubkey.params.prime.data + i);
		}
		fprintf(rsp, "P= %s\n", val);
	    } else if (strcmp(key, "Q") == 0) {
		if (pubkey.params.subPrime.data) {
		    SECITEM_ZfreeItem(&pubkey.params.subPrime, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &pubkey.params.subPrime, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, pubkey.params.subPrime.data + i);
		}
		fprintf(rsp, "Q= %s\n", val);
	    } else if (strcmp(key, "G") == 0) {
		if (pubkey.params.base.data) {
		    SECITEM_ZfreeItem(&pubkey.params.base, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &pubkey.params.base, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, pubkey.params.base.data + i);
		}
		fprintf(rsp, "G= %s\n", val);
	    } else if (strcmp(key, "Y") == 0) {
		if (pubkey.publicValue.data) {
		    SECITEM_ZfreeItem(&pubkey.publicValue, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &pubkey.publicValue, strlen(val)/2);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, pubkey.publicValue.data + i);
		}
		fprintf(rsp, "Y= %s\n", val);
	    } else if (strcmp(key, "Msg") == 0) {
		char msg[512];
		if (digest.data) {
		    SECITEM_ZfreeItem(&digest, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &digest, 20);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, msg + i);
		}
		msg[i] = '\0';
		SHA1_HashBuf(digest.data, msg, i);
		/*SHA1_Hash(digest.data, msg);*/
		fprintf(rsp, "Msg= %s\n", val);
	    } else if (strcmp(key, "Sig") == 0) {
		SECStatus rv;
		if (sig.data) {
		    SECITEM_ZfreeItem(&sig, PR_FALSE);
		}
		SECITEM_AllocItem(NULL, &sig, 40);
		for (i=0; i<strlen(val) / 2; i++) {
		    hex_from_2char(val + 2*i, sig.data + i);
		}
		rv = DSA_VerifyDigest(&pubkey, &sig, &digest);
		fprintf(rsp, "Sig= %s\n", val);
		if (rv == SECSuccess) {
		    fprintf(rsp, "result= P\n");
		} else {
		    fprintf(rsp, "result= F\n");
		}
	    }
	}
    }
    fclose(req);
    fclose(rsp);
}

#ifdef NSS_ENABLE_ECC
typedef struct curveNameTagPairStr {
    char *curveName;
    SECOidTag curveOidTag;
} CurveNameTagPair;

#define DEFAULT_CURVE_OID_TAG  SEC_OID_SECG_EC_SECP192R1
/* #define DEFAULT_CURVE_OID_TAG  SEC_OID_SECG_EC_SECP160R1 */

static CurveNameTagPair nameTagPair[] =
{ 
  { "sect163k1", SEC_OID_SECG_EC_SECT163K1},
  { "nistk163", SEC_OID_SECG_EC_SECT163K1},
  { "sect163r1", SEC_OID_SECG_EC_SECT163R1},
  { "sect163r2", SEC_OID_SECG_EC_SECT163R2},
  { "nistb163", SEC_OID_SECG_EC_SECT163R2},
  { "sect193r1", SEC_OID_SECG_EC_SECT193R1},
  { "sect193r2", SEC_OID_SECG_EC_SECT193R2},
  { "sect233k1", SEC_OID_SECG_EC_SECT233K1},
  { "nistk233", SEC_OID_SECG_EC_SECT233K1},
  { "sect233r1", SEC_OID_SECG_EC_SECT233R1},
  { "nistb233", SEC_OID_SECG_EC_SECT233R1},
  { "sect239k1", SEC_OID_SECG_EC_SECT239K1},
  { "sect283k1", SEC_OID_SECG_EC_SECT283K1},
  { "nistk283", SEC_OID_SECG_EC_SECT283K1},
  { "sect283r1", SEC_OID_SECG_EC_SECT283R1},
  { "nistb283", SEC_OID_SECG_EC_SECT283R1},
  { "sect409k1", SEC_OID_SECG_EC_SECT409K1},
  { "nistk409", SEC_OID_SECG_EC_SECT409K1},
  { "sect409r1", SEC_OID_SECG_EC_SECT409R1},
  { "nistb409", SEC_OID_SECG_EC_SECT409R1},
  { "sect571k1", SEC_OID_SECG_EC_SECT571K1},
  { "nistk571", SEC_OID_SECG_EC_SECT571K1},
  { "sect571r1", SEC_OID_SECG_EC_SECT571R1},
  { "nistb571", SEC_OID_SECG_EC_SECT571R1},
  { "secp160k1", SEC_OID_SECG_EC_SECP160K1},
  { "secp160r1", SEC_OID_SECG_EC_SECP160R1},
  { "secp160r2", SEC_OID_SECG_EC_SECP160R2},
  { "secp192k1", SEC_OID_SECG_EC_SECP192K1},
  { "secp192r1", SEC_OID_SECG_EC_SECP192R1},
  { "nistp192", SEC_OID_SECG_EC_SECP192R1},
  { "secp224k1", SEC_OID_SECG_EC_SECP224K1},
  { "secp224r1", SEC_OID_SECG_EC_SECP224R1},
  { "nistp224", SEC_OID_SECG_EC_SECP224R1},
  { "secp256k1", SEC_OID_SECG_EC_SECP256K1},
  { "secp256r1", SEC_OID_SECG_EC_SECP256R1},
  { "nistp256", SEC_OID_SECG_EC_SECP256R1},
  { "secp384r1", SEC_OID_SECG_EC_SECP384R1},
  { "nistp384", SEC_OID_SECG_EC_SECP384R1},
  { "secp521r1", SEC_OID_SECG_EC_SECP521R1},
  { "nistp521", SEC_OID_SECG_EC_SECP521R1},

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

  { "secp112r1", SEC_OID_SECG_EC_SECP112R1},
  { "secp112r2", SEC_OID_SECG_EC_SECP112R2},
  { "secp128r1", SEC_OID_SECG_EC_SECP128R1},
  { "secp128r2", SEC_OID_SECG_EC_SECP128R2},

  { "sect113r1", SEC_OID_SECG_EC_SECT113R1},
  { "sect113r2", SEC_OID_SECG_EC_SECT113R2},
  { "sect131r1", SEC_OID_SECG_EC_SECT131R1},
  { "sect131r2", SEC_OID_SECG_EC_SECT131R2},
};

static SECKEYECParams * 
getECParams(const char *curve)
{
    SECKEYECParams *ecparams;
    SECOidData *oidData = NULL;
    SECOidTag curveOidTag = SEC_OID_UNKNOWN; /* default */
    int i, numCurves;

    if (curve != NULL) {
        numCurves = sizeof(nameTagPair)/sizeof(CurveNameTagPair);
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
 * Perform the ECDSA Key Pair Generation Test.
 *
 * reqfn is the pathname of the REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void
ecdsa_keypair_test(char *reqfn)
{
    char buf[256];      /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * needs to be large enough to hold the longest
                         * line "Qx = <144 hex digits>\n".
                         */
    FILE *ecdsareq;     /* input stream from the REQUEST file */
    FILE *ecdsaresp;    /* output stream to the RESPONSE file */
    char curve[16];     /* "nistxddd" */
    ECParams *ecparams;
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
	    SECKEYECParams *encodedparams;

	    src = &buf[1];
	    dst = &curve[4];
	    *dst++ = tolower(*src);
	    src += 2;  /* skip the hyphen */
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst = '\0';
	    encodedparams = getECParams(curve);
	    if (encodedparams == NULL) {
		goto loser;
	    }
	    if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
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
		if (EC_ValidatePublicKey(ecparams, &ecpriv->publicValue)
		    != SECSuccess) {
		    goto loser;
		}
		len = ecpriv->publicValue.len;
		if (len%2 == 0) {
		    goto loser;
		}
		len = (len-1)/2;
		if (ecpriv->publicValue.data[0]
		    != EC_POINT_FORM_UNCOMPRESSED) {
		    goto loser;
		}
		fputs("Qx = ", ecdsaresp);
		to_hex_str(buf, &ecpriv->publicValue.data[1], len);
		fputs(buf, ecdsaresp);
		fputc('\n', ecdsaresp);
		fputs("Qy = ", ecdsaresp);
		to_hex_str(buf, &ecpriv->publicValue.data[1+len], len);
		fputs(buf, ecdsaresp);
		fputc('\n', ecdsaresp);
		fputc('\n', ecdsaresp);
		PORT_FreeArena(ecpriv->ecParams.arena, PR_TRUE);
	    }
	    PORT_FreeArena(ecparams->arena, PR_TRUE);
	    continue;
	}
    }
loser:
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
    char buf[256];      /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "Qx = <144 hex digits>\n".
                         */
    FILE *ecdsareq;     /* input stream from the REQUEST file */
    FILE *ecdsaresp;    /* output stream to the RESPONSE file */
    char curve[16];     /* "nistxddd" */
    ECParams *ecparams = NULL;
    SECItem pubkey;
    unsigned int i;
    unsigned int len;
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
	    SECKEYECParams *encodedparams;

	    src = &buf[1];
	    dst = &curve[4];
	    *dst++ = tolower(*src);
	    src += 2;  /* skip the hyphen */
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst = '\0';
	    if (ecparams != NULL) {
		PORT_FreeArena(ecparams->arena, PR_TRUE);
		ecparams = NULL;
	    }
	    encodedparams = getECParams(curve);
	    if (encodedparams == NULL) {
		goto loser;
	    }
	    if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
		goto loser;
	    }
	    SECITEM_FreeItem(encodedparams, PR_TRUE);
	    len = (ecparams->fieldID.size + 7) >> 3;
	    if (pubkey.data != NULL) {
		PORT_Free(pubkey.data);
		pubkey.data = NULL;
	    }
	    SECITEM_AllocItem(NULL, &pubkey, 2*len+1);
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
	    keyvalid = from_hex_str(&pubkey.data[1+len], len, &buf[i]);
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
	PORT_FreeArena(ecparams->arena, PR_TRUE);
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
    char buf[1024];     /* holds one line from the input REQUEST file
                         * or to the output RESPONSE file.
                         * needs to be large enough to hold the longest
                         * line "Msg = <256 hex digits>\n".
                         */
    FILE *ecdsareq;     /* input stream from the REQUEST file */
    FILE *ecdsaresp;    /* output stream to the RESPONSE file */
    char curve[16];     /* "nistxddd" */
    ECParams *ecparams = NULL;
    int i, j;
    unsigned int len;
    unsigned char msg[512];  /* message to be signed (<= 128 bytes) */
    unsigned int msglen;
    unsigned char sha1[20];  /* SHA-1 hash (160 bits) */
    unsigned char sig[2*MAX_ECKEY_LEN];
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
	    SECKEYECParams *encodedparams;

	    src = &buf[1];
	    dst = &curve[4];
	    *dst++ = tolower(*src);
	    src += 2;  /* skip the hyphen */
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst = '\0';
	    if (ecparams != NULL) {
		PORT_FreeArena(ecparams->arena, PR_TRUE);
		ecparams = NULL;
	    }
	    encodedparams = getECParams(curve);
	    if (encodedparams == NULL) {
		goto loser;
	    }
	    if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &msg[j]);
	    }
	    msglen = j;
	    if (SHA1_HashBuf(sha1, msg, msglen) != SECSuccess) {
		goto loser;
	    }
	    fputs(buf, ecdsaresp);

	    if (EC_NewKey(ecparams, &ecpriv) != SECSuccess) {
		goto loser;
	    }
	    if (EC_ValidatePublicKey(ecparams, &ecpriv->publicValue)
		!= SECSuccess) {
		goto loser;
	    }
	    len = ecpriv->publicValue.len;
	    if (len%2 == 0) {
		goto loser;
	    }
	    len = (len-1)/2;
	    if (ecpriv->publicValue.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
		goto loser;
	    }
	    fputs("Qx = ", ecdsaresp);
	    to_hex_str(buf, &ecpriv->publicValue.data[1], len);
	    fputs(buf, ecdsaresp);
	    fputc('\n', ecdsaresp);
	    fputs("Qy = ", ecdsaresp);
	    to_hex_str(buf, &ecpriv->publicValue.data[1+len], len);
	    fputs(buf, ecdsaresp);
	    fputc('\n', ecdsaresp);

	    digest.type = siBuffer;
	    digest.data = sha1;
	    digest.len = sizeof sha1;
	    signature.type = siBuffer;
	    signature.data = sig;
	    signature.len = sizeof sig;
	    if (ECDSA_SignDigest(ecpriv, &signature, &digest) != SECSuccess) {
		goto loser;
	    }
	    len = signature.len;
	    if (len%2 != 0) {
		goto loser;
	    }
	    len = len/2;
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
	PORT_FreeArena(ecparams->arena, PR_TRUE);
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
    char buf[1024];     /* holds one line from the input REQUEST file.
                         * needs to be large enough to hold the longest
                         * line "Msg = <256 hex digits>\n".
                         */
    FILE *ecdsareq;     /* input stream from the REQUEST file */
    FILE *ecdsaresp;    /* output stream to the RESPONSE file */
    char curve[16];     /* "nistxddd" */
    ECParams *ecparams = NULL;
    ECPublicKey ecpub;
    unsigned int i, j;
    unsigned int flen;  /* length in bytes of the field size */
    unsigned int olen;  /* length in bytes of the base point order */
    unsigned char msg[512];  /* message that was signed (<= 128 bytes) */
    unsigned int msglen;
    unsigned char sha1[20];  /* SHA-1 hash (160 bits) */
    unsigned char sig[2*MAX_ECKEY_LEN];
    SECItem signature, digest;
    PRBool keyvalid = PR_TRUE;
    PRBool sigvalid = PR_TRUE;

    ecdsareq = fopen(reqfn, "r");
    ecdsaresp = stdout;
    ecpub.publicValue.type = siBuffer;
    ecpub.publicValue.data = NULL;
    ecpub.publicValue.len = 0;
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
	    SECKEYECParams *encodedparams;

	    src = &buf[1];
	    dst = &curve[4];
	    *dst++ = tolower(*src);
	    src += 2;  /* skip the hyphen */
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst++ = *src++;
	    *dst = '\0';
	    if (ecparams != NULL) {
		PORT_FreeArena(ecparams->arena, PR_TRUE);
		ecparams = NULL;
	    }
	    encodedparams = getECParams(curve);
	    if (encodedparams == NULL) {
		goto loser;
	    }
	    if (EC_DecodeParams(encodedparams, &ecparams) != SECSuccess) {
		goto loser;
	    }
	    SECITEM_FreeItem(encodedparams, PR_TRUE);
	    ecpub.ecParams = *ecparams;
	    flen = (ecparams->fieldID.size + 7) >> 3;
	    olen = ecparams->order.len;
	    if (2*olen > sizeof sig) {
		goto loser;
	    }
	    if (ecpub.publicValue.data != NULL) {
		SECITEM_FreeItem(&ecpub.publicValue, PR_FALSE);
	    }
	    SECITEM_AllocItem(NULL, &ecpub.publicValue, 2*flen+1);
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
	    for (j=0; isxdigit(buf[i]); i+=2,j++) {
		hex_from_2char(&buf[i], &msg[j]);
	    }
	    msglen = j;
	    if (SHA1_HashBuf(sha1, msg, msglen) != SECSuccess) {
		goto loser;
	    }
	    fputs(buf, ecdsaresp);

	    digest.type = siBuffer;
	    digest.data = sha1;
	    digest.len = sizeof sha1;

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
	    keyvalid = from_hex_str(&ecpub.publicValue.data[1+flen], flen,
				    &buf[i]);
	    if (!keyvalid) {
		continue;
	    }
	    if (EC_ValidatePublicKey(ecparams, &ecpub.publicValue)
		!= SECSuccess) {
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
	    signature.len = 2*olen;

	    if (!keyvalid || !sigvalid) {
		fputs("Result = F\n", ecdsaresp);
	    } else if (ECDSA_VerifyDigest(&ecpub, &signature, &digest)
		== SECSuccess) {
		fputs("Result = P\n", ecdsaresp);
	    } else {
		fputs("Result = F\n", ecdsaresp);
	    }
	    continue;
	}
    }
loser:
    if (ecparams != NULL) {
	PORT_FreeArena(ecparams->arena, PR_TRUE);
    }
    if (ecpub.publicValue.data != NULL) {
	SECITEM_FreeItem(&ecpub.publicValue, PR_FALSE);
    }
    fclose(ecdsareq);
}
#endif /* NSS_ENABLE_ECC */

void do_random()
{
    int i, j, k = 0;
    unsigned char buf[500];
    for (i=0; i<5; i++) {
	RNG_GenerateGlobalRandomBytes(buf, sizeof buf);
	for (j=0; j<sizeof buf / 2; j++) {
	    printf("0x%02x%02x", buf[2*j], buf[2*j+1]);
	    if (++k % 8 == 0) printf("\n"); else printf(" ");
	}
    }
}

/*
 * Calculate the SHA Message Digest 
 *
 * MD = Message digest 
 * MDLen = length of Message Digest and SHA_Type
 * msg = message to digest 
 * msgLen = length of message to digest
 */
SECStatus sha_calcMD(unsigned char *MD, unsigned int MDLen, unsigned char *msg, unsigned int msgLen) 
{    
    SECStatus   sha_status = SECFailure;

    if (MDLen == SHA1_LENGTH) {
        sha_status = SHA1_HashBuf(MD, msg, msgLen);
    } else if (MDLen == SHA256_LENGTH) {
        sha_status = SHA256_HashBuf(MD, msg, msgLen);
    } else if (MDLen == SHA384_LENGTH) {
        sha_status = SHA384_HashBuf(MD, msg, msgLen);
    } else if (MDLen == SHA512_LENGTH) {
        sha_status = SHA512_HashBuf(MD, msg, msgLen);
    }

    return sha_status;
}

/*
 * Perform the SHA Monte Carlo Test
 *
 * MDLen = length of Message Digest and SHA_Type
 * seed = input seed value
 * resp = is the output response file. 
 */
SECStatus sha_mct_test(unsigned int MDLen, unsigned char *seed, FILE *resp) 
{
    int i, j;
    unsigned int msgLen = MDLen*3;
    unsigned char MD_i3[HASH_LENGTH_MAX];  /* MD[i-3] */
    unsigned char MD_i2[HASH_LENGTH_MAX];  /* MD[i-2] */
    unsigned char MD_i1[HASH_LENGTH_MAX];  /* MD[i-1] */
    unsigned char MD_i[HASH_LENGTH_MAX];   /* MD[i] */
    unsigned char msg[HASH_LENGTH_MAX*3];
    char buf[HASH_LENGTH_MAX*2 + 1];  /* MAX buf MD_i as a hex string */

    for (j=0; j<100; j++) {
        /* MD_0 = MD_1 = MD_2 = seed */
        memcpy(MD_i3, seed, MDLen);
        memcpy(MD_i2, seed, MDLen);
        memcpy(MD_i1, seed, MDLen);

        for (i=3; i < 1003; i++) {
            /* Mi = MD[i-3] || MD [i-2] || MD [i-1] */
            memcpy(msg, MD_i3, MDLen);
            memcpy(&msg[MDLen], MD_i2, MDLen);
            memcpy(&msg[MDLen*2], MD_i1,MDLen); 

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

        sprintf(buf, "COUNT = %d\n", j);
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
void sha_test(char *reqfn) 
{
    int i, j;
    unsigned int MDlen;   /* the length of the Message Digest in Bytes  */
    unsigned int msgLen;  /* the length of the input Message in Bytes */
    char *msg = NULL;      /* holds the message to digest.*/
    size_t bufSize = 25608; /*MAX buffer size */
    char *buf = NULL;      /* holds one line from the input REQUEST file.*/
    unsigned char seed[HASH_LENGTH_MAX];   /* max size of seed 64 bytes */
    unsigned char MD[HASH_LENGTH_MAX];     /* message digest */

    FILE *req;       /* input stream from the REQUEST file */
    FILE *resp;      /* output stream to the RESPONSE file */

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
                PORT_ZFree(msg,msgLen);
                msg = NULL;
            }
            msgLen = atoi(&buf[i]); /* in bits */
            msgLen = msgLen/8; /* convert to bytes */
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
            for (j=0; j< msgLen; i+=2,j++) {
                hex_from_2char(&buf[i], &msg[j]);
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
            for (j=0; j<sizeof seed; i+=2,j++) {
                hex_from_2char(&buf[i], &seed[j]);
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
    fclose(req);
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
          const char *secret_key,
          const unsigned int secret_key_length,
          const char *message,
          const unsigned int message_length,
          const HASH_HashType hashAlg )
{
    SECStatus hmac_status = SECFailure;
    HMACContext *cx = NULL;
    SECHashObject *hashObj = NULL;
    unsigned int bytes_hashed = 0;

    hashObj = (SECHashObject *) HASH_GetRawHashObject(hashAlg);
 
    if (!hashObj) 
        return( SECFailure );

    cx = HMAC_Create(hashObj, secret_key, 
                     secret_key_length, 
                     PR_TRUE);  /* PR_TRUE for in FIPS mode */

    if (cx == NULL) 
        return( SECFailure );

    HMAC_Begin(cx);
    HMAC_Update(cx, message, message_length);
    hmac_status = HMAC_Finish(cx, hmac_computed, &bytes_hashed, 
                              hmac_length);

    HMAC_Destroy(cx, PR_TRUE);

    return( hmac_status );
}

/*
 * Perform the HMAC Tests.
 *
 * reqfn is the pathname of the input REQUEST file.
 *
 * The output RESPONSE file is written to stdout.
 */
void hmac_test(char *reqfn) 
{
    int i, j;
    size_t bufSize =      288;    /* MAX buffer size */
    char *buf = NULL;  /* holds one line from the input REQUEST file.*/
    unsigned int keyLen;          /* Key Length */  
    char key[140];                /* key MAX size = 140 */
    unsigned int msgLen = 128;    /* the length of the input  */
                                  /*  Message is always 128 Bytes */
    char *msg = NULL;             /* holds the message to digest.*/
    unsigned int HMACLen;         /* the length of the HMAC Bytes  */
    unsigned char HMAC[HASH_LENGTH_MAX];  /* computed HMAC */
    HASH_HashType hash_alg;       /* HMAC type */

    FILE *req;       /* input stream from the REQUEST file */
    FILE *resp;      /* output stream to the RESPONSE file */

    buf = PORT_ZAlloc(bufSize);
    if (buf == NULL) {
        goto loser;
    }      
    msg = PORT_ZAlloc(msgLen);
    memset(msg, 0, msgLen);
    if (msg == NULL) {
        goto loser;
    } 

    req = fopen(reqfn, "r");
    resp = stdout;
    while (fgets(buf, bufSize, req) != NULL) {

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
                /* set the HASH algorithm for HMAC */
                if (HMACLen == SHA1_LENGTH) {
                    hash_alg = HASH_AlgSHA1;
                } else if (HMACLen == SHA256_LENGTH) {
                    hash_alg = HASH_AlgSHA256;
                } else if (HMACLen == SHA384_LENGTH) {
                    hash_alg = HASH_AlgSHA384;
                } else if (HMACLen == SHA512_LENGTH) {
                    hash_alg = HASH_AlgSHA512;
                } else {
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
            HMACLen = 0;
            memset(key, 0, sizeof key);     
            memset(msg, 0, sizeof msg);  
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
            for (j=0; j< keyLen; i+=2,j++) {
                hex_from_2char(&buf[i], &key[j]);
            }
           fputs(buf, resp);
        }
        /* TLen = Length of the calculated HMAC */
        if (strncmp(buf, "Tlen", 4) == 0) {
            i = 4;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            HMACLen = atoi(&buf[i]); /* in bytes */
            fputs(buf, resp);
            continue;
        }
        /* MSG = to HMAC always 128 bytes for these tests */
        if (strncmp(buf, "Msg", 3) == 0) {
            i = 3;
            while (isspace(buf[i]) || buf[i] == '=') {
                i++;
            }
            for (j=0; j< msgLen; i+=2,j++) {
                hex_from_2char(&buf[i], &msg[j]);
            }
           fputs(buf, resp);
           /* calculate the HMAC and output */ 
           if (hmac_calc(HMAC, HMACLen, key, keyLen,   
                         msg, msgLen, hash_alg) != SECSuccess) {
               goto loser;
           }
           fputs("MAC = ", resp);
           to_hex_str(buf, HMAC, HMACLen);
           fputs(buf, resp);
           fputc('\n', resp);
           continue;
        }
    }
loser:
    fclose(req);
    if (buf) {
        PORT_ZFree(buf, bufSize);
    }
    if (msg) {
        PORT_ZFree(msg, msgLen);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) exit (-1);
    NSS_NoDB_Init(NULL);
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
	if (       strcmp(argv[2], "kat") == 0) {
	    /* Known Answer Test (KAT) */
	    aes_kat_mmt(argv[4]);
	} else if (strcmp(argv[2], "mmt") == 0) {
	    /* Multi-block Message Test (MMT) */
	    aes_kat_mmt(argv[4]);
	} else if (strcmp(argv[2], "mct") == 0) {
	    /* Monte Carlo Test (MCT) */
	    if (       strcmp(argv[3], "ecb") == 0) {
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
    /*   HMAC    */
    /*************/
    } else if (strcmp(argv[1], "hmac") == 0) {
        hmac_test(argv[2]);
    /*************/
    /*   DSS     */
    /*************/
    } else if (strcmp(argv[1], "dss") == 0) {
	dss_test(argv[2], argv[3]);
#ifdef NSS_ENABLE_ECC
    /*************/
    /*   ECDSA   */
    /*************/
    } else if (strcmp(argv[1], "ecdsa") == 0) {
	/* argv[2]=keypair|pkv|siggen|sigver argv[3]=<test name>.req */
	if (       strcmp(argv[2], "keypair") == 0) {
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
#endif /* NSS_ENABLE_ECC */
    /*************/
    /*   RNG     */
    /*************/
    } else if (strcmp(argv[1], "rng") == 0) {
	do_random();
    }
    return 0;
}
