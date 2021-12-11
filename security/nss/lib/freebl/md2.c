/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"

#include "blapi.h"

#define MD2_DIGEST_LEN 16
#define MD2_BUFSIZE 16
#define MD2_X_SIZE 48  /* The X array, [CV | INPUT | TMP VARS] */
#define MD2_CV 0       /* index into X for chaining variables */
#define MD2_INPUT 16   /* index into X for input */
#define MD2_TMPVARS 32 /* index into X for temporary variables */
#define MD2_CHECKSUM_SIZE 16

struct MD2ContextStr {
    unsigned char checksum[MD2_BUFSIZE];
    unsigned char X[MD2_X_SIZE];
    PRUint8 unusedBuffer;
};

static const PRUint8 MD2S[256] = {
    0051, 0056, 0103, 0311, 0242, 0330, 0174, 0001,
    0075, 0066, 0124, 0241, 0354, 0360, 0006, 0023,
    0142, 0247, 0005, 0363, 0300, 0307, 0163, 0214,
    0230, 0223, 0053, 0331, 0274, 0114, 0202, 0312,
    0036, 0233, 0127, 0074, 0375, 0324, 0340, 0026,
    0147, 0102, 0157, 0030, 0212, 0027, 0345, 0022,
    0276, 0116, 0304, 0326, 0332, 0236, 0336, 0111,
    0240, 0373, 0365, 0216, 0273, 0057, 0356, 0172,
    0251, 0150, 0171, 0221, 0025, 0262, 0007, 0077,
    0224, 0302, 0020, 0211, 0013, 0042, 0137, 0041,
    0200, 0177, 0135, 0232, 0132, 0220, 0062, 0047,
    0065, 0076, 0314, 0347, 0277, 0367, 0227, 0003,
    0377, 0031, 0060, 0263, 0110, 0245, 0265, 0321,
    0327, 0136, 0222, 0052, 0254, 0126, 0252, 0306,
    0117, 0270, 0070, 0322, 0226, 0244, 0175, 0266,
    0166, 0374, 0153, 0342, 0234, 0164, 0004, 0361,
    0105, 0235, 0160, 0131, 0144, 0161, 0207, 0040,
    0206, 0133, 0317, 0145, 0346, 0055, 0250, 0002,
    0033, 0140, 0045, 0255, 0256, 0260, 0271, 0366,
    0034, 0106, 0141, 0151, 0064, 0100, 0176, 0017,
    0125, 0107, 0243, 0043, 0335, 0121, 0257, 0072,
    0303, 0134, 0371, 0316, 0272, 0305, 0352, 0046,
    0054, 0123, 0015, 0156, 0205, 0050, 0204, 0011,
    0323, 0337, 0315, 0364, 0101, 0201, 0115, 0122,
    0152, 0334, 0067, 0310, 0154, 0301, 0253, 0372,
    0044, 0341, 0173, 0010, 0014, 0275, 0261, 0112,
    0170, 0210, 0225, 0213, 0343, 0143, 0350, 0155,
    0351, 0313, 0325, 0376, 0073, 0000, 0035, 0071,
    0362, 0357, 0267, 0016, 0146, 0130, 0320, 0344,
    0246, 0167, 0162, 0370, 0353, 0165, 0113, 0012,
    0061, 0104, 0120, 0264, 0217, 0355, 0037, 0032,
    0333, 0231, 0215, 0063, 0237, 0021, 0203, 0024
};

SECStatus
MD2_Hash(unsigned char *dest, const char *src)
{
    unsigned int len;
    MD2Context *cx = MD2_NewContext();
    if (!cx) {
        PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
        return SECFailure;
    }
    MD2_Begin(cx);
    MD2_Update(cx, (const unsigned char *)src, PORT_Strlen(src));
    MD2_End(cx, dest, &len, MD2_DIGEST_LEN);
    MD2_DestroyContext(cx, PR_TRUE);
    return SECSuccess;
}

MD2Context *
MD2_NewContext(void)
{
    MD2Context *cx = (MD2Context *)PORT_ZAlloc(sizeof(MD2Context));
    if (cx == NULL) {
        PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
        return NULL;
    }
    return cx;
}

void
MD2_DestroyContext(MD2Context *cx, PRBool freeit)
{
    if (freeit)
        PORT_ZFree(cx, sizeof(*cx));
}

void
MD2_Begin(MD2Context *cx)
{
    memset(cx, 0, sizeof(*cx));
    cx->unusedBuffer = MD2_BUFSIZE;
}

static void
md2_compress(MD2Context *cx)
{
    int j;
    unsigned char P;
    P = cx->checksum[MD2_CHECKSUM_SIZE - 1];
/* Compute the running checksum, and set the tmp variables to be
     * CV[i] XOR input[i]
     */
#define CKSUMFN(n)                                        \
    P = cx->checksum[n] ^ MD2S[cx->X[MD2_INPUT + n] ^ P]; \
    cx->checksum[n] = P;                                  \
    cx->X[MD2_TMPVARS + n] = cx->X[n] ^ cx->X[MD2_INPUT + n];
    CKSUMFN(0);
    CKSUMFN(1);
    CKSUMFN(2);
    CKSUMFN(3);
    CKSUMFN(4);
    CKSUMFN(5);
    CKSUMFN(6);
    CKSUMFN(7);
    CKSUMFN(8);
    CKSUMFN(9);
    CKSUMFN(10);
    CKSUMFN(11);
    CKSUMFN(12);
    CKSUMFN(13);
    CKSUMFN(14);
    CKSUMFN(15);
/* The compression function. */
#define COMPRESS(n)         \
    P = cx->X[n] ^ MD2S[P]; \
    cx->X[n] = P;
    P = 0x00;
    for (j = 0; j < 18; j++) {
        COMPRESS(0);
        COMPRESS(1);
        COMPRESS(2);
        COMPRESS(3);
        COMPRESS(4);
        COMPRESS(5);
        COMPRESS(6);
        COMPRESS(7);
        COMPRESS(8);
        COMPRESS(9);
        COMPRESS(10);
        COMPRESS(11);
        COMPRESS(12);
        COMPRESS(13);
        COMPRESS(14);
        COMPRESS(15);
        COMPRESS(16);
        COMPRESS(17);
        COMPRESS(18);
        COMPRESS(19);
        COMPRESS(20);
        COMPRESS(21);
        COMPRESS(22);
        COMPRESS(23);
        COMPRESS(24);
        COMPRESS(25);
        COMPRESS(26);
        COMPRESS(27);
        COMPRESS(28);
        COMPRESS(29);
        COMPRESS(30);
        COMPRESS(31);
        COMPRESS(32);
        COMPRESS(33);
        COMPRESS(34);
        COMPRESS(35);
        COMPRESS(36);
        COMPRESS(37);
        COMPRESS(38);
        COMPRESS(39);
        COMPRESS(40);
        COMPRESS(41);
        COMPRESS(42);
        COMPRESS(43);
        COMPRESS(44);
        COMPRESS(45);
        COMPRESS(46);
        COMPRESS(47);
        P = (P + j) % 256;
    }
    cx->unusedBuffer = MD2_BUFSIZE;
}

void
MD2_Update(MD2Context *cx, const unsigned char *input, unsigned int inputLen)
{
    PRUint32 bytesToConsume;

    /* Fill the remaining input buffer. */
    if (cx->unusedBuffer != MD2_BUFSIZE) {
        bytesToConsume = PR_MIN(inputLen, cx->unusedBuffer);
        memcpy(&cx->X[MD2_INPUT + (MD2_BUFSIZE - cx->unusedBuffer)],
               input, bytesToConsume);
        if (cx->unusedBuffer + bytesToConsume >= MD2_BUFSIZE)
            md2_compress(cx);
        inputLen -= bytesToConsume;
        input += bytesToConsume;
    }

    /* Iterate over 16-byte chunks of the input. */
    while (inputLen >= MD2_BUFSIZE) {
        memcpy(&cx->X[MD2_INPUT], input, MD2_BUFSIZE);
        md2_compress(cx);
        inputLen -= MD2_BUFSIZE;
        input += MD2_BUFSIZE;
    }

    /* Copy any input that remains into the buffer. */
    if (inputLen)
        memcpy(&cx->X[MD2_INPUT], input, inputLen);
    cx->unusedBuffer = MD2_BUFSIZE - inputLen;
}

void
MD2_End(MD2Context *cx, unsigned char *digest,
        unsigned int *digestLen, unsigned int maxDigestLen)
{
    PRUint8 padStart;
    if (maxDigestLen < MD2_BUFSIZE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return;
    }
    padStart = MD2_BUFSIZE - cx->unusedBuffer;
    memset(&cx->X[MD2_INPUT + padStart], cx->unusedBuffer,
           cx->unusedBuffer);
    md2_compress(cx);
    memcpy(&cx->X[MD2_INPUT], cx->checksum, MD2_BUFSIZE);
    md2_compress(cx);
    *digestLen = MD2_DIGEST_LEN;
    memcpy(digest, &cx->X[MD2_CV], MD2_DIGEST_LEN);
}

unsigned int
MD2_FlattenSize(MD2Context *cx)
{
    return sizeof(*cx);
}

SECStatus
MD2_Flatten(MD2Context *cx, unsigned char *space)
{
    memcpy(space, cx, sizeof(*cx));
    return SECSuccess;
}

MD2Context *
MD2_Resurrect(unsigned char *space, void *arg)
{
    MD2Context *cx = MD2_NewContext();
    if (cx)
        memcpy(cx, space, sizeof(*cx));
    return cx;
}

void
MD2_Clone(MD2Context *dest, MD2Context *src)
{
    memcpy(dest, src, sizeof *dest);
}
