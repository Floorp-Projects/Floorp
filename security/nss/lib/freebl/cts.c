/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "blapit.h"
#include "blapii.h"
#include "cts.h"
#include "secerr.h"

struct CTSContextStr {
    freeblCipherFunc cipher;
    void *context;
    /* iv stores the last ciphertext block of the previous message.
     * Only used by decrypt. */
    unsigned char iv[MAX_BLOCK_SIZE];
};

CTSContext *
CTS_CreateContext(void *context, freeblCipherFunc cipher,
                  const unsigned char *iv)
{
    CTSContext *cts;

    cts = PORT_ZNew(CTSContext);
    if (cts == NULL) {
        return NULL;
    }
    PORT_Memcpy(cts->iv, iv, MAX_BLOCK_SIZE);
    cts->cipher = cipher;
    cts->context = context;
    return cts;
}

void
CTS_DestroyContext(CTSContext *cts, PRBool freeit)
{
    if (freeit) {
        PORT_Free(cts);
    }
}

/*
 * See addemdum to NIST SP 800-38A
 * Generically handle cipher text stealing. Basically this is doing CBC
 * operations except someone can pass us a partial block.
 *
 *  Output Order:
 *  CS-1:  C1||C2||C3..Cn-1(could be partial)||Cn   (NIST)
 *  CS-2: pad == 0 C1||C2||C3...Cn-1(is full)||Cn   (Schneier)
 *  CS-2: pad != 0 C1||C2||C3...Cn||Cn-1(is partial)(Schneier)
 *  CS-3: C1||C2||C3...Cn||Cn-1(could be partial)   (Kerberos)
 *
 * The characteristics of these three options:
 *  - NIST & Schneier (CS-1 & CS-2) are identical to CBC if there are no
 * partial blocks on input.
 *  - Scheier and Kerberos (CS-2 and CS-3) have no embedded partial blocks,
 * which make decoding easier.
 *  - NIST & Kerberos (CS-1 and CS-3) have consistent block order independent
 * of padding.
 *
 * PKCS #11 did not specify which version to implement, but points to the NIST
 * spec, so this code implements CTS-CS-1 from NIST.
 *
 * To convert the returned buffer to:
 *   CS-2 (Schneier): do
 *       unsigned char tmp[MAX_BLOCK_SIZE];
 *       pad = *outlen % blocksize;
 *       if (pad) {
 *          memcpy(tmp, outbuf+*outlen-blocksize, blocksize);
 *          memcpy(outbuf+*outlen-pad,outbuf+*outlen-blocksize-pad, pad);
 *      memcpy(outbuf+*outlen-blocksize-pad, tmp, blocksize);
 *       }
 *   CS-3 (Kerberos): do
 *       unsigned char tmp[MAX_BLOCK_SIZE];
 *       pad = *outlen % blocksize;
 *       if (pad == 0) {
 *           pad = blocksize;
 *       }
 *       memcpy(tmp, outbuf+*outlen-blocksize, blocksize);
 *       memcpy(outbuf+*outlen-pad,outbuf+*outlen-blocksize-pad, pad);
 *   memcpy(outbuf+*outlen-blocksize-pad, tmp, blocksize);
 */
SECStatus
CTS_EncryptUpdate(CTSContext *cts, unsigned char *outbuf,
                  unsigned int *outlen, unsigned int maxout,
                  const unsigned char *inbuf, unsigned int inlen,
                  unsigned int blocksize)
{
    unsigned char lastBlock[MAX_BLOCK_SIZE];
    unsigned int tmp;
    int fullblocks;
    int written;
    unsigned char *saveout = outbuf;
    SECStatus rv;

    if (inlen < blocksize) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    fullblocks = (inlen / blocksize) * blocksize;
    rv = (*cts->cipher)(cts->context, outbuf, outlen, maxout, inbuf,
                        fullblocks, blocksize);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    *outlen = fullblocks; /* AES low level doesn't set outlen */
    inbuf += fullblocks;
    inlen -= fullblocks;
    if (inlen == 0) {
        return SECSuccess;
    }
    written = *outlen - (blocksize - inlen);
    outbuf += written;
    maxout -= written;

    /*
     * here's the CTS magic, we pad our final block with zeros,
     * then do a CBC encrypt. CBC will xor our plain text with
     * the previous block (Cn-1), capturing part of that block (Cn-1**) as it
     * xors with the zero pad. We then write this full block, overwritting
     * (Cn-1**) in our buffer. This allows us to have input data == output
     * data since Cn contains enough information to reconver Cn-1** when
     * we decrypt (at the cost of some complexity as you can see in decrypt
     * below */
    PORT_Memcpy(lastBlock, inbuf, inlen);
    PORT_Memset(lastBlock + inlen, 0, blocksize - inlen);
    rv = (*cts->cipher)(cts->context, outbuf, &tmp, maxout, lastBlock,
                        blocksize, blocksize);
    PORT_Memset(lastBlock, 0, blocksize);
    if (rv == SECSuccess) {
        *outlen = written + blocksize;
    } else {
        PORT_Memset(saveout, 0, written + blocksize);
    }
    return rv;
}

#define XOR_BLOCK(x, y, count)  \
    for (i = 0; i < count; i++) \
    x[i] = x[i] ^ y[i]

/*
 * See addemdum to NIST SP 800-38A
 * Decrypt, Expect CS-1: input. See the comment on the encrypt side
 * to understand what CS-2 and CS-3 mean.
 *
 * To convert the input buffer to CS-1 from ...
 *   CS-2 (Schneier): do
 *       unsigned char tmp[MAX_BLOCK_SIZE];
 *       pad = inlen % blocksize;
 *       if (pad) {
 *          memcpy(tmp, inbuf+inlen-blocksize-pad, blocksize);
 *          memcpy(inbuf+inlen-blocksize-pad,inbuf+inlen-pad, pad);
 *      memcpy(inbuf+inlen-blocksize, tmp, blocksize);
 *       }
 *   CS-3 (Kerberos): do
 *       unsigned char tmp[MAX_BLOCK_SIZE];
 *       pad = inlen % blocksize;
 *       if (pad == 0) {
 *           pad = blocksize;
 *       }
 *       memcpy(tmp, inbuf+inlen-blocksize-pad, blocksize);
 *       memcpy(inbuf+inlen-blocksize-pad,inbuf+inlen-pad, pad);
 *   memcpy(inbuf+inlen-blocksize, tmp, blocksize);
 */
SECStatus
CTS_DecryptUpdate(CTSContext *cts, unsigned char *outbuf,
                  unsigned int *outlen, unsigned int maxout,
                  const unsigned char *inbuf, unsigned int inlen,
                  unsigned int blocksize)
{
    unsigned char *Pn;
    unsigned char Cn_2[MAX_BLOCK_SIZE]; /* block Cn-2 */
    unsigned char Cn_1[MAX_BLOCK_SIZE]; /* block Cn-1 */
    unsigned char Cn[MAX_BLOCK_SIZE];   /* block Cn   */
    unsigned char lastBlock[MAX_BLOCK_SIZE];
    const unsigned char *tmp;
    unsigned char *saveout = outbuf;
    unsigned int tmpLen;
    unsigned int fullblocks, pad;
    unsigned int i;
    SECStatus rv;

    if (inlen < blocksize) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    fullblocks = (inlen / blocksize) * blocksize;

    /* even though we expect the input to be CS-1, CS-2 is easier to parse,
     * so convert to CS-2 immediately. NOTE: this is the same code as in
     * the comment for encrypt. NOTE2: since we can't modify inbuf unless
     * inbuf and outbuf overlap, just copy inbuf to outbuf and modify it there
     */
    pad = inlen - fullblocks;
    if (pad != 0) {
        if (inbuf != outbuf) {
            memcpy(outbuf, inbuf, inlen);
            /* keep the names so we logically know how we are using the
         * buffers */
            inbuf = outbuf;
        }
        memcpy(lastBlock, inbuf + inlen - blocksize, blocksize);
        /* we know inbuf == outbuf now, inbuf is declared const and can't
     * be the target, so use outbuf for the target here */
        memcpy(outbuf + inlen - pad, inbuf + inlen - blocksize - pad, pad);
        memcpy(outbuf + inlen - blocksize - pad, lastBlock, blocksize);
    }
    /* save the previous to last block so we can undo the misordered
     * chaining */
    tmp = (fullblocks < blocksize * 2) ? cts->iv : inbuf + fullblocks - blocksize * 2;
    PORT_Memcpy(Cn_2, tmp, blocksize);
    PORT_Memcpy(Cn, inbuf + fullblocks - blocksize, blocksize);
    rv = (*cts->cipher)(cts->context, outbuf, outlen, maxout, inbuf,
                        fullblocks, blocksize);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    *outlen = fullblocks; /* AES low level doesn't set outlen */
    inbuf += fullblocks;
    inlen -= fullblocks;
    if (inlen == 0) {
        return SECSuccess;
    }
    outbuf += fullblocks;

    /* recover the stolen text */
    PORT_Memset(lastBlock, 0, blocksize);
    PORT_Memcpy(lastBlock, inbuf, inlen);
    PORT_Memcpy(Cn_1, inbuf, inlen);
    Pn = outbuf - blocksize;
    /* inbuf points to Cn-1* in the input buffer */
    /* NOTE: below there are 2 sections marked "make up for the out of order
     * cbc decryption". You may ask, what is going on here.
     *   Short answer: CBC automatically xors the plain text with the previous
     * encrypted block. We are decrypting the last 2 blocks out of order, so
     * we have to 'back out' the decrypt xor and 'add back' the encrypt xor.
     *   Long answer: When we encrypted, we encrypted as follows:
     *       Pn-2, Pn-1, (Pn || 0), but on decryption we can't
     *  decrypt Cn-1 until we decrypt Cn because part of Cn-1 is stored in
     *  Cn (see below).  So above we decrypted all the full blocks:
     *       Cn-2, Cn,
     *  to get:
     *       Pn-2, Pn, Except that Pn is not yet corect. On encrypt, we
     *  xor'd Pn || 0  with Cn-1, but on decrypt we xor'd it with Cn-2
     *  To recover Pn, we xor the block with Cn-1* || 0 (in last block) and
     *  Cn-2 to get Pn || Cn-1**. Pn can then be written to the output buffer
     *  and we can now reunite Cn-1. With the full Cn-1 we can decrypt it,
     *  but now decrypt is going to xor the decrypted data with Cn instead of
     *  Cn-2. xoring Cn and Cn-2 restores the original Pn-1 and we can now
     *  write that oout to the buffer */

    /* make up for the out of order CBC decryption */
    XOR_BLOCK(lastBlock, Cn_2, blocksize);
    XOR_BLOCK(lastBlock, Pn, blocksize);
    /* last buf now has Pn || Cn-1**, copy out Pn */
    PORT_Memcpy(outbuf, lastBlock, inlen);
    *outlen += inlen;
    /* copy Cn-1* into last buf to recover Cn-1 */
    PORT_Memcpy(lastBlock, Cn_1, inlen);
    /* note: because Cn and Cn-1 were out of order, our pointer to Pn also
     * points to where Pn-1 needs to reside. From here on out read Pn in
     * the code as really Pn-1. */
    rv = (*cts->cipher)(cts->context, Pn, &tmpLen, blocksize, lastBlock,
                        blocksize, blocksize);
    if (rv != SECSuccess) {
        PORT_Memset(lastBlock, 0, blocksize);
        PORT_Memset(saveout, 0, *outlen);
        return SECFailure;
    }
    /* make up for the out of order CBC decryption */
    XOR_BLOCK(Pn, Cn_2, blocksize);
    XOR_BLOCK(Pn, Cn, blocksize);
    /* reset iv to Cn  */
    PORT_Memcpy(cts->iv, Cn, blocksize);
    /* This makes Cn the last block for the next decrypt operation, which
     * matches the encrypt. We don't care about the contexts of last block,
     * only the side effect of setting the internal IV */
    (void)(*cts->cipher)(cts->context, lastBlock, &tmpLen, blocksize, Cn,
                         blocksize, blocksize);
    /* clear last block. At this point last block contains Pn xor Cn_1 xor
     * Cn_2, both of with an attacker would know, so we need to clear this
     * buffer out */
    PORT_Memset(lastBlock, 0, blocksize);
    /* Cn, Cn_1, and Cn_2 have encrypted data, so no need to clear them */
    return SECSuccess;
}
