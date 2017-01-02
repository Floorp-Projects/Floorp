/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Encryption/decryption routines for CMS implementation, none of which are exported.
 */

#include "cmslocal.h"

#include "secoid.h"
#include "secitem.h"
#include "pk11func.h"
#include "secerr.h"
#include "secpkcs5.h"

/*
 * -------------------------------------------------------------------
 * Cipher stuff.
 */

typedef SECStatus (*nss_cms_cipher_function)(void *, unsigned char *, unsigned int *,
                                             unsigned int, const unsigned char *, unsigned int);
typedef SECStatus (*nss_cms_cipher_destroy)(void *, PRBool);

#define BLOCK_SIZE 4096

struct NSSCMSCipherContextStr {
    void *cx; /* PK11 cipher context */
    nss_cms_cipher_function doit;
    nss_cms_cipher_destroy destroy;
    PRBool encrypt; /* encrypt / decrypt switch */
    int block_size; /* block & pad sizes for cipher */
    int pad_size;
    int pending_count;                     /* pending data (not yet en/decrypted */
    unsigned char pending_buf[BLOCK_SIZE]; /* because of blocking */
};

/*
 * NSS_CMSCipherContext_StartDecrypt - create a cipher context to do decryption
 * based on the given bulk encryption key and algorithm identifier (which
 * may include an iv).
 *
 * XXX Once both are working, it might be nice to combine this and the
 * function below (for starting up encryption) into one routine, and just
 * have two simple cover functions which call it.
 */
NSSCMSCipherContext *
NSS_CMSCipherContext_StartDecrypt(PK11SymKey *key, SECAlgorithmID *algid)
{
    NSSCMSCipherContext *cc;
    void *ciphercx;
    CK_MECHANISM_TYPE cryptoMechType;
    PK11SlotInfo *slot;
    SECOidTag algtag;
    SECItem *param = NULL;

    algtag = SECOID_GetAlgorithmTag(algid);

    /* set param and mechanism */
    if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
        SECItem *pwitem;

        pwitem = PK11_GetSymKeyUserData(key);
        if (!pwitem)
            return NULL;

        cryptoMechType = PK11_GetPBECryptoMechanism(algid, &param, pwitem);
        if (cryptoMechType == CKM_INVALID_MECHANISM) {
            SECITEM_FreeItem(param, PR_TRUE);
            return NULL;
        }

    } else {
        cryptoMechType = PK11_AlgtagToMechanism(algtag);
        if ((param = PK11_ParamFromAlgid(algid)) == NULL)
            return NULL;
    }

    cc = (NSSCMSCipherContext *)PORT_ZAlloc(sizeof(NSSCMSCipherContext));
    if (cc == NULL) {
        SECITEM_FreeItem(param, PR_TRUE);
        return NULL;
    }

    /* figure out pad and block sizes */
    cc->pad_size = PK11_GetBlockSize(cryptoMechType, param);
    slot = PK11_GetSlotFromKey(key);
    cc->block_size = PK11_IsHW(slot) ? BLOCK_SIZE : cc->pad_size;
    PK11_FreeSlot(slot);

    /* create PK11 cipher context */
    ciphercx = PK11_CreateContextBySymKey(cryptoMechType, CKA_DECRYPT,
                                          key, param);
    SECITEM_FreeItem(param, PR_TRUE);
    if (ciphercx == NULL) {
        PORT_Free(cc);
        return NULL;
    }

    cc->cx = ciphercx;
    cc->doit = (nss_cms_cipher_function)PK11_CipherOp;
    cc->destroy = (nss_cms_cipher_destroy)PK11_DestroyContext;
    cc->encrypt = PR_FALSE;
    cc->pending_count = 0;

    return cc;
}

/*
 * NSS_CMSCipherContext_StartEncrypt - create a cipher object to do encryption,
 * based on the given bulk encryption key and algorithm tag.  Fill in the
 * algorithm identifier (which may include an iv) appropriately.
 *
 * XXX Once both are working, it might be nice to combine this and the
 * function above (for starting up decryption) into one routine, and just
 * have two simple cover functions which call it.
 */
NSSCMSCipherContext *
NSS_CMSCipherContext_StartEncrypt(PLArenaPool *poolp, PK11SymKey *key, SECAlgorithmID *algid)
{
    NSSCMSCipherContext *cc;
    void *ciphercx = NULL;
    SECStatus rv;
    CK_MECHANISM_TYPE cryptoMechType;
    PK11SlotInfo *slot;
    SECItem *param = NULL;
    PRBool needToEncodeAlgid = PR_FALSE;
    SECOidTag algtag = SECOID_GetAlgorithmTag(algid);

    /* set param and mechanism */
    if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
        SECItem *pwitem;

        pwitem = PK11_GetSymKeyUserData(key);
        if (!pwitem)
            return NULL;

        cryptoMechType = PK11_GetPBECryptoMechanism(algid, &param, pwitem);
        if (cryptoMechType == CKM_INVALID_MECHANISM) {
            SECITEM_FreeItem(param, PR_TRUE);
            return NULL;
        }
    } else {
        cryptoMechType = PK11_AlgtagToMechanism(algtag);
        if ((param = PK11_GenerateNewParam(cryptoMechType, key)) == NULL)
            return NULL;
        needToEncodeAlgid = PR_TRUE;
    }

    cc = (NSSCMSCipherContext *)PORT_ZAlloc(sizeof(NSSCMSCipherContext));
    if (cc == NULL) {
        goto loser;
    }

    /* now find pad and block sizes for our mechanism */
    cc->pad_size = PK11_GetBlockSize(cryptoMechType, param);
    slot = PK11_GetSlotFromKey(key);
    cc->block_size = PK11_IsHW(slot) ? BLOCK_SIZE : cc->pad_size;
    PK11_FreeSlot(slot);

    /* and here we go, creating a PK11 cipher context */
    ciphercx = PK11_CreateContextBySymKey(cryptoMechType, CKA_ENCRYPT,
                                          key, param);
    if (ciphercx == NULL) {
        PORT_Free(cc);
        cc = NULL;
        goto loser;
    }

    /*
     * These are placed after the CreateContextBySymKey() because some
     * mechanisms have to generate their IVs from their card (i.e. FORTEZZA).
     * Don't move it from here.
     * XXX is that right? the purpose of this is to get the correct algid
     *     containing the IVs etc. for encoding. this means we need to set this up
     *     BEFORE encoding the algid in the contentInfo, right?
     */
    if (needToEncodeAlgid) {
        rv = PK11_ParamToAlgid(algtag, param, poolp, algid);
        if (rv != SECSuccess) {
            PORT_Free(cc);
            cc = NULL;
            goto loser;
        }
    }

    cc->cx = ciphercx;
    ciphercx = NULL;
    cc->doit = (nss_cms_cipher_function)PK11_CipherOp;
    cc->destroy = (nss_cms_cipher_destroy)PK11_DestroyContext;
    cc->encrypt = PR_TRUE;
    cc->pending_count = 0;

loser:
    SECITEM_FreeItem(param, PR_TRUE);
    if (ciphercx) {
        PK11_DestroyContext(ciphercx, PR_TRUE);
    }

    return cc;
}

void
NSS_CMSCipherContext_Destroy(NSSCMSCipherContext *cc)
{
    PORT_Assert(cc != NULL);
    if (cc == NULL)
        return;
    (*cc->destroy)(cc->cx, PR_TRUE);
    PORT_Free(cc);
}

/*
 * NSS_CMSCipherContext_DecryptLength - find the output length of the next call to decrypt.
 *
 * cc - the cipher context
 * input_len - number of bytes used as input
 * final - true if this is the final chunk of data
 *
 * Result can be used to perform memory allocations.  Note that the amount
 * is exactly accurate only when not doing a block cipher or when final
 * is false, otherwise it is an upper bound on the amount because until
 * we see the data we do not know how many padding bytes there are
 * (always between 1 and bsize).
 *
 * Note that this can return zero, which does not mean that the decrypt
 * operation can be skipped!  (It simply means that there are not enough
 * bytes to make up an entire block; the bytes will be reserved until
 * there are enough to encrypt/decrypt at least one block.)  However,
 * if zero is returned it *does* mean that no output buffer need be
 * passed in to the subsequent decrypt operation, as no output bytes
 * will be stored.
 */
unsigned int
NSS_CMSCipherContext_DecryptLength(NSSCMSCipherContext *cc, unsigned int input_len, PRBool final)
{
    int blocks, block_size;

    PORT_Assert(!cc->encrypt);

    block_size = cc->block_size;

    /*
     * If this is not a block cipher, then we always have the same
     * number of output bytes as we had input bytes.
     */
    if (block_size == 0)
        return input_len;

    /*
     * On the final call, we will always use up all of the pending
     * bytes plus all of the input bytes, *but*, there will be padding
     * at the end and we cannot predict how many bytes of padding we
     * will end up removing.  The amount given here is actually known
     * to be at least 1 byte too long (because we know we will have
     * at least 1 byte of padding), but seemed clearer/better to me.
     */
    if (final)
        return cc->pending_count + input_len;

    /*
     * Okay, this amount is exactly what we will output on the
     * next cipher operation.  We will always hang onto the last
     * 1 - block_size bytes for non-final operations.  That is,
     * we will do as many complete blocks as we can *except* the
     * last block (complete or partial).  (This is because until
     * we know we are at the end, we cannot know when to interpret
     * and removing the padding byte(s), which are guaranteed to
     * be there.)
     */
    blocks = (cc->pending_count + input_len - 1) / block_size;
    return blocks * block_size;
}

/*
 * NSS_CMSCipherContext_EncryptLength - find the output length of the next call to encrypt.
 *
 * cc - the cipher context
 * input_len - number of bytes used as input
 * final - true if this is the final chunk of data
 *
 * Result can be used to perform memory allocations.
 *
 * Note that this can return zero, which does not mean that the encrypt
 * operation can be skipped!  (It simply means that there are not enough
 * bytes to make up an entire block; the bytes will be reserved until
 * there are enough to encrypt/decrypt at least one block.)  However,
 * if zero is returned it *does* mean that no output buffer need be
 * passed in to the subsequent encrypt operation, as no output bytes
 * will be stored.
 */
unsigned int
NSS_CMSCipherContext_EncryptLength(NSSCMSCipherContext *cc, unsigned int input_len, PRBool final)
{
    int blocks, block_size;
    int pad_size;

    PORT_Assert(cc->encrypt);

    block_size = cc->block_size;
    pad_size = cc->pad_size;

    /*
     * If this is not a block cipher, then we always have the same
     * number of output bytes as we had input bytes.
     */
    if (block_size == 0)
        return input_len;

    /*
     * On the final call, we only send out what we need for
     * remaining bytes plus the padding.  (There is always padding,
     * so even if we have an exact number of blocks as input, we
     * will add another full block that is just padding.)
     */
    if (final) {
        if (pad_size == 0) {
            return cc->pending_count + input_len;
        } else {
            blocks = (cc->pending_count + input_len) / pad_size;
            blocks++;
            return blocks * pad_size;
        }
    }

    /*
     * Now, count the number of complete blocks of data we have.
     */
    blocks = (cc->pending_count + input_len) / block_size;

    return blocks * block_size;
}

/*
 * NSS_CMSCipherContext_Decrypt - do the decryption
 *
 * cc - the cipher context
 * output - buffer for decrypted result bytes
 * output_len_p - number of bytes in output
 * max_output_len - upper bound on bytes to put into output
 * input - pointer to input bytes
 * input_len - number of input bytes
 * final - true if this is the final chunk of data
 *
 * Decrypts a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the decrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "cc" is the return value from NSS_CMSCipher_StartDecrypt.
 * When "final" is true, this is the last of the data to be decrypted.
 *
 * This is much more complicated than it sounds when the cipher is
 * a block-type, meaning that the decryption function will only
 * operate on whole blocks.  But our caller is operating stream-wise,
 * and can pass in any number of bytes.  So we need to keep track
 * of block boundaries.  We save excess bytes between calls in "cc".
 * We also need to determine which bytes are padding, and remove
 * them from the output.  We can only do this step when we know we
 * have the final block of data.  PKCS #7 specifies that the padding
 * used for a block cipher is a string of bytes, each of whose value is
 * the same as the length of the padding, and that all data is padded.
 * (Even data that starts out with an exact multiple of blocks gets
 * added to it another block, all of which is padding.)
 */
SECStatus
NSS_CMSCipherContext_Decrypt(NSSCMSCipherContext *cc, unsigned char *output,
                             unsigned int *output_len_p, unsigned int max_output_len,
                             const unsigned char *input, unsigned int input_len,
                             PRBool final)
{
    unsigned int blocks, bsize, pcount, padsize;
    unsigned int max_needed, ifraglen, ofraglen, output_len;
    unsigned char *pbuf;
    SECStatus rv;

    PORT_Assert(!cc->encrypt);

    /*
     * Check that we have enough room for the output.  Our caller should
     * already handle this; failure is really an internal error (i.e. bug).
     */
    max_needed = NSS_CMSCipherContext_DecryptLength(cc, input_len, final);
    PORT_Assert(max_output_len >= max_needed);
    if (max_output_len < max_needed) {
        /* PORT_SetError (XXX); */
        return SECFailure;
    }

    /*
     * hardware encryption does not like small decryption sizes here, so we
     * allow both blocking and padding.
     */
    bsize = cc->block_size;
    padsize = cc->pad_size;

    /*
     * When no blocking or padding work to do, we can simply call the
     * cipher function and we are done.
     */
    if (bsize == 0) {
        return (*cc->doit)(cc->cx, output, output_len_p, max_output_len,
                           input, input_len);
    }

    pcount = cc->pending_count;
    pbuf = cc->pending_buf;

    output_len = 0;

    if (pcount) {
        /*
         * Try to fill in an entire block, starting with the bytes
         * we already have saved away.
         */
        while (input_len && pcount < bsize) {
            pbuf[pcount++] = *input++;
            input_len--;
        }
        /*
         * If we have at most a whole block and this is not our last call,
         * then we are done for now.  (We do not try to decrypt a lone
         * single block because we cannot interpret the padding bytes
         * until we know we are handling the very last block of all input.)
         */
        if (input_len == 0 && !final) {
            cc->pending_count = pcount;
            if (output_len_p)
                *output_len_p = 0;
            return SECSuccess;
        }
        /*
         * Given the logic above, we expect to have a full block by now.
         * If we do not, there is something wrong, either with our own
         * logic or with (length of) the data given to us.
         */
        if ((padsize != 0) && (pcount % padsize) != 0) {
            PORT_Assert(final);
            PORT_SetError(SEC_ERROR_BAD_DATA);
            return SECFailure;
        }
        /*
         * Decrypt the block.
         */
        rv = (*cc->doit)(cc->cx, output, &ofraglen, max_output_len,
                         pbuf, pcount);
        if (rv != SECSuccess)
            return rv;

        /*
         * For now anyway, all of our ciphers have the same number of
         * bytes of output as they do input.  If this ever becomes untrue,
         * then NSS_CMSCipherContext_DecryptLength needs to be made smarter!
         */
        PORT_Assert(ofraglen == pcount);

        /*
         * Account for the bytes now in output.
         */
        max_output_len -= ofraglen;
        output_len += ofraglen;
        output += ofraglen;
    }

    /*
     * If this is our last call, we expect to have an exact number of
     * blocks left to be decrypted; we will decrypt them all.
     *
     * If not our last call, we always save between 1 and bsize bytes
     * until next time.  (We must do this because we cannot be sure
     * that none of the decrypted bytes are padding bytes until we
     * have at least another whole block of data.  You cannot tell by
     * looking -- the data could be anything -- you can only tell by
     * context, knowing you are looking at the last block.)  We could
     * decrypt a whole block now but it is easier if we just treat it
     * the same way we treat partial block bytes.
     */
    if (final) {
        if (padsize) {
            blocks = input_len / padsize;
            ifraglen = blocks * padsize;
        } else
            ifraglen = input_len;
        PORT_Assert(ifraglen == input_len);

        if (ifraglen != input_len) {
            PORT_SetError(SEC_ERROR_BAD_DATA);
            return SECFailure;
        }
    } else {
        blocks = (input_len - 1) / bsize;
        ifraglen = blocks * bsize;
        PORT_Assert(ifraglen < input_len);

        pcount = input_len - ifraglen;
        PORT_Memcpy(pbuf, input + ifraglen, pcount);
        cc->pending_count = pcount;
    }

    if (ifraglen) {
        rv = (*cc->doit)(cc->cx, output, &ofraglen, max_output_len,
                         input, ifraglen);
        if (rv != SECSuccess)
            return rv;

        /*
         * For now anyway, all of our ciphers have the same number of
         * bytes of output as they do input.  If this ever becomes untrue,
         * then sec_PKCS7DecryptLength needs to be made smarter!
         */
        PORT_Assert(ifraglen == ofraglen);
        if (ifraglen != ofraglen) {
            PORT_SetError(SEC_ERROR_BAD_DATA);
            return SECFailure;
        }

        output_len += ofraglen;
    } else {
        ofraglen = 0;
    }

    /*
     * If we just did our very last block, "remove" the padding by
     * adjusting the output length.
     */
    if (final && (padsize != 0)) {
        unsigned int padlen = *(output + ofraglen - 1);

        if (padlen == 0 || padlen > padsize) {
            PORT_SetError(SEC_ERROR_BAD_DATA);
            return SECFailure;
        }
        output_len -= padlen;
    }

    PORT_Assert(output_len_p != NULL || output_len == 0);
    if (output_len_p != NULL)
        *output_len_p = output_len;

    return SECSuccess;
}

/*
 * NSS_CMSCipherContext_Encrypt - do the encryption
 *
 * cc - the cipher context
 * output - buffer for decrypted result bytes
 * output_len_p - number of bytes in output
 * max_output_len - upper bound on bytes to put into output
 * input - pointer to input bytes
 * input_len - number of input bytes
 * final - true if this is the final chunk of data
 *
 * Encrypts a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the encrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "cc" is the return value from NSS_CMSCipher_StartEncrypt.
 * When "final" is true, this is the last of the data to be encrypted.
 *
 * This is much more complicated than it sounds when the cipher is
 * a block-type, meaning that the encryption function will only
 * operate on whole blocks.  But our caller is operating stream-wise,
 * and can pass in any number of bytes.  So we need to keep track
 * of block boundaries.  We save excess bytes between calls in "cc".
 * We also need to add padding bytes at the end.  PKCS #7 specifies
 * that the padding used for a block cipher is a string of bytes,
 * each of whose value is the same as the length of the padding,
 * and that all data is padded.  (Even data that starts out with
 * an exact multiple of blocks gets added to it another block,
 * all of which is padding.)
 *
 * XXX I would kind of like to combine this with the function above
 * which does decryption, since they have a lot in common.  But the
 * tricky parts about padding and filling blocks would be much
 * harder to read that way, so I left them separate.  At least for
 * now until it is clear that they are right.
 */
SECStatus
NSS_CMSCipherContext_Encrypt(NSSCMSCipherContext *cc, unsigned char *output,
                             unsigned int *output_len_p, unsigned int max_output_len,
                             const unsigned char *input, unsigned int input_len,
                             PRBool final)
{
    int blocks, bsize, padlen, pcount, padsize;
    unsigned int max_needed, ifraglen, ofraglen, output_len;
    unsigned char *pbuf;
    SECStatus rv;

    PORT_Assert(cc->encrypt);

    /*
     * Check that we have enough room for the output.  Our caller should
     * already handle this; failure is really an internal error (i.e. bug).
     */
    max_needed = NSS_CMSCipherContext_EncryptLength(cc, input_len, final);
    PORT_Assert(max_output_len >= max_needed);
    if (max_output_len < max_needed) {
        /* PORT_SetError (XXX); */
        return SECFailure;
    }

    bsize = cc->block_size;
    padsize = cc->pad_size;

    /*
     * When no blocking and padding work to do, we can simply call the
     * cipher function and we are done.
     */
    if (bsize == 0) {
        return (*cc->doit)(cc->cx, output, output_len_p, max_output_len,
                           input, input_len);
    }

    pcount = cc->pending_count;
    pbuf = cc->pending_buf;

    output_len = 0;

    if (pcount) {
        /*
         * Try to fill in an entire block, starting with the bytes
         * we already have saved away.
         */
        while (input_len && pcount < bsize) {
            pbuf[pcount++] = *input++;
            input_len--;
        }
        /*
         * If we do not have a full block and we know we will be
         * called again, then we are done for now.
         */
        if (pcount < bsize && !final) {
            cc->pending_count = pcount;
            if (output_len_p != NULL)
                *output_len_p = 0;
            return SECSuccess;
        }
        /*
         * If we have a whole block available, encrypt it.
         */
        if ((padsize == 0) || (pcount % padsize) == 0) {
            rv = (*cc->doit)(cc->cx, output, &ofraglen, max_output_len,
                             pbuf, pcount);
            if (rv != SECSuccess)
                return rv;

            /*
             * For now anyway, all of our ciphers have the same number of
             * bytes of output as they do input.  If this ever becomes untrue,
             * then sec_PKCS7EncryptLength needs to be made smarter!
             */
            PORT_Assert(ofraglen == pcount);

            /*
             * Account for the bytes now in output.
             */
            max_output_len -= ofraglen;
            output_len += ofraglen;
            output += ofraglen;

            pcount = 0;
        }
    }

    if (input_len) {
        PORT_Assert(pcount == 0);

        blocks = input_len / bsize;
        ifraglen = blocks * bsize;

        if (ifraglen) {
            rv = (*cc->doit)(cc->cx, output, &ofraglen, max_output_len,
                             input, ifraglen);
            if (rv != SECSuccess)
                return rv;

            /*
             * For now anyway, all of our ciphers have the same number of
             * bytes of output as they do input.  If this ever becomes untrue,
             * then sec_PKCS7EncryptLength needs to be made smarter!
             */
            PORT_Assert(ifraglen == ofraglen);

            max_output_len -= ofraglen;
            output_len += ofraglen;
            output += ofraglen;
        }

        pcount = input_len - ifraglen;
        PORT_Assert(pcount < bsize);
        if (pcount)
            PORT_Memcpy(pbuf, input + ifraglen, pcount);
    }

    if (final) {
        if (padsize <= 0) {
            padlen = 0;
        } else {
            padlen = padsize - (pcount % padsize);
            PORT_Memset(pbuf + pcount, padlen, padlen);
        }
        rv = (*cc->doit)(cc->cx, output, &ofraglen, max_output_len,
                         pbuf, pcount + padlen);
        if (rv != SECSuccess)
            return rv;

        /*
         * For now anyway, all of our ciphers have the same number of
         * bytes of output as they do input.  If this ever becomes untrue,
         * then sec_PKCS7EncryptLength needs to be made smarter!
         */
        PORT_Assert(ofraglen == (pcount + padlen));
        output_len += ofraglen;
    } else {
        cc->pending_count = pcount;
    }

    PORT_Assert(output_len_p != NULL || output_len == 0);
    if (output_len_p != NULL)
        *output_len_p = output_len;

    return SECSuccess;
}
