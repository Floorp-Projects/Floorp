/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PKCS7 decoding, verification.
 */

#include "p7local.h"

#include "cert.h"
/* XXX do not want to have to include */
#include "certdb.h" /* certdb.h -- the trust stuff needed by */
                    /* the add certificate code needs to get */
                    /* rewritten/abstracted and then this */
                    /* include should be removed! */
/*#include "cdbhdl.h" */
#include "cryptohi.h"
#include "keyhi.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"
#include "sechash.h" /* for HASH_GetHashObject() */
#include "secder.h"
#include "secpkcs5.h"

struct sec_pkcs7_decoder_worker {
    int depth;
    int digcnt;
    void **digcxs;
    const SECHashObject **digobjs;
    sec_PKCS7CipherObject *decryptobj;
    PRBool saw_contents;
};

struct SEC_PKCS7DecoderContextStr {
    SEC_ASN1DecoderContext *dcx;
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7DecoderContentCallback cb;
    void *cb_arg;
    SECKEYGetPasswordKey pwfn;
    void *pwfn_arg;
    struct sec_pkcs7_decoder_worker worker;
    PLArenaPool *tmp_poolp;
    int error;
    SEC_PKCS7GetDecryptKeyCallback dkcb;
    void *dkcb_arg;
    SEC_PKCS7DecryptionAllowedCallback decrypt_allowed_cb;
};

/*
 * Handle one worker, decrypting and digesting the data as necessary.
 *
 * XXX If/when we support nested contents, this probably needs to be
 * revised somewhat to get passed the content-info (which unfortunately
 * can be two different types depending on whether it is encrypted or not)
 * corresponding to the given worker.
 */
static void
sec_pkcs7_decoder_work_data(SEC_PKCS7DecoderContext *p7dcx,
                            struct sec_pkcs7_decoder_worker *worker,
                            const unsigned char *data, unsigned long len,
                            PRBool final)
{
    unsigned char *buf = NULL;
    SECStatus rv;
    int i;

    /*
     * We should really have data to process, or we should be trying
     * to finish/flush the last block.  (This is an overly paranoid
     * check since all callers are in this file and simple inspection
     * proves they do it right.  But it could find a bug in future
     * modifications/development, that is why it is here.)
     */
    PORT_Assert((data != NULL && len) || final);

    /*
     * Decrypt this chunk.
     *
     * XXX If we get an error, we do not want to do the digest or callback,
     * but we want to keep decoding.  Or maybe we want to stop decoding
     * altogether if there is a callback, because obviously we are not
     * sending the data back and they want to know that.
     */
    if (worker->decryptobj != NULL) {
        /* XXX the following lengths should all be longs? */
        unsigned int inlen;  /* length of data being decrypted */
        unsigned int outlen; /* length of decrypted data */
        unsigned int buflen; /* length available for decrypted data */
        SECItem *plain;

        inlen = len;
        buflen = sec_PKCS7DecryptLength(worker->decryptobj, inlen, final);
        if (buflen == 0) {
            if (inlen == 0) /* no input and no output */
                return;
            /*
             * No output is expected, but the input data may be buffered
             * so we still have to call Decrypt.
             */
            rv = sec_PKCS7Decrypt(worker->decryptobj, NULL, NULL, 0,
                                  data, inlen, final);
            if (rv != SECSuccess) {
                p7dcx->error = PORT_GetError();
                return; /* XXX indicate error? */
            }
            return;
        }

        if (p7dcx->cb != NULL) {
            buf = (unsigned char *)PORT_Alloc(buflen);
            plain = NULL;
        } else {
            unsigned long oldlen;

            /*
             * XXX This assumes one level of content only.
             * See comment above about nested content types.
             * XXX Also, it should work for signedAndEnvelopedData, too!
             */
            plain = &(p7dcx->cinfo->content.envelopedData->encContentInfo.plainContent);

            oldlen = plain->len;
            if (oldlen == 0) {
                buf = (unsigned char *)PORT_ArenaAlloc(p7dcx->cinfo->poolp,
                                                       buflen);
            } else {
                buf = (unsigned char *)PORT_ArenaGrow(p7dcx->cinfo->poolp,
                                                      plain->data,
                                                      oldlen, oldlen + buflen);
                if (buf != NULL)
                    buf += oldlen;
            }
            plain->data = buf;
        }
        if (buf == NULL) {
            p7dcx->error = SEC_ERROR_NO_MEMORY;
            return; /* XXX indicate error? */
        }
        rv = sec_PKCS7Decrypt(worker->decryptobj, buf, &outlen, buflen,
                              data, inlen, final);
        if (rv != SECSuccess) {
            p7dcx->error = PORT_GetError();
            return; /* XXX indicate error? */
        }
        if (plain != NULL) {
            PORT_Assert(final || outlen == buflen);
            plain->len += outlen;
        }
        data = buf;
        len = outlen;
    }

    /*
     * Update the running digests.
     */
    if (len) {
        for (i = 0; i < worker->digcnt; i++) {
            (*worker->digobjs[i]->update)(worker->digcxs[i], data, len);
        }
    }

    /*
     * Pass back the contents bytes, and free the temporary buffer.
     */
    if (p7dcx->cb != NULL) {
        if (len)
            (*p7dcx->cb)(p7dcx->cb_arg, (const char *)data, len);
        if (worker->decryptobj != NULL) {
            PORT_Assert(buf != NULL);
            PORT_Free(buf);
        }
    }
}

static void
sec_pkcs7_decoder_filter(void *arg, const char *data, unsigned long len,
                         int depth, SEC_ASN1EncodingPart data_kind)
{
    SEC_PKCS7DecoderContext *p7dcx;
    struct sec_pkcs7_decoder_worker *worker;

    /*
     * Since we do not handle any nested contents, the only bytes we
     * are really interested in are the actual contents bytes (not
     * the identifier, length, or end-of-contents bytes).  If we were
     * handling nested types we would probably need to do something
     * smarter based on depth and data_kind.
     */
    if (data_kind != SEC_ASN1_Contents)
        return;

    /*
     * The ASN.1 decoder should not even call us with a length of 0.
     * Just being paranoid.
     */
    PORT_Assert(len);
    if (len == 0)
        return;

    p7dcx = (SEC_PKCS7DecoderContext *)arg;

    /*
     * Handling nested contents would mean that there is a chain
     * of workers -- one per each level of content.  The following
     * would start with the first worker and loop over them.
     */
    worker = &(p7dcx->worker);

    worker->saw_contents = PR_TRUE;

    sec_pkcs7_decoder_work_data(p7dcx, worker,
                                (const unsigned char *)data, len, PR_FALSE);
}

/*
 * Create digest contexts for each algorithm in "digestalgs".
 * No algorithms is not an error, we just do not do anything.
 * An error (like trouble allocating memory), marks the error
 * in "p7dcx" and returns SECFailure, which means that our caller
 * should just give up altogether.
 */
static SECStatus
sec_pkcs7_decoder_start_digests(SEC_PKCS7DecoderContext *p7dcx, int depth,
                                SECAlgorithmID **digestalgs)
{
    int i, digcnt;

    if (digestalgs == NULL)
        return SECSuccess;

    /*
     * Count the algorithms.
     */
    digcnt = 0;
    while (digestalgs[digcnt] != NULL)
        digcnt++;

    /*
     * No algorithms means no work to do.
     * Just act as if there were no algorithms specified.
     */
    if (digcnt == 0)
        return SECSuccess;

    p7dcx->worker.digcxs = (void **)PORT_ArenaAlloc(p7dcx->tmp_poolp,
                                                    digcnt * sizeof(void *));
    p7dcx->worker.digobjs = (const SECHashObject **)PORT_ArenaAlloc(p7dcx->tmp_poolp,
                                                                    digcnt * sizeof(SECHashObject *));
    if (p7dcx->worker.digcxs == NULL || p7dcx->worker.digobjs == NULL) {
        p7dcx->error = SEC_ERROR_NO_MEMORY;
        return SECFailure;
    }

    p7dcx->worker.depth = depth;
    p7dcx->worker.digcnt = 0;

    /*
     * Create a digest context for each algorithm.
     */
    for (i = 0; i < digcnt; i++) {
        SECAlgorithmID *algid = digestalgs[i];
        SECOidTag oidTag = SECOID_FindOIDTag(&(algid->algorithm));
        const SECHashObject *digobj = HASH_GetHashObjectByOidTag(oidTag);
        void *digcx;

        /*
         * Skip any algorithm we do not even recognize; obviously,
         * this could be a problem, but if it is critical then the
         * result will just be that the signature does not verify.
         * We do not necessarily want to error out here, because
         * the particular algorithm may not actually be important,
         * but we cannot know that until later.
         */
        if (digobj == NULL) {
            p7dcx->worker.digcnt--;
            continue;
        }

        digcx = (*digobj->create)();
        if (digcx != NULL) {
            (*digobj->begin)(digcx);
            p7dcx->worker.digobjs[p7dcx->worker.digcnt] = digobj;
            p7dcx->worker.digcxs[p7dcx->worker.digcnt] = digcx;
            p7dcx->worker.digcnt++;
        }
    }

    if (p7dcx->worker.digcnt != 0)
        SEC_ASN1DecoderSetFilterProc(p7dcx->dcx,
                                     sec_pkcs7_decoder_filter,
                                     p7dcx,
                                     (PRBool)(p7dcx->cb != NULL));
    return SECSuccess;
}

/*
 * Close out all of the digest contexts, storing the results in "digestsp".
 */
static SECStatus
sec_pkcs7_decoder_finish_digests(SEC_PKCS7DecoderContext *p7dcx,
                                 PLArenaPool *poolp,
                                 SECItem ***digestsp)
{
    struct sec_pkcs7_decoder_worker *worker;
    const SECHashObject *digobj;
    void *digcx;
    SECItem **digests, *digest;
    int i;
    void *mark;

    /*
     * XXX Handling nested contents would mean that there is a chain
     * of workers -- one per each level of content.  The following
     * would want to find the last worker in the chain.
     */
    worker = &(p7dcx->worker);

    /*
     * If no digests, then we have nothing to do.
     */
    if (worker->digcnt == 0)
        return SECSuccess;

    /*
     * No matter what happens after this, we want to stop filtering.
     * XXX If we handle nested contents, we only want to stop filtering
     * if we are finishing off the *last* worker.
     */
    SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);

    /*
     * If we ended up with no contents, just destroy each
     * digest context -- they are meaningless and potentially
     * confusing, because their presence would imply some content
     * was digested.
     */
    if (!worker->saw_contents) {
        for (i = 0; i < worker->digcnt; i++) {
            digcx = worker->digcxs[i];
            digobj = worker->digobjs[i];
            (*digobj->destroy)(digcx, PR_TRUE);
        }
        return SECSuccess;
    }

    mark = PORT_ArenaMark(poolp);

    /*
     * Close out each digest context, saving digest away.
     */
    digests =
        (SECItem **)PORT_ArenaAlloc(poolp, (worker->digcnt + 1) * sizeof(SECItem *));
    digest = (SECItem *)PORT_ArenaAlloc(poolp, worker->digcnt * sizeof(SECItem));
    if (digests == NULL || digest == NULL) {
        p7dcx->error = PORT_GetError();
        PORT_ArenaRelease(poolp, mark);
        return SECFailure;
    }

    for (i = 0; i < worker->digcnt; i++, digest++) {
        digcx = worker->digcxs[i];
        digobj = worker->digobjs[i];

        digest->data = (unsigned char *)PORT_ArenaAlloc(poolp, digobj->length);
        if (digest->data == NULL) {
            p7dcx->error = PORT_GetError();
            PORT_ArenaRelease(poolp, mark);
            return SECFailure;
        }

        digest->len = digobj->length;
        (*digobj->end)(digcx, digest->data, &(digest->len), digest->len);
        (*digobj->destroy)(digcx, PR_TRUE);

        digests[i] = digest;
    }
    digests[i] = NULL;
    *digestsp = digests;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
}

/*
 * XXX Need comment explaining following helper function (which is used
 * by sec_pkcs7_decoder_start_decrypt).
 */

static PK11SymKey *
sec_pkcs7_decoder_get_recipient_key(SEC_PKCS7DecoderContext *p7dcx,
                                    SEC_PKCS7RecipientInfo **recipientinfos,
                                    SEC_PKCS7EncryptedContentInfo *enccinfo)
{
    SEC_PKCS7RecipientInfo *ri;
    CERTCertificate *cert = NULL;
    SECKEYPrivateKey *privkey = NULL;
    PK11SymKey *bulkkey = NULL;
    SECOidTag keyalgtag, bulkalgtag, encalgtag;
    PK11SlotInfo *slot = NULL;

    if (recipientinfos == NULL || recipientinfos[0] == NULL) {
        p7dcx->error = SEC_ERROR_NOT_A_RECIPIENT;
        goto no_key_found;
    }

    cert = PK11_FindCertAndKeyByRecipientList(&slot, recipientinfos, &ri,
                                              &privkey, p7dcx->pwfn_arg);
    if (cert == NULL) {
        p7dcx->error = SEC_ERROR_NOT_A_RECIPIENT;
        goto no_key_found;
    }

    ri->cert = cert; /* so we can find it later */
    PORT_Assert(privkey != NULL);

    keyalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    encalgtag = SECOID_GetAlgorithmTag(&(ri->keyEncAlg));
    if (keyalgtag != encalgtag) {
        p7dcx->error = SEC_ERROR_PKCS7_KEYALG_MISMATCH;
        goto no_key_found;
    }
    bulkalgtag = SECOID_GetAlgorithmTag(&(enccinfo->contentEncAlg));

    switch (encalgtag) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            bulkkey = PK11_PubUnwrapSymKey(privkey, &ri->encKey,
                                           PK11_AlgtagToMechanism(bulkalgtag),
                                           CKA_DECRYPT, 0);
            if (bulkkey == NULL) {
                p7dcx->error = PORT_GetError();
                PORT_SetError(0);
                goto no_key_found;
            }
            break;
        default:
            p7dcx->error = SEC_ERROR_UNSUPPORTED_KEYALG;
            break;
    }

no_key_found:
    if (privkey != NULL)
        SECKEY_DestroyPrivateKey(privkey);
    if (slot != NULL)
        PK11_FreeSlot(slot);

    return bulkkey;
}

/*
 * XXX The following comment is old -- the function used to only handle
 * EnvelopedData or SignedAndEnvelopedData but now handles EncryptedData
 * as well (and it had all of the code of the helper function above
 * built into it), though the comment was left as is.  Fix it...
 *
 * We are just about to decode the content of an EnvelopedData.
 * Set up a decryption context so we can decrypt as we go.
 * Presumably we are one of the recipients listed in "recipientinfos".
 * (XXX And if we are not, or if we have trouble, what should we do?
 *  It would be nice to let the decoding still work.  Maybe it should
 *  be an error if there is a content callback, but not an error otherwise?)
 * The encryption key and related information can be found in "enccinfo".
 */
static SECStatus
sec_pkcs7_decoder_start_decrypt(SEC_PKCS7DecoderContext *p7dcx, int depth,
                                SEC_PKCS7RecipientInfo **recipientinfos,
                                SEC_PKCS7EncryptedContentInfo *enccinfo,
                                PK11SymKey **copy_key_for_signature)
{
    PK11SymKey *bulkkey = NULL;
    sec_PKCS7CipherObject *decryptobj;

    /*
     * If a callback is supplied to retrieve the encryption key,
     * for instance, for Encrypted Content infos, then retrieve
     * the bulkkey from the callback.  Otherwise, assume that
     * we are processing Enveloped or SignedAndEnveloped data
     * content infos.
     *
     * XXX Put an assert here?
     */
    if (SEC_PKCS7ContentType(p7dcx->cinfo) == SEC_OID_PKCS7_ENCRYPTED_DATA) {
        if (p7dcx->dkcb != NULL) {
            bulkkey = (*p7dcx->dkcb)(p7dcx->dkcb_arg,
                                     &(enccinfo->contentEncAlg));
        }
        enccinfo->keysize = 0;
    } else {
        bulkkey = sec_pkcs7_decoder_get_recipient_key(p7dcx, recipientinfos,
                                                      enccinfo);
        if (bulkkey == NULL)
            goto no_decryption;
        enccinfo->keysize = PK11_GetKeyStrength(bulkkey,
                                                &(enccinfo->contentEncAlg));
    }

    /*
     * XXX I think following should set error in p7dcx and clear set error
     * (as used to be done here, or as is done in get_receipient_key above.
     */
    if (bulkkey == NULL) {
        goto no_decryption;
    }

    /*
     * We want to make sure decryption is allowed.  This is done via
     * a callback specified in SEC_PKCS7DecoderStart().
     */
    if (p7dcx->decrypt_allowed_cb) {
        if ((*p7dcx->decrypt_allowed_cb)(&(enccinfo->contentEncAlg),
                                         bulkkey) == PR_FALSE) {
            p7dcx->error = SEC_ERROR_DECRYPTION_DISALLOWED;
            goto no_decryption;
        }
    } else {
        p7dcx->error = SEC_ERROR_DECRYPTION_DISALLOWED;
        goto no_decryption;
    }

    /*
     * When decrypting a signedAndEnvelopedData, the signature also has
     * to be decrypted with the bulk encryption key; to avoid having to
     * get it all over again later (and do another potentially expensive
     * RSA operation), copy it for later signature verification to use.
     */
    if (copy_key_for_signature != NULL)
        *copy_key_for_signature = PK11_ReferenceSymKey(bulkkey);

    /*
     * Now we have the bulk encryption key (in bulkkey) and the
     * the algorithm (in enccinfo->contentEncAlg).  Using those,
     * create a decryption context.
     */
    decryptobj = sec_PKCS7CreateDecryptObject(bulkkey,
                                              &(enccinfo->contentEncAlg));

    /*
     * We are done with (this) bulkkey now.
     */
    PK11_FreeSymKey(bulkkey);

    if (decryptobj == NULL) {
        p7dcx->error = PORT_GetError();
        PORT_SetError(0);
        goto no_decryption;
    }

    SEC_ASN1DecoderSetFilterProc(p7dcx->dcx,
                                 sec_pkcs7_decoder_filter,
                                 p7dcx,
                                 (PRBool)(p7dcx->cb != NULL));

    p7dcx->worker.depth = depth;
    p7dcx->worker.decryptobj = decryptobj;

    return SECSuccess;

no_decryption:
    PK11_FreeSymKey(bulkkey);
    /*
     * For some reason (error set already, if appropriate), we cannot
     * decrypt the content.  I am not sure what exactly is the right
     * thing to do here; in some cases we want to just stop, and in
     * others we want to let the decoding finish even though we cannot
     * decrypt the content.  My current thinking is that if the caller
     * set up a content callback, then they are really interested in
     * getting (decrypted) content, and if they cannot they will want
     * to know about it.  However, if no callback was specified, then
     * maybe it is not important that the decryption failed.
     */
    if (p7dcx->cb != NULL)
        return SECFailure;
    else
        return SECSuccess; /* Let the decoding continue. */
}

static SECStatus
sec_pkcs7_decoder_finish_decrypt(SEC_PKCS7DecoderContext *p7dcx,
                                 PLArenaPool *poolp,
                                 SEC_PKCS7EncryptedContentInfo *enccinfo)
{
    struct sec_pkcs7_decoder_worker *worker;

    /*
     * XXX Handling nested contents would mean that there is a chain
     * of workers -- one per each level of content.  The following
     * would want to find the last worker in the chain.
     */
    worker = &(p7dcx->worker);

    /*
     * If no decryption context, then we have nothing to do.
     */
    if (worker->decryptobj == NULL)
        return SECSuccess;

    /*
     * No matter what happens after this, we want to stop filtering.
     * XXX If we handle nested contents, we only want to stop filtering
     * if we are finishing off the *last* worker.
     */
    SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);

    /*
     * Handle the last block.
     */
    sec_pkcs7_decoder_work_data(p7dcx, worker, NULL, 0, PR_TRUE);

    /*
     * All done, destroy it.
     */
    sec_PKCS7DestroyDecryptObject(worker->decryptobj);
    worker->decryptobj = NULL;

    return SECSuccess;
}

static void
sec_pkcs7_decoder_notify(void *arg, PRBool before, void *dest, int depth)
{
    SEC_PKCS7DecoderContext *p7dcx;
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7SignedData *sigd;
    SEC_PKCS7EnvelopedData *envd;
    SEC_PKCS7SignedAndEnvelopedData *saed;
    SEC_PKCS7EncryptedData *encd;
    SEC_PKCS7DigestedData *digd;
    PRBool after;
    SECStatus rv;

    /*
     * Just to make the code easier to read, create an "after" variable
     * that is equivalent to "not before".
     * (This used to be just the statement "after = !before", but that
     * causes a warning on the mac; to avoid that, we do it the long way.)
     */
    if (before)
        after = PR_FALSE;
    else
        after = PR_TRUE;

    p7dcx = (SEC_PKCS7DecoderContext *)arg;
    if (!p7dcx) {
        return;
    }

    cinfo = p7dcx->cinfo;

    if (!cinfo) {
        return;
    }

    if (cinfo->contentTypeTag == NULL) {
        if (after && dest == &(cinfo->contentType))
            cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));
        return;
    }

    switch (cinfo->contentTypeTag->offset) {
        case SEC_OID_PKCS7_SIGNED_DATA:
            sigd = cinfo->content.signedData;
            if (sigd == NULL)
                break;

            if (sigd->contentInfo.contentTypeTag == NULL) {
                if (after && dest == &(sigd->contentInfo.contentType))
                    sigd->contentInfo.contentTypeTag =
                        SECOID_FindOID(&(sigd->contentInfo.contentType));
                break;
            }

            /*
             * We only set up a filtering digest if the content is
             * plain DATA; anything else needs more work because a
             * second pass is required to produce a DER encoding from
             * an input that can be BER encoded.  (This is a requirement
             * of PKCS7 that is unfortunate, but there you have it.)
             *
             * XXX Also, since we stop here if this is not DATA, the
             * inner content is not getting processed at all.  Someday
             * we may want to fix that.
             */
            if (sigd->contentInfo.contentTypeTag->offset != SEC_OID_PKCS7_DATA) {
                /* XXX Set an error in p7dcx->error */
                SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
                break;
            }

            /*
             * Just before the content, we want to set up a digest context
             * for each digest algorithm listed, and start a filter which
             * will run all of the contents bytes through that digest.
             */
            if (before && dest == &(sigd->contentInfo.content)) {
                rv = sec_pkcs7_decoder_start_digests(p7dcx, depth,
                                                     sigd->digestAlgorithms);
                if (rv != SECSuccess)
                    SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);

                break;
            }

            /*
             * XXX To handle nested types, here is where we would want
             * to check for inner boundaries that need handling.
             */

            /*
             * Are we done?
             */
            if (after && dest == &(sigd->contentInfo.content)) {
                /*
                 * Close out the digest contexts.  We ignore any error
                 * because we are stopping anyway; the error status left
                 * behind in p7dcx will be seen by outer functions.
                 */
                (void)sec_pkcs7_decoder_finish_digests(p7dcx, cinfo->poolp,
                                                       &(sigd->digests));

                /*
                 * XXX To handle nested contents, we would need to remove
                 * the worker from the chain (and free it).
                 */

                /*
                 * Stop notify.
                 */
                SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
            }
            break;

        case SEC_OID_PKCS7_ENVELOPED_DATA:
            envd = cinfo->content.envelopedData;
            if (envd == NULL)
                break;

            if (envd->encContentInfo.contentTypeTag == NULL) {
                if (after && dest == &(envd->encContentInfo.contentType))
                    envd->encContentInfo.contentTypeTag =
                        SECOID_FindOID(&(envd->encContentInfo.contentType));
                break;
            }

            /*
             * Just before the content, we want to set up a decryption
             * context, and start a filter which will run all of the
             * contents bytes through it to determine the plain content.
             */
            if (before && dest == &(envd->encContentInfo.encContent)) {
                rv = sec_pkcs7_decoder_start_decrypt(p7dcx, depth,
                                                     envd->recipientInfos,
                                                     &(envd->encContentInfo),
                                                     NULL);
                if (rv != SECSuccess)
                    SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);

                break;
            }

            /*
             * Are we done?
             */
            if (after && dest == &(envd->encContentInfo.encContent)) {
                /*
             * Close out the decryption context.  We ignore any error
             * because we are stopping anyway; the error status left
             * behind in p7dcx will be seen by outer functions.
             */
                (void)sec_pkcs7_decoder_finish_decrypt(p7dcx, cinfo->poolp,
                                                       &(envd->encContentInfo));

                /*
                 * XXX To handle nested contents, we would need to remove
                 * the worker from the chain (and free it).
                 */

                /*
                 * Stop notify.
                 */
                SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
            }
            break;

        case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
            saed = cinfo->content.signedAndEnvelopedData;
            if (saed == NULL)
                break;

            if (saed->encContentInfo.contentTypeTag == NULL) {
                if (after && dest == &(saed->encContentInfo.contentType))
                    saed->encContentInfo.contentTypeTag =
                        SECOID_FindOID(&(saed->encContentInfo.contentType));
                break;
            }

            /*
             * Just before the content, we want to set up a decryption
             * context *and* digest contexts, and start a filter which
             * will run all of the contents bytes through both.
             */
            if (before && dest == &(saed->encContentInfo.encContent)) {
                rv = sec_pkcs7_decoder_start_decrypt(p7dcx, depth,
                                                     saed->recipientInfos,
                                                     &(saed->encContentInfo),
                                                     &(saed->sigKey));
                if (rv == SECSuccess)
                    rv = sec_pkcs7_decoder_start_digests(p7dcx, depth,
                                                         saed->digestAlgorithms);
                if (rv != SECSuccess)
                    SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);

                break;
            }

            /*
             * Are we done?
             */
            if (after && dest == &(saed->encContentInfo.encContent)) {
                /*
                 * Close out the decryption and digests contexts.
                 * We ignore any errors because we are stopping anyway;
                 * the error status left behind in p7dcx will be seen by
                 * outer functions.
                 *
                 * Note that the decrypt stuff must be called first;
                 * it may have a last buffer to do which in turn has
                 * to be added to the digest.
                 */
                (void)sec_pkcs7_decoder_finish_decrypt(p7dcx, cinfo->poolp,
                                                       &(saed->encContentInfo));
                (void)sec_pkcs7_decoder_finish_digests(p7dcx, cinfo->poolp,
                                                       &(saed->digests));

                /*
                 * XXX To handle nested contents, we would need to remove
                 * the worker from the chain (and free it).
                 */

                /*
                 * Stop notify.
                 */
                SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
            }
            break;

        case SEC_OID_PKCS7_DIGESTED_DATA:
            digd = cinfo->content.digestedData;

            /*
             * XXX Want to do the digest or not?  Maybe future enhancement...
             */
            if (before && dest == &(digd->contentInfo.content.data)) {
                SEC_ASN1DecoderSetFilterProc(p7dcx->dcx, sec_pkcs7_decoder_filter,
                                             p7dcx,
                                             (PRBool)(p7dcx->cb != NULL));
                break;
            }

            /*
             * Are we done?
             */
            if (after && dest == &(digd->contentInfo.content.data)) {
                SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);
            }
            break;

        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            encd = cinfo->content.encryptedData;

            if (!encd) {
                break;
            }

            /*
             * XXX If the decryption key callback is set, we want to start
             * the decryption.  If the callback is not set, we will treat the
             * content as plain data, since we do not have the key.
             *
             * Is this the proper thing to do?
             */
            if (before && dest == &(encd->encContentInfo.encContent)) {
                /*
                 * Start the encryption process if the decryption key callback
                 * is present.  Otherwise, treat the content like plain data.
                 */
                rv = SECSuccess;
                if (p7dcx->dkcb != NULL) {
                    rv = sec_pkcs7_decoder_start_decrypt(p7dcx, depth, NULL,
                                                         &(encd->encContentInfo),
                                                         NULL);
                }

                if (rv != SECSuccess)
                    SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);

                break;
            }

            /*
             * Are we done?
             */
            if (after && dest == &(encd->encContentInfo.encContent)) {
                /*
                 * Close out the decryption context.  We ignore any error
                 * because we are stopping anyway; the error status left
                 * behind in p7dcx will be seen by outer functions.
                 */
                (void)sec_pkcs7_decoder_finish_decrypt(p7dcx, cinfo->poolp,
                                                       &(encd->encContentInfo));

                /*
                 * Stop notify.
                 */
                SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
            }
            break;

        case SEC_OID_PKCS7_DATA:
            /*
             * If a output callback has been specified, we want to set the filter
             * to call the callback.  This is taken care of in
             * sec_pkcs7_decoder_start_decrypt() or
             * sec_pkcs7_decoder_start_digests() for the other content types.
             */

            if (before && dest == &(cinfo->content.data)) {

                /*
                 * Set the filter proc up.
                 */
                SEC_ASN1DecoderSetFilterProc(p7dcx->dcx,
                                             sec_pkcs7_decoder_filter,
                                             p7dcx,
                                             (PRBool)(p7dcx->cb != NULL));
                break;
            }

            if (after && dest == &(cinfo->content.data)) {
                /*
                 * Time to clean up after ourself, stop the Notify and Filter
                 * procedures.
                 */
                SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
                SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);
            }
            break;

        default:
            SEC_ASN1DecoderClearNotifyProc(p7dcx->dcx);
            break;
    }
}

SEC_PKCS7DecoderContext *
SEC_PKCS7DecoderStart(SEC_PKCS7DecoderContentCallback cb, void *cb_arg,
                      SECKEYGetPasswordKey pwfn, void *pwfn_arg,
                      SEC_PKCS7GetDecryptKeyCallback decrypt_key_cb,
                      void *decrypt_key_cb_arg,
                      SEC_PKCS7DecryptionAllowedCallback decrypt_allowed_cb)
{
    SEC_PKCS7DecoderContext *p7dcx;
    SEC_ASN1DecoderContext *dcx;
    SEC_PKCS7ContentInfo *cinfo;
    PLArenaPool *poolp;

    poolp = PORT_NewArena(1024); /* XXX what is right value? */
    if (poolp == NULL)
        return NULL;

    cinfo = (SEC_PKCS7ContentInfo *)PORT_ArenaZAlloc(poolp, sizeof(*cinfo));
    if (cinfo == NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
        return NULL;
    }

    cinfo->poolp = poolp;
    cinfo->pwfn = pwfn;
    cinfo->pwfn_arg = pwfn_arg;
    cinfo->created = PR_FALSE;
    cinfo->refCount = 1;

    p7dcx =
        (SEC_PKCS7DecoderContext *)PORT_ZAlloc(sizeof(SEC_PKCS7DecoderContext));
    if (p7dcx == NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
        return NULL;
    }

    p7dcx->tmp_poolp = PORT_NewArena(1024); /* XXX what is right value? */
    if (p7dcx->tmp_poolp == NULL) {
        PORT_Free(p7dcx);
        PORT_FreeArena(poolp, PR_FALSE);
        return NULL;
    }

    dcx = SEC_ASN1DecoderStart(poolp, cinfo, sec_PKCS7ContentInfoTemplate);
    if (dcx == NULL) {
        PORT_FreeArena(p7dcx->tmp_poolp, PR_FALSE);
        PORT_Free(p7dcx);
        PORT_FreeArena(poolp, PR_FALSE);
        return NULL;
    }

    SEC_ASN1DecoderSetNotifyProc(dcx, sec_pkcs7_decoder_notify, p7dcx);

    p7dcx->dcx = dcx;
    p7dcx->cinfo = cinfo;
    p7dcx->cb = cb;
    p7dcx->cb_arg = cb_arg;
    p7dcx->pwfn = pwfn;
    p7dcx->pwfn_arg = pwfn_arg;
    p7dcx->dkcb = decrypt_key_cb;
    p7dcx->dkcb_arg = decrypt_key_cb_arg;
    p7dcx->decrypt_allowed_cb = decrypt_allowed_cb;

    return p7dcx;
}

/*
 * Do the next chunk of PKCS7 decoding.  If there is a problem, set
 * an error and return a failure status.  Note that in the case of
 * an error, this routine is still prepared to be called again and
 * again in case that is the easiest route for our caller to take.
 * We simply detect it and do not do anything except keep setting
 * that error in case our caller has not noticed it yet...
 */
SECStatus
SEC_PKCS7DecoderUpdate(SEC_PKCS7DecoderContext *p7dcx,
                       const char *buf, unsigned long len)
{
    if (!p7dcx) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (p7dcx->cinfo != NULL && p7dcx->dcx != NULL) {
        PORT_Assert(p7dcx->error == 0);
        if (p7dcx->error == 0) {
            if (SEC_ASN1DecoderUpdate(p7dcx->dcx, buf, len) != SECSuccess) {
                p7dcx->error = PORT_GetError();
                PORT_Assert(p7dcx->error);
                if (p7dcx->error == 0)
                    p7dcx->error = -1;
            }
        }
    }

    if (p7dcx->error) {
        if (p7dcx->dcx != NULL) {
            (void)SEC_ASN1DecoderFinish(p7dcx->dcx);
            p7dcx->dcx = NULL;
        }
        if (p7dcx->cinfo != NULL) {
            SEC_PKCS7DestroyContentInfo(p7dcx->cinfo);
            p7dcx->cinfo = NULL;
        }
        PORT_SetError(p7dcx->error);
        return SECFailure;
    }

    return SECSuccess;
}

SEC_PKCS7ContentInfo *
SEC_PKCS7DecoderFinish(SEC_PKCS7DecoderContext *p7dcx)
{
    SEC_PKCS7ContentInfo *cinfo;

    cinfo = p7dcx->cinfo;
    if (p7dcx->dcx != NULL) {
        if (SEC_ASN1DecoderFinish(p7dcx->dcx) != SECSuccess) {
            SEC_PKCS7DestroyContentInfo(cinfo);
            cinfo = NULL;
        }
    }
    /* free any NSS data structures */
    if (p7dcx->worker.decryptobj) {
        sec_PKCS7DestroyDecryptObject(p7dcx->worker.decryptobj);
    }
    PORT_FreeArena(p7dcx->tmp_poolp, PR_FALSE);
    PORT_Free(p7dcx);
    return cinfo;
}

SEC_PKCS7ContentInfo *
SEC_PKCS7DecodeItem(SECItem *p7item,
                    SEC_PKCS7DecoderContentCallback cb, void *cb_arg,
                    SECKEYGetPasswordKey pwfn, void *pwfn_arg,
                    SEC_PKCS7GetDecryptKeyCallback decrypt_key_cb,
                    void *decrypt_key_cb_arg,
                    SEC_PKCS7DecryptionAllowedCallback decrypt_allowed_cb)
{
    SEC_PKCS7DecoderContext *p7dcx;

    p7dcx = SEC_PKCS7DecoderStart(cb, cb_arg, pwfn, pwfn_arg, decrypt_key_cb,
                                  decrypt_key_cb_arg, decrypt_allowed_cb);
    if (!p7dcx) {
        /* error code is set */
        return NULL;
    }
    (void)SEC_PKCS7DecoderUpdate(p7dcx, (char *)p7item->data, p7item->len);
    return SEC_PKCS7DecoderFinish(p7dcx);
}

/*
 * Abort the ASN.1 stream. Used by pkcs 12
 */
void
SEC_PKCS7DecoderAbort(SEC_PKCS7DecoderContext *p7dcx, int error)
{
    PORT_Assert(p7dcx);
    SEC_ASN1DecoderAbort(p7dcx->dcx, error);
}

/*
 * If the thing contains any certs or crls return true; false otherwise.
 */
PRBool
SEC_PKCS7ContainsCertsOrCrls(SEC_PKCS7ContentInfo *cinfo)
{
    SECOidTag kind;
    SECItem **certs;
    CERTSignedCrl **crls;

    kind = SEC_PKCS7ContentType(cinfo);
    switch (kind) {
        default:
        case SEC_OID_PKCS7_DATA:
        case SEC_OID_PKCS7_DIGESTED_DATA:
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            return PR_FALSE;
        case SEC_OID_PKCS7_SIGNED_DATA:
            certs = cinfo->content.signedData->rawCerts;
            crls = cinfo->content.signedData->crls;
            break;
        case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
            certs = cinfo->content.signedAndEnvelopedData->rawCerts;
            crls = cinfo->content.signedAndEnvelopedData->crls;
            break;
    }

    /*
     * I know this could be collapsed, but I was in a mood to be explicit.
     */
    if (certs != NULL && certs[0] != NULL)
        return PR_TRUE;
    else if (crls != NULL && crls[0] != NULL)
        return PR_TRUE;
    else
        return PR_FALSE;
}

/* return the content length...could use GetContent, however we
 * need the encrypted content length
 */
PRBool
SEC_PKCS7IsContentEmpty(SEC_PKCS7ContentInfo *cinfo, unsigned int minLen)
{
    SECItem *item = NULL;

    if (cinfo == NULL) {
        return PR_TRUE;
    }

    switch (SEC_PKCS7ContentType(cinfo)) {
        case SEC_OID_PKCS7_DATA:
            item = cinfo->content.data;
            break;
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            item = &cinfo->content.encryptedData->encContentInfo.encContent;
            break;
        default:
            /* add other types */
            return PR_FALSE;
    }

    if (!item) {
        return PR_TRUE;
    } else if (item->len <= minLen) {
        return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool
SEC_PKCS7ContentIsEncrypted(SEC_PKCS7ContentInfo *cinfo)
{
    SECOidTag kind;

    kind = SEC_PKCS7ContentType(cinfo);
    switch (kind) {
        default:
        case SEC_OID_PKCS7_DATA:
        case SEC_OID_PKCS7_DIGESTED_DATA:
        case SEC_OID_PKCS7_SIGNED_DATA:
            return PR_FALSE;
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
            return PR_TRUE;
    }
}

/*
 * If the PKCS7 content has a signature (not just *could* have a signature)
 * return true; false otherwise.  This can/should be called before calling
 * VerifySignature, which will always indicate failure if no signature is
 * present, but that does not mean there even was a signature!
 * Note that the content itself can be empty (detached content was sent
 * another way); it is the presence of the signature that matters.
 */
PRBool
SEC_PKCS7ContentIsSigned(SEC_PKCS7ContentInfo *cinfo)
{
    SECOidTag kind;
    SEC_PKCS7SignerInfo **signerinfos;

    kind = SEC_PKCS7ContentType(cinfo);
    switch (kind) {
        default:
        case SEC_OID_PKCS7_DATA:
        case SEC_OID_PKCS7_DIGESTED_DATA:
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            return PR_FALSE;
        case SEC_OID_PKCS7_SIGNED_DATA:
            signerinfos = cinfo->content.signedData->signerInfos;
            break;
        case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
            signerinfos = cinfo->content.signedAndEnvelopedData->signerInfos;
            break;
    }

    /*
     * I know this could be collapsed; but I kind of think it will get
     * more complicated before I am finished, so...
     */
    if (signerinfos != NULL && signerinfos[0] != NULL)
        return PR_TRUE;
    else
        return PR_FALSE;
}

/*
 * sec_pkcs7_verify_signature
 *
 *      Look at a PKCS7 contentInfo and check if the signature is good.
 *      The digest was either calculated earlier (and is stored in the
 *      contentInfo itself) or is passed in via "detached_digest".
 *
 *      The verification checks that the signing cert is valid and trusted
 *      for the purpose specified by "certusage" at
 *      - "*atTime" if "atTime" is not null, or
 *      - the signing time if the signing time is available in "cinfo", or
 *      - the current time (as returned by PR_Now).
 *
 *      In addition, if "keepcerts" is true, add any new certificates found
 *      into our local database.
 *
 * XXX Each place which returns PR_FALSE should be sure to have a good
 * error set for inspection by the caller.  Alternatively, we could create
 * an enumeration of success and each type of failure and return that
 * instead of a boolean.  For now, the default in a bad situation is to
 * set the error to SEC_ERROR_PKCS7_BAD_SIGNATURE.  But this should be
 * reviewed; better (more specific) errors should be possible (to distinguish
 * a signature failure from a badly-formed pkcs7 signedData, for example).
 * Some of the errors should probably just be SEC_ERROR_BAD_SIGNATURE,
 * but that has a less helpful error string associated with it right now;
 * if/when that changes, review and change these as needed.
 *
 * XXX This is broken wrt signedAndEnvelopedData.  In that case, the
 * message digest is doubly encrypted -- first encrypted with the signer
 * private key but then again encrypted with the bulk encryption key used
 * to encrypt the content.  So before we can pass the digest to VerifyDigest,
 * we need to decrypt it with the bulk encryption key.  Also, in this case,
 * there should be NO authenticatedAttributes (signerinfo->authAttr should
 * be NULL).
 */
static PRBool
sec_pkcs7_verify_signature(SEC_PKCS7ContentInfo *cinfo,
                           SECCertUsage certusage,
                           const SECItem *detached_digest,
                           HASH_HashType digest_type,
                           PRBool keepcerts,
                           const PRTime *atTime)
{
    SECAlgorithmID **digestalgs, *bulkid;
    const SECItem *digest;
    SECItem **digests;
    SECItem **rawcerts;
    SEC_PKCS7SignerInfo **signerinfos, *signerinfo;
    CERTCertificate *cert, **certs;
    PRBool goodsig;
    CERTCertDBHandle *certdb, *defaultdb;
    SECOidTag encTag, digestTag;
    HASH_HashType found_type;
    int i, certcount;
    SECKEYPublicKey *publickey;
    SECItem *content_type;
    PK11SymKey *sigkey;
    SECItem *encoded_stime;
    PRTime stime;
    PRTime verificationTime;
    SECStatus rv;

    /*
     * Everything needed in order to "goto done" safely.
     */
    goodsig = PR_FALSE;
    certcount = 0;
    cert = NULL;
    certs = NULL;
    certdb = NULL;
    defaultdb = CERT_GetDefaultCertDB();
    publickey = NULL;

    if (!SEC_PKCS7ContentIsSigned(cinfo)) {
        PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
        goto done;
    }

    PORT_Assert(cinfo->contentTypeTag != NULL);

    switch (cinfo->contentTypeTag->offset) {
        default:
        case SEC_OID_PKCS7_DATA:
        case SEC_OID_PKCS7_DIGESTED_DATA:
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            /* Could only get here if SEC_PKCS7ContentIsSigned is broken. */
            PORT_Assert(0);
        case SEC_OID_PKCS7_SIGNED_DATA: {
            SEC_PKCS7SignedData *sdp;

            sdp = cinfo->content.signedData;
            digestalgs = sdp->digestAlgorithms;
            digests = sdp->digests;
            rawcerts = sdp->rawCerts;
            signerinfos = sdp->signerInfos;
            content_type = &(sdp->contentInfo.contentType);
            sigkey = NULL;
            bulkid = NULL;
        } break;
        case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA: {
            SEC_PKCS7SignedAndEnvelopedData *saedp;

            saedp = cinfo->content.signedAndEnvelopedData;
            digestalgs = saedp->digestAlgorithms;
            digests = saedp->digests;
            rawcerts = saedp->rawCerts;
            signerinfos = saedp->signerInfos;
            content_type = &(saedp->encContentInfo.contentType);
            sigkey = saedp->sigKey;
            bulkid = &(saedp->encContentInfo.contentEncAlg);
        } break;
    }

    if ((signerinfos == NULL) || (signerinfos[0] == NULL)) {
        PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
        goto done;
    }

    /*
     * XXX Need to handle multiple signatures; checking them is easy,
     * but what should be the semantics here (like, return value)?
     */
    if (signerinfos[1] != NULL) {
        PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
        goto done;
    }

    signerinfo = signerinfos[0];

    /*
     * XXX I would like to just pass the issuerAndSN, along with the rawcerts
     * and crls, to some function that did all of this certificate stuff
     * (open/close the database if necessary, verifying the certs, etc.)
     * and gave me back a cert pointer if all was good.
     */
    certdb = defaultdb;
    if (certdb == NULL) {
        goto done;
    }

    certcount = 0;
    if (rawcerts != NULL) {
        for (; rawcerts[certcount] != NULL; certcount++) {
            /* just counting */
        }
    }

    /*
     * Note that the result of this is that each cert in "certs"
     * needs to be destroyed.
     */
    rv = CERT_ImportCerts(certdb, certusage, certcount, rawcerts, &certs,
                          keepcerts, PR_FALSE, NULL);
    if (rv != SECSuccess) {
        goto done;
    }

    /*
     * This cert will also need to be freed, but since we save it
     * in signerinfo for later, we do not want to destroy it when
     * we leave this function -- we let the clean-up of the entire
     * cinfo structure later do the destroy of this cert.
     */
    cert = CERT_FindCertByIssuerAndSN(certdb, signerinfo->issuerAndSN);
    if (cert == NULL) {
        goto done;
    }

    signerinfo->cert = cert;

    /*
     * Get and convert the signing time; if available, it will be used
     * both on the cert verification and for importing the sender
     * email profile.
     */
    encoded_stime = SEC_PKCS7GetSigningTime(cinfo);
    if (encoded_stime != NULL) {
        if (DER_DecodeTimeChoice(&stime, encoded_stime) != SECSuccess)
            encoded_stime = NULL; /* conversion failed, so pretend none */
    }

    /*
     * XXX  This uses the signing time, if available.  Additionally, we
     * might want to, if there is no signing time, get the message time
     * from the mail header itself, and use that.  That would require
     * a change to our interface though, and for S/MIME callers to pass
     * in a time (and for non-S/MIME callers to pass in nothing, or
     * maybe make them pass in the current time, always?).
     */
    if (atTime) {
        verificationTime = *atTime;
    } else if (encoded_stime != NULL) {
        verificationTime = stime;
    } else {
        verificationTime = PR_Now();
    }
    if (CERT_VerifyCert(certdb, cert, PR_TRUE, certusage, verificationTime,
                        cinfo->pwfn_arg, NULL) != SECSuccess) {
        /*
         * XXX Give the user an option to check the signature anyway?
         * If we want to do this, need to give a way to leave and display
         * some dialog and get the answer and come back through (or do
         * the rest of what we do below elsewhere, maybe by putting it
         * in a function that we call below and could call from a dialog
         * finish handler).
         */
        goto savecert;
    }

    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL)
        goto done;

    /*
     * XXX No!  If digests is empty, see if we can create it now by
     * digesting the contents.  This is necessary if we want to allow
     * somebody to do a simple decode (without filtering, etc.) and
     * then later call us here to do the verification.
     * OR, we can just specify that the interface to this routine
     * *requires* that the digest(s) be done before calling and either
     * stashed in the struct itself or passed in explicitly (as would
     * be done for detached contents).
     */
    if ((digests == NULL || digests[0] == NULL) && (detached_digest == NULL || detached_digest->data == NULL))
        goto done;

    /*
     * Find and confirm digest algorithm.
     */
    digestTag = SECOID_FindOIDTag(&(signerinfo->digestAlg.algorithm));

    /* make sure we understand the digest type first */
    found_type = HASH_GetHashTypeByOidTag(digestTag);
    if ((digestTag == SEC_OID_UNKNOWN) || (found_type == HASH_AlgNULL)) {
        PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
        goto done;
    }

    if (detached_digest != NULL) {
        unsigned int hashLen = HASH_ResultLen(found_type);

        if (digest_type != found_type ||
            detached_digest->len != hashLen) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }
        digest = detached_digest;
    } else {
        PORT_Assert(digestalgs != NULL && digestalgs[0] != NULL);
        if (digestalgs == NULL || digestalgs[0] == NULL) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        /*
         * pick digest matching signerinfo->digestAlg from digests
         */
        for (i = 0; digestalgs[i] != NULL; i++) {
            if (SECOID_FindOIDTag(&(digestalgs[i]->algorithm)) == digestTag)
                break;
        }
        if (digestalgs[i] == NULL) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        digest = digests[i];
    }

    encTag = SECOID_FindOIDTag(&(signerinfo->digestEncAlg.algorithm));
    if (encTag == SEC_OID_UNKNOWN) {
        PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
        goto done;
    }

    if (signerinfo->authAttr != NULL) {
        SEC_PKCS7Attribute *attr;
        SECItem *value;
        SECItem encoded_attrs;

        /*
         * We have a sigkey only for signedAndEnvelopedData, which is
         * not supposed to have any authenticated attributes.
         */
        if (sigkey != NULL) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        /*
         * PKCS #7 says that if there are any authenticated attributes,
         * then there must be one for content type which matches the
         * content type of the content being signed, and there must
         * be one for message digest which matches our message digest.
         * So check these things first.
         * XXX Might be nice to have a compare-attribute-value function
         * which could collapse the following nicely.
         */
        attr = sec_PKCS7FindAttribute(signerinfo->authAttr,
                                      SEC_OID_PKCS9_CONTENT_TYPE, PR_TRUE);
        value = sec_PKCS7AttributeValue(attr);
        if (value == NULL || value->len != content_type->len) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }
        if (PORT_Memcmp(value->data, content_type->data, value->len) != 0) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        attr = sec_PKCS7FindAttribute(signerinfo->authAttr,
                                      SEC_OID_PKCS9_MESSAGE_DIGEST, PR_TRUE);
        value = sec_PKCS7AttributeValue(attr);
        if (value == NULL || value->len != digest->len) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }
        if (PORT_Memcmp(value->data, digest->data, value->len) != 0) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        /*
         * Okay, we met the constraints of the basic attributes.
         * Now check the signature, which is based on a digest of
         * the DER-encoded authenticated attributes.  So, first we
         * encode and then we digest/verify.
         */
        encoded_attrs.data = NULL;
        encoded_attrs.len = 0;
        if (sec_PKCS7EncodeAttributes(NULL, &encoded_attrs,
                                      &(signerinfo->authAttr)) == NULL)
            goto done;

        if (encoded_attrs.data == NULL || encoded_attrs.len == 0) {
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        goodsig = (PRBool)(VFY_VerifyDataDirect(encoded_attrs.data,
                                                encoded_attrs.len,
                                                publickey, &(signerinfo->encDigest),
                                                encTag, digestTag, NULL,
                                                cinfo->pwfn_arg) == SECSuccess);
        PORT_Free(encoded_attrs.data);
    } else {
        SECItem *sig;
        SECItem holder;

        /*
         * No authenticated attributes.
         * The signature is based on the plain message digest.
         */

        sig = &(signerinfo->encDigest);
        if (sig->len == 0) { /* bad signature */
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
            goto done;
        }

        if (sigkey != NULL) {
            sec_PKCS7CipherObject *decryptobj;
            unsigned int buflen;

            /*
             * For signedAndEnvelopedData, we first must decrypt the encrypted
             * digest with the bulk encryption key.  The result is the normal
             * encrypted digest (aka the signature).
             */
            decryptobj = sec_PKCS7CreateDecryptObject(sigkey, bulkid);
            if (decryptobj == NULL)
                goto done;

            buflen = sec_PKCS7DecryptLength(decryptobj, sig->len, PR_TRUE);
            PORT_Assert(buflen);
            if (buflen == 0) { /* something is wrong */
                sec_PKCS7DestroyDecryptObject(decryptobj);
                goto done;
            }

            holder.data = (unsigned char *)PORT_Alloc(buflen);
            if (holder.data == NULL) {
                sec_PKCS7DestroyDecryptObject(decryptobj);
                goto done;
            }

            rv = sec_PKCS7Decrypt(decryptobj, holder.data, &holder.len, buflen,
                                  sig->data, sig->len, PR_TRUE);
            sec_PKCS7DestroyDecryptObject(decryptobj);
            if (rv != SECSuccess) {
                goto done;
            }

            sig = &holder;
        }

        goodsig = (PRBool)(VFY_VerifyDigestDirect(digest, publickey, sig,
                                                  encTag, digestTag, cinfo->pwfn_arg) == SECSuccess);

        if (sigkey != NULL) {
            PORT_Assert(sig == &holder);
            PORT_ZFree(holder.data, holder.len);
        }
    }

    if (!goodsig) {
        /*
         * XXX Change the generic error into our specific one, because
         * in that case we get a better explanation out of the Security
         * Advisor.  This is really a bug in our error strings (the
         * "generic" error has a lousy/wrong message associated with it
         * which assumes the signature verification was done for the
         * purposes of checking the issuer signature on a certificate)
         * but this is at least an easy workaround and/or in the
         * Security Advisor, which specifically checks for the error
         * SEC_ERROR_PKCS7_BAD_SIGNATURE and gives more explanation
         * in that case but does not similarly check for
         * SEC_ERROR_BAD_SIGNATURE.  It probably should, but then would
         * probably say the wrong thing in the case that it *was* the
         * certificate signature check that failed during the cert
         * verification done above.  Our error handling is really a mess.
         */
        if (PORT_GetError() == SEC_ERROR_BAD_SIGNATURE)
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
    }

savecert:
    /*
     * Only save the smime profile if we are checking an email message and
     * the cert has an email address in it.
     */
    if (cert->emailAddr && cert->emailAddr[0] &&
        ((certusage == certUsageEmailSigner) ||
         (certusage == certUsageEmailRecipient))) {
        SECItem *profile = NULL;
        int save_error;

        /*
         * Remember the current error set because we do not care about
         * anything set by the functions we are about to call.
         */
        save_error = PORT_GetError();

        if (goodsig && (signerinfo->authAttr != NULL)) {
            /*
             * If the signature is good, then we can save the S/MIME profile,
             * if we have one.
             */
            SEC_PKCS7Attribute *attr;

            attr = sec_PKCS7FindAttribute(signerinfo->authAttr,
                                          SEC_OID_PKCS9_SMIME_CAPABILITIES,
                                          PR_TRUE);
            profile = sec_PKCS7AttributeValue(attr);
        }

        rv = CERT_SaveSMimeProfile(cert, profile, encoded_stime);

        /*
         * Restore the saved error in case the calls above set a new
         * one that we do not actually care about.
         */
        PORT_SetError(save_error);

        /*
         * XXX Failure is not indicated anywhere -- the signature
         * verification itself is unaffected by whether or not the
         * profile was successfully saved.
         */
    }

done:

    /*
     * See comment above about why we do not want to destroy cert
     * itself here.
     */

    if (certs != NULL)
        CERT_DestroyCertArray(certs, certcount);

    if (publickey != NULL)
        SECKEY_DestroyPublicKey(publickey);

    return goodsig;
}

/*
 * SEC_PKCS7VerifySignature
 *      Look at a PKCS7 contentInfo and check if the signature is good.
 *      The verification checks that the signing cert is valid and trusted
 *      for the purpose specified by "certusage".
 *
 *      In addition, if "keepcerts" is true, add any new certificates found
 *      into our local database.
 */
PRBool
SEC_PKCS7VerifySignature(SEC_PKCS7ContentInfo *cinfo,
                         SECCertUsage certusage,
                         PRBool keepcerts)
{
    return sec_pkcs7_verify_signature(cinfo, certusage,
                                      NULL, HASH_AlgNULL, keepcerts, NULL);
}

/*
 * SEC_PKCS7VerifyDetachedSignature
 *      Look at a PKCS7 contentInfo and check if the signature matches
 *      a passed-in digest (calculated, supposedly, from detached contents).
 *      The verification checks that the signing cert is valid and trusted
 *      for the purpose specified by "certusage".
 *
 *      In addition, if "keepcerts" is true, add any new certificates found
 *      into our local database.
 */
PRBool
SEC_PKCS7VerifyDetachedSignature(SEC_PKCS7ContentInfo *cinfo,
                                 SECCertUsage certusage,
                                 const SECItem *detached_digest,
                                 HASH_HashType digest_type,
                                 PRBool keepcerts)
{
    return sec_pkcs7_verify_signature(cinfo, certusage,
                                      detached_digest, digest_type,
                                      keepcerts, NULL);
}

/*
 * SEC_PKCS7VerifyDetachedSignatureAtTime
 *      Look at a PKCS7 contentInfo and check if the signature matches
 *      a passed-in digest (calculated, supposedly, from detached contents).
 *      The verification checks that the signing cert is valid and trusted
 *      for the purpose specified by "certusage" at time "atTime".
 *
 *      In addition, if "keepcerts" is true, add any new certificates found
 *      into our local database.
 */
PRBool
SEC_PKCS7VerifyDetachedSignatureAtTime(SEC_PKCS7ContentInfo *cinfo,
                                       SECCertUsage certusage,
                                       const SECItem *detached_digest,
                                       HASH_HashType digest_type,
                                       PRBool keepcerts,
                                       PRTime atTime)
{
    return sec_pkcs7_verify_signature(cinfo, certusage,
                                      detached_digest, digest_type,
                                      keepcerts, &atTime);
}

/*
 * Return the asked-for portion of the name of the signer of a PKCS7
 * signed object.
 *
 * Returns a pointer to allocated memory, which must be freed.
 * A NULL return value is an error.
 */

#define sec_common_name 1
#define sec_email_address 2

static char *
sec_pkcs7_get_signer_cert_info(SEC_PKCS7ContentInfo *cinfo, int selector)
{
    SECOidTag kind;
    SEC_PKCS7SignerInfo **signerinfos;
    CERTCertificate *signercert;
    char *container;

    kind = SEC_PKCS7ContentType(cinfo);
    switch (kind) {
        default:
        case SEC_OID_PKCS7_DATA:
        case SEC_OID_PKCS7_DIGESTED_DATA:
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            PORT_Assert(0);
            return NULL;
        case SEC_OID_PKCS7_SIGNED_DATA: {
            SEC_PKCS7SignedData *sdp;

            sdp = cinfo->content.signedData;
            signerinfos = sdp->signerInfos;
        } break;
        case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA: {
            SEC_PKCS7SignedAndEnvelopedData *saedp;

            saedp = cinfo->content.signedAndEnvelopedData;
            signerinfos = saedp->signerInfos;
        } break;
    }

    if (signerinfos == NULL || signerinfos[0] == NULL)
        return NULL;

    signercert = signerinfos[0]->cert;

    /*
     * No cert there; see if we can find one by calling verify ourselves.
     */
    if (signercert == NULL) {
        /*
         * The cert usage does not matter in this case, because we do not
         * actually care about the verification itself, but we have to pick
         * some valid usage to pass in.
         */
        (void)sec_pkcs7_verify_signature(cinfo, certUsageEmailSigner,
                                         NULL, HASH_AlgNULL, PR_FALSE, NULL);
        signercert = signerinfos[0]->cert;
        if (signercert == NULL)
            return NULL;
    }

    switch (selector) {
        case sec_common_name:
            container = CERT_GetCommonName(&signercert->subject);
            break;
        case sec_email_address:
            if (signercert->emailAddr && signercert->emailAddr[0]) {
                container = PORT_Strdup(signercert->emailAddr);
            } else {
                container = NULL;
            }
            break;
        default:
            PORT_Assert(0);
            container = NULL;
            break;
    }

    return container;
}

char *
SEC_PKCS7GetSignerCommonName(SEC_PKCS7ContentInfo *cinfo)
{
    return sec_pkcs7_get_signer_cert_info(cinfo, sec_common_name);
}

char *
SEC_PKCS7GetSignerEmailAddress(SEC_PKCS7ContentInfo *cinfo)
{
    return sec_pkcs7_get_signer_cert_info(cinfo, sec_email_address);
}

/*
 * Return the signing time, in UTCTime format, of a PKCS7 contentInfo.
 */
SECItem *
SEC_PKCS7GetSigningTime(SEC_PKCS7ContentInfo *cinfo)
{
    SEC_PKCS7SignerInfo **signerinfos;
    SEC_PKCS7Attribute *attr;

    if (SEC_PKCS7ContentType(cinfo) != SEC_OID_PKCS7_SIGNED_DATA)
        return NULL;

    signerinfos = cinfo->content.signedData->signerInfos;

    /*
     * No signature, or more than one, means no deal.
     */
    if (signerinfos == NULL || signerinfos[0] == NULL || signerinfos[1] != NULL)
        return NULL;

    attr = sec_PKCS7FindAttribute(signerinfos[0]->authAttr,
                                  SEC_OID_PKCS9_SIGNING_TIME, PR_TRUE);
    return sec_PKCS7AttributeValue(attr);
}
