/*
 * alg2268.c - implementation of the algorithm in RFC 2268
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "../stubs.h"
#endif

#include "../blapi.h"
#include "../blapii.h"
#include "secerr.h"
#ifdef XP_UNIX_XXX
#include <stddef.h> /* for ptrdiff_t */
#endif

/*
** RC2 symmetric block cypher
*/

typedef SECStatus(rc2Func)(RC2Context *cx, unsigned char *output,
                           const unsigned char *input, unsigned int inputLen);

/* forward declarations */
static rc2Func rc2_EncryptECB;
static rc2Func rc2_DecryptECB;
static rc2Func rc2_EncryptCBC;
static rc2Func rc2_DecryptCBC;

typedef union {
    PRUint32 l[2];
    PRUint16 s[4];
    PRUint8 b[8];
} RC2Block;

struct RC2ContextStr {
    union {
        PRUint8 Kb[128];
        PRUint16 Kw[64];
    } u;
    RC2Block iv;
    rc2Func *enc;
    rc2Func *dec;
};

#define B u.Kb
#define K u.Kw
#define BYTESWAP(x) ((x) << 8 | (x) >> 8)
#define SWAPK(i) cx->K[i] = (tmpS = cx->K[i], BYTESWAP(tmpS))
#define RC2_BLOCK_SIZE 8

#define LOAD_HARD(R)                           \
    R[0] = (PRUint16)input[1] << 8 | input[0]; \
    R[1] = (PRUint16)input[3] << 8 | input[2]; \
    R[2] = (PRUint16)input[5] << 8 | input[4]; \
    R[3] = (PRUint16)input[7] << 8 | input[6];
#define LOAD_EASY(R)               \
    R[0] = ((PRUint16 *)input)[0]; \
    R[1] = ((PRUint16 *)input)[1]; \
    R[2] = ((PRUint16 *)input)[2]; \
    R[3] = ((PRUint16 *)input)[3];
#define STORE_HARD(R)                 \
    output[0] = (PRUint8)(R[0]);      \
    output[1] = (PRUint8)(R[0] >> 8); \
    output[2] = (PRUint8)(R[1]);      \
    output[3] = (PRUint8)(R[1] >> 8); \
    output[4] = (PRUint8)(R[2]);      \
    output[5] = (PRUint8)(R[2] >> 8); \
    output[6] = (PRUint8)(R[3]);      \
    output[7] = (PRUint8)(R[3] >> 8);
#define STORE_EASY(R)               \
    ((PRUint16 *)output)[0] = R[0]; \
    ((PRUint16 *)output)[1] = R[1]; \
    ((PRUint16 *)output)[2] = R[2]; \
    ((PRUint16 *)output)[3] = R[3];

#if defined(NSS_X86_OR_X64)
#define LOAD(R) LOAD_EASY(R)
#define STORE(R) STORE_EASY(R)
#elif !defined(IS_LITTLE_ENDIAN)
#define LOAD(R) LOAD_HARD(R)
#define STORE(R) STORE_HARD(R)
#else
#define LOAD(R)                 \
    if ((ptrdiff_t)input & 1) { \
        LOAD_HARD(R)            \
    } else {                    \
        LOAD_EASY(R)            \
    }
#define STORE(R)                \
    if ((ptrdiff_t)input & 1) { \
        STORE_HARD(R)           \
    } else {                    \
        STORE_EASY(R)           \
    }
#endif

static const PRUint8 S[256] = {
    0331, 0170, 0371, 0304, 0031, 0335, 0265, 0355, 0050, 0351, 0375, 0171, 0112, 0240, 0330, 0235,
    0306, 0176, 0067, 0203, 0053, 0166, 0123, 0216, 0142, 0114, 0144, 0210, 0104, 0213, 0373, 0242,
    0027, 0232, 0131, 0365, 0207, 0263, 0117, 0023, 0141, 0105, 0155, 0215, 0011, 0201, 0175, 0062,
    0275, 0217, 0100, 0353, 0206, 0267, 0173, 0013, 0360, 0225, 0041, 0042, 0134, 0153, 0116, 0202,
    0124, 0326, 0145, 0223, 0316, 0140, 0262, 0034, 0163, 0126, 0300, 0024, 0247, 0214, 0361, 0334,
    0022, 0165, 0312, 0037, 0073, 0276, 0344, 0321, 0102, 0075, 0324, 0060, 0243, 0074, 0266, 0046,
    0157, 0277, 0016, 0332, 0106, 0151, 0007, 0127, 0047, 0362, 0035, 0233, 0274, 0224, 0103, 0003,
    0370, 0021, 0307, 0366, 0220, 0357, 0076, 0347, 0006, 0303, 0325, 0057, 0310, 0146, 0036, 0327,
    0010, 0350, 0352, 0336, 0200, 0122, 0356, 0367, 0204, 0252, 0162, 0254, 0065, 0115, 0152, 0052,
    0226, 0032, 0322, 0161, 0132, 0025, 0111, 0164, 0113, 0237, 0320, 0136, 0004, 0030, 0244, 0354,
    0302, 0340, 0101, 0156, 0017, 0121, 0313, 0314, 0044, 0221, 0257, 0120, 0241, 0364, 0160, 0071,
    0231, 0174, 0072, 0205, 0043, 0270, 0264, 0172, 0374, 0002, 0066, 0133, 0045, 0125, 0227, 0061,
    0055, 0135, 0372, 0230, 0343, 0212, 0222, 0256, 0005, 0337, 0051, 0020, 0147, 0154, 0272, 0311,
    0323, 0000, 0346, 0317, 0341, 0236, 0250, 0054, 0143, 0026, 0001, 0077, 0130, 0342, 0211, 0251,
    0015, 0070, 0064, 0033, 0253, 0063, 0377, 0260, 0273, 0110, 0014, 0137, 0271, 0261, 0315, 0056,
    0305, 0363, 0333, 0107, 0345, 0245, 0234, 0167, 0012, 0246, 0040, 0150, 0376, 0177, 0301, 0255
};

RC2Context *
RC2_AllocateContext(void)
{
    return PORT_ZNew(RC2Context);
}
SECStatus
RC2_InitContext(RC2Context *cx, const unsigned char *key, unsigned int len,
                const unsigned char *input, int mode, unsigned int efLen8,
                unsigned int unused)
{
    PRUint8 *L, *L2;
    int i;
#if !defined(IS_LITTLE_ENDIAN)
    PRUint16 tmpS;
#endif
    PRUint8 tmpB;

    if (!key || !cx || !len || len > (sizeof cx->B) ||
        efLen8 > (sizeof cx->B)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (mode == NSS_RC2) {
        /* groovy */
    } else if (mode == NSS_RC2_CBC) {
        if (!input) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
    } else {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (mode == NSS_RC2_CBC) {
        cx->enc = &rc2_EncryptCBC;
        cx->dec = &rc2_DecryptCBC;
        LOAD(cx->iv.s);
    } else {
        cx->enc = &rc2_EncryptECB;
        cx->dec = &rc2_DecryptECB;
    }

    /* Step 0. Copy key into table. */
    memcpy(cx->B, key, len);

    /* Step 1. Compute all values to the right of the key. */
    L2 = cx->B;
    L = L2 + len;
    tmpB = L[-1];
    for (i = (sizeof cx->B) - len; i > 0; --i) {
        *L++ = tmpB = S[(PRUint8)(tmpB + *L2++)];
    }

    /* step 2. Adjust left most byte of effective key. */
    i = (sizeof cx->B) - efLen8;
    L = cx->B + i;
    *L = tmpB = S[*L]; /* mask is always 0xff */

    /* step 3. Recompute all values to the left of effective key. */
    L2 = --L + efLen8;
    while (L >= cx->B) {
        *L-- = tmpB = S[tmpB ^ *L2--];
    }

#if !defined(IS_LITTLE_ENDIAN)
    for (i = 63; i >= 0; --i) {
        SWAPK(i); /* candidate for unrolling */
    }
#endif
    return SECSuccess;
}

/*
** Create a new RC2 context suitable for RC2 encryption/decryption.
**  "key" raw key data
**  "len" the number of bytes of key data
**  "iv" is the CBC initialization vector (if mode is NSS_RC2_CBC)
**  "mode" one of NSS_RC2 or NSS_RC2_CBC
**  "effectiveKeyLen" in bytes, not bits.
**
** When mode is set to NSS_RC2_CBC the RC2 cipher is run in "cipher block
** chaining" mode.
*/
RC2Context *
RC2_CreateContext(const unsigned char *key, unsigned int len,
                  const unsigned char *iv, int mode, unsigned efLen8)
{
    RC2Context *cx = PORT_ZNew(RC2Context);
    if (cx) {
        SECStatus rv = RC2_InitContext(cx, key, len, iv, mode, efLen8, 0);
        if (rv != SECSuccess) {
            RC2_DestroyContext(cx, PR_TRUE);
            cx = NULL;
        }
    }
    return cx;
}

/*
** Destroy an RC2 encryption/decryption context.
**  "cx" the context
**  "freeit" if PR_TRUE then free the object as well as its sub-objects
*/
void
RC2_DestroyContext(RC2Context *cx, PRBool freeit)
{
    if (cx) {
        memset(cx, 0, sizeof *cx);
        if (freeit) {
            PORT_Free(cx);
        }
    }
}

#define ROL(x, k) (x << k | x >> (16 - k))
#define MIX(j)                                           \
    R0 = R0 + cx->K[4 * j + 0] + (R3 & R2) + (~R3 & R1); \
    R0 = ROL(R0, 1);                                     \
    R1 = R1 + cx->K[4 * j + 1] + (R0 & R3) + (~R0 & R2); \
    R1 = ROL(R1, 2);                                     \
    R2 = R2 + cx->K[4 * j + 2] + (R1 & R0) + (~R1 & R3); \
    R2 = ROL(R2, 3);                                     \
    R3 = R3 + cx->K[4 * j + 3] + (R2 & R1) + (~R2 & R0); \
    R3 = ROL(R3, 5)
#define MASH                  \
    R0 = R0 + cx->K[R3 & 63]; \
    R1 = R1 + cx->K[R0 & 63]; \
    R2 = R2 + cx->K[R1 & 63]; \
    R3 = R3 + cx->K[R2 & 63]

/* Encrypt one block */
static void
rc2_Encrypt1Block(RC2Context *cx, RC2Block *output, RC2Block *input)
{
    register PRUint16 R0, R1, R2, R3;

    /* step 1. Initialize input. */
    R0 = input->s[0];
    R1 = input->s[1];
    R2 = input->s[2];
    R3 = input->s[3];

    /* step 2.  Expand Key (already done, in context) */
    /* step 3.  j = 0 */
    /* step 4.  Perform 5 mixing rounds. */

    MIX(0);
    MIX(1);
    MIX(2);
    MIX(3);
    MIX(4);

    /* step 5. Perform 1 mashing round. */
    MASH;

    /* step 6. Perform 6 mixing rounds. */

    MIX(5);
    MIX(6);
    MIX(7);
    MIX(8);
    MIX(9);
    MIX(10);

    /* step 7. Perform 1 mashing round. */
    MASH;

    /* step 8. Perform 5 mixing rounds. */

    MIX(11);
    MIX(12);
    MIX(13);
    MIX(14);
    MIX(15);

    /* output results */
    output->s[0] = R0;
    output->s[1] = R1;
    output->s[2] = R2;
    output->s[3] = R3;
}

#define ROR(x, k) (x >> k | x << (16 - k))
#define R_MIX(j)                                         \
    R3 = ROR(R3, 5);                                     \
    R3 = R3 - cx->K[4 * j + 3] - (R2 & R1) - (~R2 & R0); \
    R2 = ROR(R2, 3);                                     \
    R2 = R2 - cx->K[4 * j + 2] - (R1 & R0) - (~R1 & R3); \
    R1 = ROR(R1, 2);                                     \
    R1 = R1 - cx->K[4 * j + 1] - (R0 & R3) - (~R0 & R2); \
    R0 = ROR(R0, 1);                                     \
    R0 = R0 - cx->K[4 * j + 0] - (R3 & R2) - (~R3 & R1)
#define R_MASH                \
    R3 = R3 - cx->K[R2 & 63]; \
    R2 = R2 - cx->K[R1 & 63]; \
    R1 = R1 - cx->K[R0 & 63]; \
    R0 = R0 - cx->K[R3 & 63]

/* Encrypt one block */
static void
rc2_Decrypt1Block(RC2Context *cx, RC2Block *output, RC2Block *input)
{
    register PRUint16 R0, R1, R2, R3;

    /* step 1. Initialize input. */
    R0 = input->s[0];
    R1 = input->s[1];
    R2 = input->s[2];
    R3 = input->s[3];

    /* step 2.  Expand Key (already done, in context) */
    /* step 3.  j = 63 */
    /* step 4.  Perform 5 r_mixing rounds. */
    R_MIX(15);
    R_MIX(14);
    R_MIX(13);
    R_MIX(12);
    R_MIX(11);

    /* step 5.  Perform 1 r_mashing round. */
    R_MASH;

    /* step 6.  Perform 6 r_mixing rounds. */
    R_MIX(10);
    R_MIX(9);
    R_MIX(8);
    R_MIX(7);
    R_MIX(6);
    R_MIX(5);

    /* step 7.  Perform 1 r_mashing round. */
    R_MASH;

    /* step 8.  Perform 5 r_mixing rounds. */
    R_MIX(4);
    R_MIX(3);
    R_MIX(2);
    R_MIX(1);
    R_MIX(0);

    /* output results */
    output->s[0] = R0;
    output->s[1] = R1;
    output->s[2] = R2;
    output->s[3] = R3;
}

static SECStatus NO_SANITIZE_ALIGNMENT
rc2_EncryptECB(RC2Context *cx, unsigned char *output,
               const unsigned char *input, unsigned int inputLen)
{
    RC2Block iBlock;

    while (inputLen > 0) {
        LOAD(iBlock.s)
        rc2_Encrypt1Block(cx, &iBlock, &iBlock);
        STORE(iBlock.s)
        output += RC2_BLOCK_SIZE;
        input += RC2_BLOCK_SIZE;
        inputLen -= RC2_BLOCK_SIZE;
    }
    return SECSuccess;
}

static SECStatus NO_SANITIZE_ALIGNMENT
rc2_DecryptECB(RC2Context *cx, unsigned char *output,
               const unsigned char *input, unsigned int inputLen)
{
    RC2Block iBlock;

    while (inputLen > 0) {
        LOAD(iBlock.s)
        rc2_Decrypt1Block(cx, &iBlock, &iBlock);
        STORE(iBlock.s)
        output += RC2_BLOCK_SIZE;
        input += RC2_BLOCK_SIZE;
        inputLen -= RC2_BLOCK_SIZE;
    }
    return SECSuccess;
}

static SECStatus NO_SANITIZE_ALIGNMENT
rc2_EncryptCBC(RC2Context *cx, unsigned char *output,
               const unsigned char *input, unsigned int inputLen)
{
    RC2Block iBlock;

    while (inputLen > 0) {

        LOAD(iBlock.s)
        iBlock.l[0] ^= cx->iv.l[0];
        iBlock.l[1] ^= cx->iv.l[1];
        rc2_Encrypt1Block(cx, &iBlock, &iBlock);
        cx->iv = iBlock;
        STORE(iBlock.s)
        output += RC2_BLOCK_SIZE;
        input += RC2_BLOCK_SIZE;
        inputLen -= RC2_BLOCK_SIZE;
    }
    return SECSuccess;
}

static SECStatus NO_SANITIZE_ALIGNMENT
rc2_DecryptCBC(RC2Context *cx, unsigned char *output,
               const unsigned char *input, unsigned int inputLen)
{
    RC2Block iBlock;
    RC2Block oBlock;

    while (inputLen > 0) {
        LOAD(iBlock.s)
        rc2_Decrypt1Block(cx, &oBlock, &iBlock);
        oBlock.l[0] ^= cx->iv.l[0];
        oBlock.l[1] ^= cx->iv.l[1];
        cx->iv = iBlock;
        STORE(oBlock.s)
        output += RC2_BLOCK_SIZE;
        input += RC2_BLOCK_SIZE;
        inputLen -= RC2_BLOCK_SIZE;
    }
    return SECSuccess;
}

/*
** Perform RC2 encryption.
**  "cx" the context
**  "output" the output buffer to store the encrypted data.
**  "outputLen" how much data is stored in "output". Set by the routine
**     after some data is stored in output.
**  "maxOutputLen" the maximum amount of data that can ever be
**     stored in "output"
**  "input" the input data
**  "inputLen" the amount of input data
*/
SECStatus
RC2_Encrypt(RC2Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECSuccess;
    if (inputLen) {
        if (inputLen % RC2_BLOCK_SIZE) {
            PORT_SetError(SEC_ERROR_INPUT_LEN);
            return SECFailure;
        }
        if (maxOutputLen < inputLen) {
            PORT_SetError(SEC_ERROR_OUTPUT_LEN);
            return SECFailure;
        }
        rv = (*cx->enc)(cx, output, input, inputLen);
    }
    if (rv == SECSuccess) {
        *outputLen = inputLen;
    }
    return rv;
}

/*
** Perform RC2 decryption.
**  "cx" the context
**  "output" the output buffer to store the decrypted data.
**  "outputLen" how much data is stored in "output". Set by the routine
**     after some data is stored in output.
**  "maxOutputLen" the maximum amount of data that can ever be
**     stored in "output"
**  "input" the input data
**  "inputLen" the amount of input data
*/
SECStatus
RC2_Decrypt(RC2Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECSuccess;
    if (inputLen) {
        if (inputLen % RC2_BLOCK_SIZE) {
            PORT_SetError(SEC_ERROR_INPUT_LEN);
            return SECFailure;
        }
        if (maxOutputLen < inputLen) {
            PORT_SetError(SEC_ERROR_OUTPUT_LEN);
            return SECFailure;
        }
        rv = (*cx->dec)(cx, output, input, inputLen);
    }
    if (rv == SECSuccess) {
        *outputLen = inputLen;
    }
    return rv;
}
