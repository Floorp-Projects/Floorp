/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secport.h"
#include "hasht.h"
#include "blapit.h"
#include "hmacct.h"
#include "secerr.h"

/* MAX_HASH_BIT_COUNT_BYTES is the maximum number of bytes in the hash's length
 * field. (SHA-384/512 have 128-bit length.) */
#define MAX_HASH_BIT_COUNT_BYTES 16

/* Some utility functions are needed:
 *
 * These macros return the given value with the MSB copied to all the other
 * bits. They use the fact that an arithmetic shift shifts-in the sign bit.
 * However, this is not ensured by the C standard so you may need to replace
 * them with something else on odd CPUs.
 *
 * Note: the argument to these macros must be an unsigned int.
 * */
#define DUPLICATE_MSB_TO_ALL(x) ((unsigned int)((int)(x) >> (sizeof(int) * 8 - 1)))
#define DUPLICATE_MSB_TO_ALL_8(x) ((unsigned char)(DUPLICATE_MSB_TO_ALL(x)))

/* constantTimeGE returns 0xff if a>=b and 0x00 otherwise, where a, b <
 * MAX_UINT/2. */
static unsigned char
constantTimeGE(unsigned int a, unsigned int b)
{
    a -= b;
    return DUPLICATE_MSB_TO_ALL(~a);
}

/* constantTimeEQ8 returns 0xff if a==b and 0x00 otherwise. */
static unsigned char
constantTimeEQ8(unsigned char a, unsigned char b)
{
    unsigned int c = a ^ b;
    c--;
    return DUPLICATE_MSB_TO_ALL_8(c);
}

/* MAC performs a constant time SSLv3/TLS MAC of |dataLen| bytes of |data|,
 * where |dataLen| includes both the authenticated bytes and the MAC tag from
 * the sender. |dataLen| must be >= the length of the MAC tag.
 *
 * |dataTotalLen| is >= |dataLen| and also accounts for any padding bytes
 * that may follow the sender's MAC. (Only a single block of padding may
 * follow in SSLv3, or up to 255 bytes in TLS.)
 *
 * Since the results of decryption are secret information (otherwise a
 * padding-oracle is created), this function is constant-time with respect to
 * |dataLen|.
 *
 * |header| contains either the 13-byte TLS header (containing the sequence
 * number, record type etc), or it contains the SSLv3 header with the SSLv3
 * padding bytes etc. */
static SECStatus
MAC(unsigned char *mdOut,
    unsigned int *mdOutLen,
    unsigned int mdOutMax,
    const SECHashObject *hashObj,
    const unsigned char *macSecret,
    unsigned int macSecretLen,
    const unsigned char *header,
    unsigned int headerLen,
    const unsigned char *data,
    unsigned int dataLen,
    unsigned int dataTotalLen,
    unsigned char isSSLv3)
{
    void *mdState = hashObj->create();
    const unsigned int mdSize = hashObj->length;
    const unsigned int mdBlockSize = hashObj->blocklength;
    /* mdLengthSize is the number of bytes in the length field that terminates
     * the hash.
     *
     * This assumes that hash functions with a 64 byte block size use a 64-bit
     * length, and otherwise they use a 128-bit length. This is true of {MD5,
     * SHA*} (which are all of the hash functions specified for use with TLS
     * today). */
    const unsigned int mdLengthSize = mdBlockSize == 64 ? 8 : 16;

    const unsigned int sslv3PadLen = hashObj->type == HASH_AlgMD5 ? 48 : 40;

    /* varianceBlocks is the number of blocks of the hash that we have to
     * calculate in constant time because they could be altered by the
     * padding value.
     *
     * In SSLv3, the padding must be minimal so the end of the plaintext
     * varies by, at most, 15+20 = 35 bytes. (We conservatively assume that
     * the MAC size varies from 0..20 bytes.) In case the 9 bytes of hash
     * termination (0x80 + 64-bit length) don't fit in the final block, we
     * say that the final two blocks can vary based on the padding.
     *
     * TLSv1 has MACs up to 48 bytes long (SHA-384) and the padding is not
     * required to be minimal. Therefore we say that the final six blocks
     * can vary based on the padding.
     *
     * Later in the function, if the message is short and there obviously
     * cannot be this many blocks then varianceBlocks can be reduced. */
    unsigned int varianceBlocks = isSSLv3 ? 2 : 6;
    /* From now on we're dealing with the MAC, which conceptually has 13
     * bytes of `header' before the start of the data (TLS) or 71/75 bytes
     * (SSLv3) */
    const unsigned int len = dataTotalLen + headerLen;
    /* maxMACBytes contains the maximum bytes of bytes in the MAC, including
     * |header|, assuming that there's no padding. */
    const unsigned int maxMACBytes = len - mdSize - 1;
    /* numBlocks is the maximum number of hash blocks. */
    const unsigned int numBlocks =
        (maxMACBytes + 1 + mdLengthSize + mdBlockSize - 1) / mdBlockSize;
    /* macEndOffset is the index just past the end of the data to be
     * MACed. */
    const unsigned int macEndOffset = dataLen + headerLen - mdSize;
    /* c is the index of the 0x80 byte in the final hash block that
     * contains application data. */
    const unsigned int c = macEndOffset % mdBlockSize;
    /* indexA is the hash block number that contains the 0x80 terminating
     * value. */
    const unsigned int indexA = macEndOffset / mdBlockSize;
    /* indexB is the hash block number that contains the 64-bit hash
     * length, in bits. */
    const unsigned int indexB = (macEndOffset + mdLengthSize) / mdBlockSize;
    /* bits is the hash-length in bits. It includes the additional hash
     * block for the masked HMAC key, or whole of |header| in the case of
     * SSLv3. */
    unsigned int bits;
    /* In order to calculate the MAC in constant time we have to handle
     * the final blocks specially because the padding value could cause the
     * end to appear somewhere in the final |varianceBlocks| blocks and we
     * can't leak where. However, |numStartingBlocks| worth of data can
     * be hashed right away because no padding value can affect whether
     * they are plaintext. */
    unsigned int numStartingBlocks = 0;
    /* k is the starting byte offset into the conceptual header||data where
     * we start processing. */
    unsigned int k = 0;
    unsigned char lengthBytes[MAX_HASH_BIT_COUNT_BYTES];
    /* hmacPad is the masked HMAC key. */
    unsigned char hmacPad[HASH_BLOCK_LENGTH_MAX];
    unsigned char firstBlock[HASH_BLOCK_LENGTH_MAX];
    unsigned char macOut[HASH_LENGTH_MAX];
    unsigned i, j;

    /* For SSLv3, if we're going to have any starting blocks then we need
     * at least two because the header is larger than a single block. */
    if (numBlocks > varianceBlocks + (isSSLv3 ? 1 : 0)) {
        numStartingBlocks = numBlocks - varianceBlocks;
        k = mdBlockSize * numStartingBlocks;
    }

    bits = 8 * macEndOffset;
    hashObj->begin(mdState);
    if (!isSSLv3) {
        /* Compute the initial HMAC block. For SSLv3, the padding and
         * secret bytes are included in |header| because they take more
         * than a single block. */
        bits += 8 * mdBlockSize;
        memset(hmacPad, 0, mdBlockSize);
        PORT_Assert(macSecretLen <= sizeof(hmacPad));
        memcpy(hmacPad, macSecret, macSecretLen);
        for (i = 0; i < mdBlockSize; i++)
            hmacPad[i] ^= 0x36;
        hashObj->update(mdState, hmacPad, mdBlockSize);
    }

    j = 0;
    memset(lengthBytes, 0, sizeof(lengthBytes));
    if (mdLengthSize == 16) {
        j = 8;
    }
    if (hashObj->type == HASH_AlgMD5) {
        /* MD5 appends a little-endian length. */
        for (i = 0; i < 4; i++) {
            lengthBytes[i + j] = bits >> (8 * i);
        }
    } else {
        /* All other TLS hash functions use a big-endian length. */
        for (i = 0; i < 4; i++) {
            lengthBytes[4 + i + j] = bits >> (8 * (3 - i));
        }
    }

    if (k > 0) {
        if (isSSLv3) {
            /* The SSLv3 header is larger than a single block.
             * overhang is the number of bytes beyond a single
             * block that the header consumes: either 7 bytes
             * (SHA1) or 11 bytes (MD5). */
            const unsigned int overhang = headerLen - mdBlockSize;
            hashObj->update(mdState, header, mdBlockSize);
            memcpy(firstBlock, header + mdBlockSize, overhang);
            memcpy(firstBlock + overhang, data, mdBlockSize - overhang);
            hashObj->update(mdState, firstBlock, mdBlockSize);
            for (i = 1; i < k / mdBlockSize - 1; i++) {
                hashObj->update(mdState, data + mdBlockSize * i - overhang,
                                mdBlockSize);
            }
        } else {
            /* k is a multiple of mdBlockSize. */
            memcpy(firstBlock, header, 13);
            memcpy(firstBlock + 13, data, mdBlockSize - 13);
            hashObj->update(mdState, firstBlock, mdBlockSize);
            for (i = 1; i < k / mdBlockSize; i++) {
                hashObj->update(mdState, data + mdBlockSize * i - 13,
                                mdBlockSize);
            }
        }
    }

    memset(macOut, 0, sizeof(macOut));

    /* We now process the final hash blocks. For each block, we construct
     * it in constant time. If i == indexA then we'll include the 0x80
     * bytes and zero pad etc. For each block we selectively copy it, in
     * constant time, to |macOut|. */
    for (i = numStartingBlocks; i <= numStartingBlocks + varianceBlocks; i++) {
        unsigned char block[HASH_BLOCK_LENGTH_MAX];
        unsigned char isBlockA = constantTimeEQ8(i, indexA);
        unsigned char isBlockB = constantTimeEQ8(i, indexB);
        for (j = 0; j < mdBlockSize; j++) {
            unsigned char isPastC = isBlockA & constantTimeGE(j, c);
            unsigned char isPastCPlus1 = isBlockA & constantTimeGE(j, c + 1);
            unsigned char b = 0;
            if (k < headerLen) {
                b = header[k];
            } else if (k < dataTotalLen + headerLen) {
                b = data[k - headerLen];
            }
            k++;

            /* If this is the block containing the end of the
             * application data, and we are at the offset for the
             * 0x80 value, then overwrite b with 0x80. */
            b = (b & ~isPastC) | (0x80 & isPastC);
            /* If this the the block containing the end of the
             * application data and we're past the 0x80 value then
             * just write zero. */
            b = b & ~isPastCPlus1;
            /* If this is indexB (the final block), but not
             * indexA (the end of the data), then the 64-bit
             * length didn't fit into indexA and we're having to
             * add an extra block of zeros. */
            b &= ~isBlockB | isBlockA;

            /* The final bytes of one of the blocks contains the length. */
            if (j >= mdBlockSize - mdLengthSize) {
                /* If this is indexB, write a length byte. */
                b = (b & ~isBlockB) |
                    (isBlockB & lengthBytes[j - (mdBlockSize - mdLengthSize)]);
            }
            block[j] = b;
        }

        hashObj->update(mdState, block, mdBlockSize);
        hashObj->end_raw(mdState, block, NULL, mdSize);
        /* If this is indexB, copy the hash value to |macOut|. */
        for (j = 0; j < mdSize; j++) {
            macOut[j] |= block[j] & isBlockB;
        }
    }

    hashObj->begin(mdState);

    if (isSSLv3) {
        /* We repurpose |hmacPad| to contain the SSLv3 pad2 block. */
        for (i = 0; i < sslv3PadLen; i++)
            hmacPad[i] = 0x5c;

        hashObj->update(mdState, macSecret, macSecretLen);
        hashObj->update(mdState, hmacPad, sslv3PadLen);
        hashObj->update(mdState, macOut, mdSize);
    } else {
        /* Complete the HMAC in the standard manner. */
        for (i = 0; i < mdBlockSize; i++)
            hmacPad[i] ^= 0x6a;

        hashObj->update(mdState, hmacPad, mdBlockSize);
        hashObj->update(mdState, macOut, mdSize);
    }

    hashObj->end(mdState, mdOut, mdOutLen, mdOutMax);
    hashObj->destroy(mdState, PR_TRUE);

    return SECSuccess;
}

SECStatus
HMAC_ConstantTime(
    unsigned char *result,
    unsigned int *resultLen,
    unsigned int maxResultLen,
    const SECHashObject *hashObj,
    const unsigned char *secret,
    unsigned int secretLen,
    const unsigned char *header,
    unsigned int headerLen,
    const unsigned char *body,
    unsigned int bodyLen,
    unsigned int bodyTotalLen)
{
    if (hashObj->end_raw == NULL)
        return SECFailure;
    return MAC(result, resultLen, maxResultLen, hashObj, secret, secretLen,
               header, headerLen, body, bodyLen, bodyTotalLen,
               0 /* not SSLv3 */);
}

SECStatus
SSLv3_MAC_ConstantTime(
    unsigned char *result,
    unsigned int *resultLen,
    unsigned int maxResultLen,
    const SECHashObject *hashObj,
    const unsigned char *secret,
    unsigned int secretLen,
    const unsigned char *header,
    unsigned int headerLen,
    const unsigned char *body,
    unsigned int bodyLen,
    unsigned int bodyTotalLen)
{
    if (hashObj->end_raw == NULL)
        return SECFailure;
    return MAC(result, resultLen, maxResultLen, hashObj, secret, secretLen,
               header, headerLen, body, bodyLen, bodyTotalLen,
               1 /* SSLv3 */);
}
