/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "p12t.h"
#include "p12.h"
#include "plarena.h"
#include "secitem.h"
#include "secoid.h"
#include "seccomon.h"
#include "secport.h"
#include "cert.h"
#include "secpkcs5.h"
#include "secpkcs7.h"
#include "secasn1.h"
#include "secerr.h"
#include "sechash.h"
#include "pk11func.h"
#include "p12plcy.h"
#include "p12local.h"
#include "prcpucfg.h"

extern const int NSS_PBE_DEFAULT_ITERATION_COUNT; /* defined in p7create.c */

/*
** This PKCS12 file encoder uses numerous nested ASN.1 and PKCS7 encoder
** contexts.  It can be difficult to keep straight.  Here's a picture:
**
**  "outer"  ASN.1 encoder.  The output goes to the library caller's CB.
**  "middle" PKCS7 encoder.  Feeds    the "outer" ASN.1 encoder.
**  "middle" ASN1  encoder.  Encodes  the encrypted aSafes.
**                           Feeds    the "middle" P7 encoder above.
**  "inner"  PKCS7 encoder.  Encrypts the "authenticated Safes" (aSafes)
**                           Feeds    the "middle" ASN.1 encoder above.
**  "inner"  ASN.1 encoder.  Encodes  the unencrypted aSafes.
**                           Feeds    the "inner" P7 enocder above.
**
** Buffering has been added at each point where the output of an ASN.1
** encoder feeds the input of a PKCS7 encoder.
*/

/*********************************
 * Output buffer object, used to buffer output from ASN.1 encoder
 * before passing data on down to the next PKCS7 encoder.
 *********************************/

#define PK12_OUTPUT_BUFFER_SIZE 8192

struct sec_pkcs12OutputBufferStr {
    SEC_PKCS7EncoderContext *p7eCx;
    PK11Context *hmacCx;
    unsigned int numBytes;
    unsigned int bufBytes;
    char buf[PK12_OUTPUT_BUFFER_SIZE];
};
typedef struct sec_pkcs12OutputBufferStr sec_pkcs12OutputBuffer;

/*********************************
 * Structures used in exporting the PKCS 12 blob
 *********************************/

/* A SafeInfo is used for each ContentInfo which makes up the
 * sequence of safes in the AuthenticatedSafe portion of the
 * PFX structure.
 */
struct SEC_PKCS12SafeInfoStr {
    PLArenaPool *arena;

    /* information for setting up password encryption */
    SECItem pwitem;
    SECOidTag algorithm;
    PK11SymKey *encryptionKey;

    /* how many items have been stored in this safe,
     * we will skip any safe which does not contain any
     * items
     */
    unsigned int itemCount;

    /* the content info for the safe */
    SEC_PKCS7ContentInfo *cinfo;

    sec_PKCS12SafeContents *safe;
};

/* An opaque structure which contains information needed for exporting
 * certificates and keys through PKCS 12.
 */
struct SEC_PKCS12ExportContextStr {
    PLArenaPool *arena;
    PK11SlotInfo *slot;
    void *wincx;

    /* integrity information */
    PRBool integrityEnabled;
    PRBool pwdIntegrity;
    union {
        struct sec_PKCS12PasswordModeInfo pwdInfo;
        struct sec_PKCS12PublicKeyModeInfo pubkeyInfo;
    } integrityInfo;

    /* helper functions */
    /* retrieve the password call back */
    SECKEYGetPasswordKey pwfn;
    void *pwfnarg;

    /* safe contents bags */
    SEC_PKCS12SafeInfo **safeInfos;
    unsigned int safeInfoCount;

    /* the sequence of safes */
    sec_PKCS12AuthenticatedSafe authSafe;

    /* information needing deletion */
    CERTCertificate **certList;
};

/* structures for passing information to encoder callbacks when processing
 * data through the ASN1 engine.
 */
struct sec_pkcs12_encoder_output {
    SEC_PKCS12EncoderOutputCallback outputfn;
    void *outputarg;
};

struct sec_pkcs12_hmac_and_output_info {
    void *arg;
    struct sec_pkcs12_encoder_output output;
};

/* An encoder context which is used for the actual encoding
 * portion of PKCS 12.
 */
typedef struct sec_PKCS12EncoderContextStr {
    PLArenaPool *arena;
    SEC_PKCS12ExportContext *p12exp;

    /* encoder information - this is set up based on whether
     * password based or public key pased privacy is being used
     */
    SEC_ASN1EncoderContext *outerA1ecx;
    union {
        struct sec_pkcs12_hmac_and_output_info hmacAndOutputInfo;
        struct sec_pkcs12_encoder_output encOutput;
    } output;

    /* structures for encoding of PFX and MAC */
    sec_PKCS12PFXItem pfx;
    sec_PKCS12MacData mac;

    /* authenticated safe encoding tracking information */
    SEC_PKCS7ContentInfo *aSafeCinfo;
    SEC_PKCS7EncoderContext *middleP7ecx;
    SEC_ASN1EncoderContext *middleA1ecx;
    unsigned int currentSafe;

    /* hmac context */
    PK11Context *hmacCx;

    /* output buffers */
    sec_pkcs12OutputBuffer middleBuf;
    sec_pkcs12OutputBuffer innerBuf;

} sec_PKCS12EncoderContext;

/*********************************
 * Export setup routines
 *********************************/

/* SEC_PKCS12CreateExportContext
 *   Creates an export context and sets the unicode and password retrieval
 *   callbacks.  This is the first call which must be made when exporting
 *   a PKCS 12 blob.
 *
 * pwfn, pwfnarg - password retrieval callback and argument.  these are
 *                 required for password-authentication mode.
 */
SEC_PKCS12ExportContext *
SEC_PKCS12CreateExportContext(SECKEYGetPasswordKey pwfn, void *pwfnarg,
                              PK11SlotInfo *slot, void *wincx)
{
    PLArenaPool *arena = NULL;
    SEC_PKCS12ExportContext *p12ctxt = NULL;

    /* allocate the arena and create the context */
    arena = PORT_NewArena(4096);
    if (!arena) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    p12ctxt = (SEC_PKCS12ExportContext *)PORT_ArenaZAlloc(arena,
                                                          sizeof(SEC_PKCS12ExportContext));
    if (!p12ctxt) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* password callback for key retrieval */
    p12ctxt->pwfn = pwfn;
    p12ctxt->pwfnarg = pwfnarg;

    p12ctxt->integrityEnabled = PR_FALSE;
    p12ctxt->arena = arena;
    p12ctxt->wincx = wincx;
    p12ctxt->slot = (slot) ? PK11_ReferenceSlot(slot) : PK11_GetInternalSlot();

    return p12ctxt;

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }

    return NULL;
}

/*
 * Adding integrity mode
 */

/* SEC_PKCS12AddPasswordIntegrity
 *      Add password integrity to the exported data.  If an integrity method
 *      has already been set, then return an error.
 *
 *      p12ctxt - the export context
 *      pwitem - the password for integrity mode
 *      integAlg - the integrity algorithm to use for authentication.
 */
SECStatus
SEC_PKCS12AddPasswordIntegrity(SEC_PKCS12ExportContext *p12ctxt,
                               SECItem *pwitem, SECOidTag integAlg)
{
    if (!p12ctxt || p12ctxt->integrityEnabled) {
        return SECFailure;
    }

    /* set up integrity information */
    p12ctxt->pwdIntegrity = PR_TRUE;
    p12ctxt->integrityInfo.pwdInfo.password =
        (SECItem *)PORT_ArenaZAlloc(p12ctxt->arena, sizeof(SECItem));
    if (!p12ctxt->integrityInfo.pwdInfo.password) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    if (SECITEM_CopyItem(p12ctxt->arena,
                         p12ctxt->integrityInfo.pwdInfo.password, pwitem) != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    p12ctxt->integrityInfo.pwdInfo.algorithm = integAlg;
    p12ctxt->integrityEnabled = PR_TRUE;

    return SECSuccess;
}

/* SEC_PKCS12AddPublicKeyIntegrity
 *      Add public key integrity to the exported data.  If an integrity method
 *      has already been set, then return an error.  The certificate must be
 *      allowed to be used as a signing cert.
 *
 *      p12ctxt - the export context
 *      cert - signer certificate
 *      certDb - the certificate database
 *      algorithm - signing algorithm
 *      keySize - size of the signing key (?)
 */
SECStatus
SEC_PKCS12AddPublicKeyIntegrity(SEC_PKCS12ExportContext *p12ctxt,
                                CERTCertificate *cert, CERTCertDBHandle *certDb,
                                SECOidTag algorithm, int keySize)
{
    if (!p12ctxt) {
        return SECFailure;
    }

    p12ctxt->integrityInfo.pubkeyInfo.cert = cert;
    p12ctxt->integrityInfo.pubkeyInfo.certDb = certDb;
    p12ctxt->integrityInfo.pubkeyInfo.algorithm = algorithm;
    p12ctxt->integrityInfo.pubkeyInfo.keySize = keySize;
    p12ctxt->integrityEnabled = PR_TRUE;

    return SECSuccess;
}

/*
 * Adding safes - encrypted (password/public key) or unencrypted
 *      Each of the safe creation routines return an opaque pointer which
 *      are later passed into the routines for exporting certificates and
 *      keys.
 */

/* append the newly created safeInfo to list of safeInfos in the export
 * context.
 */
static SECStatus
sec_pkcs12_append_safe_info(SEC_PKCS12ExportContext *p12ctxt, SEC_PKCS12SafeInfo *info)
{
    void *mark = NULL, *dummy1 = NULL, *dummy2 = NULL;

    if (!p12ctxt || !info) {
        return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    /* if no safeInfos have been set, create the list, otherwise expand it. */
    if (!p12ctxt->safeInfoCount) {
        p12ctxt->safeInfos = (SEC_PKCS12SafeInfo **)PORT_ArenaZAlloc(p12ctxt->arena,
                                                                     2 * sizeof(SEC_PKCS12SafeInfo *));
        dummy1 = p12ctxt->safeInfos;
        p12ctxt->authSafe.encodedSafes = (SECItem **)PORT_ArenaZAlloc(p12ctxt->arena,
                                                                      2 * sizeof(SECItem *));
        dummy2 = p12ctxt->authSafe.encodedSafes;
    } else {
        dummy1 = PORT_ArenaGrow(p12ctxt->arena, p12ctxt->safeInfos,
                                (p12ctxt->safeInfoCount + 1) * sizeof(SEC_PKCS12SafeInfo *),
                                (p12ctxt->safeInfoCount + 2) * sizeof(SEC_PKCS12SafeInfo *));
        p12ctxt->safeInfos = (SEC_PKCS12SafeInfo **)dummy1;
        dummy2 = PORT_ArenaGrow(p12ctxt->arena, p12ctxt->authSafe.encodedSafes,
                                (p12ctxt->authSafe.safeCount + 1) * sizeof(SECItem *),
                                (p12ctxt->authSafe.safeCount + 2) * sizeof(SECItem *));
        p12ctxt->authSafe.encodedSafes = (SECItem **)dummy2;
    }
    if (!dummy1 || !dummy2) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* append the new safeInfo and null terminate the list */
    p12ctxt->safeInfos[p12ctxt->safeInfoCount] = info;
    p12ctxt->safeInfos[++p12ctxt->safeInfoCount] = NULL;
    p12ctxt->authSafe.encodedSafes[p12ctxt->authSafe.safeCount] =
        (SECItem *)PORT_ArenaZAlloc(p12ctxt->arena, sizeof(SECItem));
    if (!p12ctxt->authSafe.encodedSafes[p12ctxt->authSafe.safeCount]) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }
    p12ctxt->authSafe.encodedSafes[++p12ctxt->authSafe.safeCount] = NULL;

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(p12ctxt->arena, mark);
    return SECFailure;
}

/* SEC_PKCS12CreatePasswordPrivSafe
 *      Create a password privacy safe to store exported information in.
 *
 *      p12ctxt - export context
 *      pwitem - password for encryption
 *      privAlg - pbe algorithm through which encryption is done.
 */
SEC_PKCS12SafeInfo *
SEC_PKCS12CreatePasswordPrivSafe(SEC_PKCS12ExportContext *p12ctxt,
                                 SECItem *pwitem, SECOidTag privAlg)
{
    SEC_PKCS12SafeInfo *safeInfo = NULL;
    void *mark = NULL;
    PK11SlotInfo *slot = NULL;
    SECAlgorithmID *algId;
    SECItem uniPwitem = { siBuffer, NULL, 0 };

    if (!p12ctxt) {
        return NULL;
    }

    /* allocate the safe info */
    mark = PORT_ArenaMark(p12ctxt->arena);
    safeInfo = (SEC_PKCS12SafeInfo *)PORT_ArenaZAlloc(p12ctxt->arena,
                                                      sizeof(SEC_PKCS12SafeInfo));
    if (!safeInfo) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        PORT_ArenaRelease(p12ctxt->arena, mark);
        return NULL;
    }

    safeInfo->itemCount = 0;

    /* create the encrypted safe */
    if (!SEC_PKCS5IsAlgorithmPBEAlgTag(privAlg)) {
        SECOidTag prfAlg = SEC_OID_UNKNOWN;
        /* if we have password integrity set, use that to set the integrity
         * hash algorithm to set our password PRF. If we haven't set it, just
         * let the low level code pick it */
        if (p12ctxt->integrityEnabled && p12ctxt->pwdIntegrity) {
            prfAlg = HASH_GetHMACOidTagByHashOidTag(
                p12ctxt->integrityInfo.pwdInfo.algorithm);
        }
        safeInfo->cinfo = SEC_PKCS7CreateEncryptedDataWithPBEV2(SEC_OID_PKCS5_PBES2,
                                                                privAlg,
                                                                prfAlg,
                                                                0,
                                                                p12ctxt->pwfn,
                                                                p12ctxt->pwfnarg);
    } else {
        safeInfo->cinfo = SEC_PKCS7CreateEncryptedData(privAlg, 0, p12ctxt->pwfn,
                                                       p12ctxt->pwfnarg);
    }
    if (!safeInfo->cinfo) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }
    safeInfo->arena = p12ctxt->arena;

    if (!sec_pkcs12_encode_password(NULL, &uniPwitem, privAlg, pwitem)) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }
    if (SECITEM_CopyItem(p12ctxt->arena, &safeInfo->pwitem, &uniPwitem) != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* generate the encryption key */
    slot = PK11_ReferenceSlot(p12ctxt->slot);
    if (!slot) {
        slot = PK11_GetInternalKeySlot();
        if (!slot) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }
    }

    algId = SEC_PKCS7GetEncryptionAlgorithm(safeInfo->cinfo);
    safeInfo->encryptionKey = PK11_PBEKeyGen(slot, algId, &uniPwitem,
                                             PR_FALSE, p12ctxt->wincx);
    if (!safeInfo->encryptionKey) {
        goto loser;
    }

    safeInfo->arena = p12ctxt->arena;
    safeInfo->safe = NULL;
    if (sec_pkcs12_append_safe_info(p12ctxt, safeInfo) != SECSuccess) {
        goto loser;
    }

    if (uniPwitem.data) {
        SECITEM_ZfreeItem(&uniPwitem, PR_FALSE);
    }
    PORT_ArenaUnmark(p12ctxt->arena, mark);

    if (slot) {
        PK11_FreeSlot(slot);
    }
    return safeInfo;

loser:
    if (slot) {
        PK11_FreeSlot(slot);
    }
    if (safeInfo->cinfo) {
        SEC_PKCS7DestroyContentInfo(safeInfo->cinfo);
    }

    if (uniPwitem.data) {
        SECITEM_ZfreeItem(&uniPwitem, PR_FALSE);
    }

    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/* SEC_PKCS12CreateUnencryptedSafe
 *      Creates an unencrypted safe within the export context.
 *
 *      p12ctxt - the export context
 */
SEC_PKCS12SafeInfo *
SEC_PKCS12CreateUnencryptedSafe(SEC_PKCS12ExportContext *p12ctxt)
{
    SEC_PKCS12SafeInfo *safeInfo = NULL;
    void *mark = NULL;

    if (!p12ctxt) {
        return NULL;
    }

    /* create the safe info */
    mark = PORT_ArenaMark(p12ctxt->arena);
    safeInfo = (SEC_PKCS12SafeInfo *)PORT_ArenaZAlloc(p12ctxt->arena,
                                                      sizeof(SEC_PKCS12SafeInfo));
    if (!safeInfo) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    safeInfo->itemCount = 0;

    /* create the safe content */
    safeInfo->cinfo = SEC_PKCS7CreateData();
    if (!safeInfo->cinfo) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    if (sec_pkcs12_append_safe_info(p12ctxt, safeInfo) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return safeInfo;

loser:
    if (safeInfo->cinfo) {
        SEC_PKCS7DestroyContentInfo(safeInfo->cinfo);
    }

    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/* SEC_PKCS12CreatePubKeyEncryptedSafe
 *      Creates a safe which is protected by public key encryption.
 *
 *      p12ctxt - the export context
 *      certDb - the certificate database
 *      signer - the signer's certificate
 *      recipients - the list of recipient certificates.
 *      algorithm - the encryption algorithm to use
 *      keysize - the algorithms key size (?)
 */
SEC_PKCS12SafeInfo *
SEC_PKCS12CreatePubKeyEncryptedSafe(SEC_PKCS12ExportContext *p12ctxt,
                                    CERTCertDBHandle *certDb,
                                    CERTCertificate *signer,
                                    CERTCertificate **recipients,
                                    SECOidTag algorithm, int keysize)
{
    SEC_PKCS12SafeInfo *safeInfo = NULL;
    void *mark = NULL;

    if (!p12ctxt || !signer || !recipients || !(*recipients)) {
        return NULL;
    }

    /* allocate the safeInfo */
    mark = PORT_ArenaMark(p12ctxt->arena);
    safeInfo = (SEC_PKCS12SafeInfo *)PORT_ArenaZAlloc(p12ctxt->arena,
                                                      sizeof(SEC_PKCS12SafeInfo));
    if (!safeInfo) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    safeInfo->itemCount = 0;
    safeInfo->arena = p12ctxt->arena;

    /* create the enveloped content info using certUsageEmailSigner currently.
     * XXX We need to eventually use something other than certUsageEmailSigner
     */
    safeInfo->cinfo = SEC_PKCS7CreateEnvelopedData(signer, certUsageEmailSigner,
                                                   certDb, algorithm, keysize,
                                                   p12ctxt->pwfn, p12ctxt->pwfnarg);
    if (!safeInfo->cinfo) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* add recipients */
    if (recipients) {
        unsigned int i = 0;
        while (recipients[i] != NULL) {
            SECStatus rv = SEC_PKCS7AddRecipient(safeInfo->cinfo, recipients[i],
                                                 certUsageEmailRecipient, certDb);
            if (rv != SECSuccess) {
                goto loser;
            }
            i++;
        }
    }

    if (sec_pkcs12_append_safe_info(p12ctxt, safeInfo) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return safeInfo;

loser:
    if (safeInfo->cinfo) {
        SEC_PKCS7DestroyContentInfo(safeInfo->cinfo);
        safeInfo->cinfo = NULL;
    }

    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/*********************************
 * Routines to handle the exporting of the keys and certificates
 *********************************/

/* creates a safe contents which safeBags will be appended to */
sec_PKCS12SafeContents *
sec_PKCS12CreateSafeContents(PLArenaPool *arena)
{
    sec_PKCS12SafeContents *safeContents;

    if (arena == NULL) {
        return NULL;
    }

    /* create the safe contents */
    safeContents = (sec_PKCS12SafeContents *)PORT_ArenaZAlloc(arena,
                                                              sizeof(sec_PKCS12SafeContents));
    if (!safeContents) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* set up the internal contents info */
    safeContents->safeBags = NULL;
    safeContents->arena = arena;
    safeContents->bagCount = 0;

    return safeContents;

loser:
    return NULL;
}

/* appends a safe bag to a safeContents using the specified arena.
 */
SECStatus
sec_pkcs12_append_bag_to_safe_contents(PLArenaPool *arena,
                                       sec_PKCS12SafeContents *safeContents,
                                       sec_PKCS12SafeBag *safeBag)
{
    void *mark = NULL, *dummy = NULL;

    if (!arena || !safeBag || !safeContents) {
        return SECFailure;
    }

    mark = PORT_ArenaMark(arena);
    if (!mark) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    /* allocate space for the list, or reallocate to increase space */
    if (!safeContents->safeBags) {
        safeContents->safeBags = (sec_PKCS12SafeBag **)PORT_ArenaZAlloc(arena,
                                                                        (2 * sizeof(sec_PKCS12SafeBag *)));
        dummy = safeContents->safeBags;
        safeContents->bagCount = 0;
    } else {
        dummy = PORT_ArenaGrow(arena, safeContents->safeBags,
                               (safeContents->bagCount + 1) * sizeof(sec_PKCS12SafeBag *),
                               (safeContents->bagCount + 2) * sizeof(sec_PKCS12SafeBag *));
        safeContents->safeBags = (sec_PKCS12SafeBag **)dummy;
    }

    if (!dummy) {
        PORT_ArenaRelease(arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    /* append the bag at the end and null terminate the list */
    safeContents->safeBags[safeContents->bagCount++] = safeBag;
    safeContents->safeBags[safeContents->bagCount] = NULL;

    PORT_ArenaUnmark(arena, mark);

    return SECSuccess;
}

/* appends a safeBag to a specific safeInfo.
 */
SECStatus
sec_pkcs12_append_bag(SEC_PKCS12ExportContext *p12ctxt,
                      SEC_PKCS12SafeInfo *safeInfo, sec_PKCS12SafeBag *safeBag)
{
    sec_PKCS12SafeContents *dest;
    SECStatus rv = SECFailure;

    if (!p12ctxt || !safeBag || !safeInfo) {
        return SECFailure;
    }

    if (!safeInfo->safe) {
        safeInfo->safe = sec_PKCS12CreateSafeContents(p12ctxt->arena);
        if (!safeInfo->safe) {
            return SECFailure;
        }
    }

    dest = safeInfo->safe;
    rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena, dest, safeBag);
    if (rv == SECSuccess) {
        safeInfo->itemCount++;
    }

    return rv;
}

/* Creates a safeBag of the specified type, and if bagData is specified,
 * the contents are set.  The contents could be set later by the calling
 * routine.
 */
sec_PKCS12SafeBag *
sec_PKCS12CreateSafeBag(SEC_PKCS12ExportContext *p12ctxt, SECOidTag bagType,
                        void *bagData)
{
    sec_PKCS12SafeBag *safeBag;
    void *mark = NULL;
    SECStatus rv = SECSuccess;
    SECOidData *oidData = NULL;

    if (!p12ctxt) {
        return NULL;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);
    if (!mark) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    safeBag = (sec_PKCS12SafeBag *)PORT_ArenaZAlloc(p12ctxt->arena,
                                                    sizeof(sec_PKCS12SafeBag));
    if (!safeBag) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    /* set the bags content based upon bag type */
    switch (bagType) {
        case SEC_OID_PKCS12_V1_KEY_BAG_ID:
            safeBag->safeBagContent.pkcs8KeyBag =
                (SECKEYPrivateKeyInfo *)bagData;
            break;
        case SEC_OID_PKCS12_V1_CERT_BAG_ID:
            safeBag->safeBagContent.certBag = (sec_PKCS12CertBag *)bagData;
            break;
        case SEC_OID_PKCS12_V1_CRL_BAG_ID:
            safeBag->safeBagContent.crlBag = (sec_PKCS12CRLBag *)bagData;
            break;
        case SEC_OID_PKCS12_V1_SECRET_BAG_ID:
            safeBag->safeBagContent.secretBag = (sec_PKCS12SecretBag *)bagData;
            break;
        case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
            safeBag->safeBagContent.pkcs8ShroudedKeyBag =
                (SECKEYEncryptedPrivateKeyInfo *)bagData;
            break;
        case SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID:
            safeBag->safeBagContent.safeContents =
                (sec_PKCS12SafeContents *)bagData;
            break;
        default:
            goto loser;
    }

    oidData = SECOID_FindOIDByTag(bagType);
    if (oidData) {
        rv = SECITEM_CopyItem(p12ctxt->arena, &safeBag->safeBagType, &oidData->oid);
        if (rv != SECSuccess) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }
    } else {
        goto loser;
    }

    safeBag->arena = p12ctxt->arena;
    PORT_ArenaUnmark(p12ctxt->arena, mark);

    return safeBag;

loser:
    if (mark) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
    }

    return NULL;
}

/* Creates a new certificate bag and returns a pointer to it.  If an error
 * occurs NULL is returned.
 */
sec_PKCS12CertBag *
sec_PKCS12NewCertBag(PLArenaPool *arena, SECOidTag certType)
{
    sec_PKCS12CertBag *certBag = NULL;
    SECOidData *bagType = NULL;
    SECStatus rv;
    void *mark = NULL;

    if (!arena) {
        return NULL;
    }

    mark = PORT_ArenaMark(arena);
    certBag = (sec_PKCS12CertBag *)PORT_ArenaZAlloc(arena,
                                                    sizeof(sec_PKCS12CertBag));
    if (!certBag) {
        PORT_ArenaRelease(arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    bagType = SECOID_FindOIDByTag(certType);
    if (!bagType) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    rv = SECITEM_CopyItem(arena, &certBag->bagID, &bagType->oid);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    PORT_ArenaUnmark(arena, mark);
    return certBag;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}

/* Creates a new CRL bag and returns a pointer to it.  If an error
 * occurs NULL is returned.
 */
sec_PKCS12CRLBag *
sec_PKCS12NewCRLBag(PLArenaPool *arena, SECOidTag crlType)
{
    sec_PKCS12CRLBag *crlBag = NULL;
    SECOidData *bagType = NULL;
    SECStatus rv;
    void *mark = NULL;

    if (!arena) {
        return NULL;
    }

    mark = PORT_ArenaMark(arena);
    crlBag = (sec_PKCS12CRLBag *)PORT_ArenaZAlloc(arena,
                                                  sizeof(sec_PKCS12CRLBag));
    if (!crlBag) {
        PORT_ArenaRelease(arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    bagType = SECOID_FindOIDByTag(crlType);
    if (!bagType) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    rv = SECITEM_CopyItem(arena, &crlBag->bagID, &bagType->oid);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    PORT_ArenaUnmark(arena, mark);
    return crlBag;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}

/* sec_PKCS12AddAttributeToBag
 * adds an attribute to a safeBag.  currently, the only attributes supported
 * are those which are specified within PKCS 12.
 *
 *      p12ctxt - the export context
 *      safeBag - the safeBag to which attributes are appended
 *      attrType - the attribute type
 *      attrData - the attribute data
 */
SECStatus
sec_PKCS12AddAttributeToBag(SEC_PKCS12ExportContext *p12ctxt,
                            sec_PKCS12SafeBag *safeBag, SECOidTag attrType,
                            SECItem *attrData)
{
    sec_PKCS12Attribute *attribute;
    void *mark = NULL, *dummy = NULL;
    SECOidData *oiddata = NULL;
    SECItem unicodeName = { siBuffer, NULL, 0 };
    void *src = NULL;
    unsigned int nItems = 0;
    SECStatus rv;

    PORT_Assert(p12ctxt->arena == safeBag->arena);
    if (!safeBag || !p12ctxt || p12ctxt->arena != safeBag->arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    mark = PORT_ArenaMark(safeBag->arena);

    /* allocate the attribute */
    attribute = (sec_PKCS12Attribute *)PORT_ArenaZAlloc(safeBag->arena,
                                                        sizeof(sec_PKCS12Attribute));
    if (!attribute) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* set up the attribute */
    oiddata = SECOID_FindOIDByTag(attrType);
    if (!oiddata) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }
    if (SECITEM_CopyItem(p12ctxt->arena, &attribute->attrType, &oiddata->oid) !=
        SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    nItems = 1;
    switch (attrType) {
        case SEC_OID_PKCS9_LOCAL_KEY_ID: {
            src = attrData;
            break;
        }
        case SEC_OID_PKCS9_FRIENDLY_NAME: {
            if (!sec_pkcs12_convert_item_to_unicode(p12ctxt->arena,
                                                    &unicodeName, attrData, PR_FALSE,
                                                    PR_FALSE, PR_TRUE)) {
                goto loser;
            }
            src = &unicodeName;
            break;
        }
        default:
            goto loser;
    }

    /* append the attribute to the attribute value list  */
    attribute->attrValue = (SECItem **)PORT_ArenaZAlloc(p12ctxt->arena,
                                                        ((nItems + 1) * sizeof(SECItem *)));
    if (!attribute->attrValue) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* XXX this will need to be changed if attributes requiring more than
     * one element are ever used.
     */
    attribute->attrValue[0] = (SECItem *)PORT_ArenaZAlloc(p12ctxt->arena,
                                                          sizeof(SECItem));
    if (!attribute->attrValue[0]) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }
    attribute->attrValue[1] = NULL;

    rv = SECITEM_CopyItem(p12ctxt->arena, attribute->attrValue[0],
                          (SECItem *)src);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* append the attribute to the safeBag attributes */
    if (safeBag->nAttribs) {
        dummy = PORT_ArenaGrow(p12ctxt->arena, safeBag->attribs,
                               ((safeBag->nAttribs + 1) * sizeof(sec_PKCS12Attribute *)),
                               ((safeBag->nAttribs + 2) * sizeof(sec_PKCS12Attribute *)));
        safeBag->attribs = (sec_PKCS12Attribute **)dummy;
    } else {
        safeBag->attribs = (sec_PKCS12Attribute **)PORT_ArenaZAlloc(p12ctxt->arena,
                                                                    2 * sizeof(sec_PKCS12Attribute *));
        dummy = safeBag->attribs;
    }
    if (!dummy) {
        goto loser;
    }

    safeBag->attribs[safeBag->nAttribs] = attribute;
    safeBag->attribs[++safeBag->nAttribs] = NULL;

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    if (mark) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
    }

    return SECFailure;
}

/* SEC_PKCS12AddCert
 *      Adds a certificate to the data being exported.
 *
 *      p12ctxt - the export context
 *      safe - the safeInfo to which the certificate is placed
 *      nestedDest - if the cert is to be placed within a nested safeContents then,
 *                   this value is to be specified with the destination
 *      cert - the cert to export
 *      certDb - the certificate database handle
 *      keyId - a unique identifier to associate a certificate/key pair
 *      includeCertChain - PR_TRUE if the certificate chain is to be included.
 */
SECStatus
SEC_PKCS12AddCert(SEC_PKCS12ExportContext *p12ctxt, SEC_PKCS12SafeInfo *safe,
                  void *nestedDest, CERTCertificate *cert,
                  CERTCertDBHandle *certDb, SECItem *keyId,
                  PRBool includeCertChain)
{
    sec_PKCS12CertBag *certBag;
    sec_PKCS12SafeBag *safeBag;
    void *mark;
    SECStatus rv;
    SECItem nick = { siBuffer, NULL, 0 };

    if (!p12ctxt || !cert) {
        return SECFailure;
    }
    mark = PORT_ArenaMark(p12ctxt->arena);

    /* allocate the cert bag */
    certBag = sec_PKCS12NewCertBag(p12ctxt->arena,
                                   SEC_OID_PKCS9_X509_CERT);
    if (!certBag) {
        goto loser;
    }

    if (SECITEM_CopyItem(p12ctxt->arena, &certBag->value.x509Cert,
                         &cert->derCert) != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* if the cert chain is to be included, we should only be exporting
     * the cert from our internal database.
     */
    if (includeCertChain) {
        CERTCertificateList *certList = CERT_CertChainFromCert(cert,
                                                               certUsageSSLClient,
                                                               PR_TRUE);
        unsigned int count = 0;
        if (!certList) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }

        /* add cert chain */
        for (count = 0; count < (unsigned int)certList->len; count++) {
            if (SECITEM_CompareItem(&certList->certs[count], &cert->derCert) != SECEqual) {
                CERTCertificate *tempCert;

                /* decode the certificate */
                /* XXX
                 * This was rather silly.  The chain is constructed above
                 * by finding all of the CERTCertificate's in the database.
                 * Then the chain is put into a CERTCertificateList, which only
                 * contains the DER.  Finally, the DER was decoded, and the
                 * decoded cert was sent recursively back to this function.
                 * Beyond being inefficent, this causes data loss (specifically,
                 * the nickname).  Instead, for 3.4, we'll do a lookup by the
                 * DER, which should return the cached entry.
                 */
                tempCert = CERT_FindCertByDERCert(CERT_GetDefaultCertDB(),
                                                  &certList->certs[count]);
                if (!tempCert) {
                    CERT_DestroyCertificateList(certList);
                    goto loser;
                }

                /* add the certificate */
                if (SEC_PKCS12AddCert(p12ctxt, safe, nestedDest, tempCert,
                                      certDb, NULL, PR_FALSE) != SECSuccess) {
                    CERT_DestroyCertificate(tempCert);
                    CERT_DestroyCertificateList(certList);
                    goto loser;
                }
                CERT_DestroyCertificate(tempCert);
            }
        }
        CERT_DestroyCertificateList(certList);
    }

    /* if the certificate has a nickname, we will set the friendly name
     * to that.
     */
    if (cert->nickname) {
        if (cert->slot && !PK11_IsInternal(cert->slot)) {
            /*
             * The cert is coming off of an external token,
             * let's strip the token name from the nickname
             * and only add what comes after the colon as the
             * nickname. -javi
             */
            char *delimit;

            delimit = PORT_Strchr(cert->nickname, ':');
            if (delimit == NULL) {
                nick.data = (unsigned char *)cert->nickname;
                nick.len = PORT_Strlen(cert->nickname);
            } else {
                delimit++;
                nick.data = (unsigned char *)PORT_ArenaStrdup(p12ctxt->arena,
                                                              delimit);
                nick.len = PORT_Strlen(delimit);
            }
        } else {
            nick.data = (unsigned char *)cert->nickname;
            nick.len = PORT_Strlen(cert->nickname);
        }
    }

    safeBag = sec_PKCS12CreateSafeBag(p12ctxt, SEC_OID_PKCS12_V1_CERT_BAG_ID,
                                      certBag);
    if (!safeBag) {
        goto loser;
    }

    /* add the friendly name and keyId attributes, if necessary */
    if (nick.data) {
        if (sec_PKCS12AddAttributeToBag(p12ctxt, safeBag,
                                        SEC_OID_PKCS9_FRIENDLY_NAME, &nick) != SECSuccess) {
            goto loser;
        }
    }

    if (keyId) {
        if (sec_PKCS12AddAttributeToBag(p12ctxt, safeBag, SEC_OID_PKCS9_LOCAL_KEY_ID,
                                        keyId) != SECSuccess) {
            goto loser;
        }
    }

    /* append the cert safeBag */
    if (nestedDest) {
        rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena,
                                                    (sec_PKCS12SafeContents *)nestedDest,
                                                    safeBag);
    } else {
        rv = sec_pkcs12_append_bag(p12ctxt, safe, safeBag);
    }

    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    if (mark) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
    }

    return SECFailure;
}

/* SEC_PKCS12AddKeyForCert
 *      Extracts the key associated with a particular certificate and exports
 *      it.
 *
 *      p12ctxt - the export context
 *      safe - the safeInfo to place the key in
 *      nestedDest - the nested safeContents to place a key
 *      cert - the certificate which the key belongs to
 *      shroudKey - encrypt the private key for export.  This value should
 *              always be true.  lower level code will not allow the export
 *              of unencrypted private keys.
 *      algorithm - the algorithm with which to encrypt the private key
 *      pwitem - the password to encrypt the private key with
 *      keyId - the keyID attribute
 *      nickName - the nickname attribute
 */
SECStatus
SEC_PKCS12AddKeyForCert(SEC_PKCS12ExportContext *p12ctxt, SEC_PKCS12SafeInfo *safe,
                        void *nestedDest, CERTCertificate *cert,
                        PRBool shroudKey, SECOidTag algorithm, SECItem *pwitem,
                        SECItem *keyId, SECItem *nickName)
{
    void *mark;
    void *keyItem;
    SECOidTag keyType;
    SECStatus rv = SECFailure;
    SECItem nickname = { siBuffer, NULL, 0 }, uniPwitem = { siBuffer, NULL, 0 };
    sec_PKCS12SafeBag *returnBag;

    if (!p12ctxt || !cert || !safe) {
        return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    /* retrieve the key based upon the type that it is and
     * specify the type of safeBag to store the key in
     */
    if (!shroudKey) {

        /* extract the key unencrypted.  this will most likely go away */
        SECKEYPrivateKeyInfo *pki = PK11_ExportPrivateKeyInfo(cert,
                                                              p12ctxt->wincx);
        if (!pki) {
            PORT_ArenaRelease(p12ctxt->arena, mark);
            PORT_SetError(SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY);
            return SECFailure;
        }
        keyItem = PORT_ArenaZAlloc(p12ctxt->arena, sizeof(SECKEYPrivateKeyInfo));
        if (!keyItem) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }
        rv = SECKEY_CopyPrivateKeyInfo(p12ctxt->arena,
                                       (SECKEYPrivateKeyInfo *)keyItem, pki);
        keyType = SEC_OID_PKCS12_V1_KEY_BAG_ID;
        SECKEY_DestroyPrivateKeyInfo(pki, PR_TRUE);
    } else {

        /* extract the key encrypted */
        SECKEYEncryptedPrivateKeyInfo *epki = NULL;
        PK11SlotInfo *slot = NULL;
        SECOidTag prfAlg = SEC_OID_UNKNOWN;

        if (!sec_pkcs12_encode_password(p12ctxt->arena, &uniPwitem, algorithm,
                                        pwitem)) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }

        /* if we have password integrity set, use that to set the integrity
         * hash algorithm to set our password PRF. If we haven't set it, just
         * let the low level code pick it */
        if (p12ctxt->integrityEnabled && p12ctxt->pwdIntegrity) {
            prfAlg = HASH_GetHMACOidTagByHashOidTag(
                p12ctxt->integrityInfo.pwdInfo.algorithm);
        }

        /* we want to make sure to take the key out of the key slot */
        if (PK11_IsInternal(p12ctxt->slot)) {
            slot = PK11_GetInternalKeySlot();
        } else {
            slot = PK11_ReferenceSlot(p12ctxt->slot);
        }

        /* passing algorithm as the pbe will force the PBE code to
         * automatically handle the selection between using the algorithm
         * as a the pbe algorithm, or using the algorithm as a cipher
         * and building a pkcs5 pbe */
        epki = PK11_ExportEncryptedPrivateKeyInfoV2(slot, algorithm,
                                                    SEC_OID_UNKNOWN, prfAlg,
                                                    &uniPwitem, cert,
                                                    NSS_PBE_DEFAULT_ITERATION_COUNT,
                                                    p12ctxt->wincx);
        PK11_FreeSlot(slot);
        if (!epki) {
            PORT_SetError(SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY);
            goto loser;
        }

        keyItem = PORT_ArenaZAlloc(p12ctxt->arena,
                                   sizeof(SECKEYEncryptedPrivateKeyInfo));
        if (!keyItem) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }
        rv = SECKEY_CopyEncryptedPrivateKeyInfo(p12ctxt->arena,
                                                (SECKEYEncryptedPrivateKeyInfo *)keyItem,
                                                epki);
        keyType = SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID;
        SECKEY_DestroyEncryptedPrivateKeyInfo(epki, PR_TRUE);
    }

    if (rv != SECSuccess) {
        goto loser;
    }

    /* if no nickname specified, let's see if the certificate has a
     * nickname.
     */
    if (!nickName) {
        if (cert->nickname) {
            nickname.data = (unsigned char *)cert->nickname;
            nickname.len = PORT_Strlen(cert->nickname);
            nickName = &nickname;
        }
    }

    /* create the safe bag and set any attributes */
    returnBag = sec_PKCS12CreateSafeBag(p12ctxt, keyType, keyItem);
    if (!returnBag) {
        rv = SECFailure;
        goto loser;
    }

    if (nickName) {
        if (sec_PKCS12AddAttributeToBag(p12ctxt, returnBag,
                                        SEC_OID_PKCS9_FRIENDLY_NAME, nickName) != SECSuccess) {
            goto loser;
        }
    }

    if (keyId) {
        if (sec_PKCS12AddAttributeToBag(p12ctxt, returnBag, SEC_OID_PKCS9_LOCAL_KEY_ID,
                                        keyId) != SECSuccess) {
            goto loser;
        }
    }

    if (nestedDest) {
        rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena,
                                                    (sec_PKCS12SafeContents *)nestedDest,
                                                    returnBag);
    } else {
        rv = sec_pkcs12_append_bag(p12ctxt, safe, returnBag);
    }

loser:

    if (rv != SECSuccess) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
    } else {
        PORT_ArenaUnmark(p12ctxt->arena, mark);
    }

    return rv;
}

/* SEC_PKCS12AddCertOrChainAndKey
 *      Add a certificate and key pair to be exported.
 *
 *      p12ctxt          - the export context
 *      certSafe         - the safeInfo where the cert is stored
 *      certNestedDest   - the nested safeContents to store the cert
 *      keySafe          - the safeInfo where the key is stored
 *      keyNestedDest    - the nested safeContents to store the key
 *      shroudKey        - extract the private key encrypted?
 *      pwitem           - the password with which the key is encrypted
 *      algorithm        - the algorithm with which the key is encrypted
 *      includeCertChain - also add certs from chain to bag.
 */
SECStatus
SEC_PKCS12AddCertOrChainAndKey(SEC_PKCS12ExportContext *p12ctxt,
                               void *certSafe, void *certNestedDest,
                               CERTCertificate *cert, CERTCertDBHandle *certDb,
                               void *keySafe, void *keyNestedDest,
                               PRBool shroudKey, SECItem *pwitem,
                               SECOidTag algorithm, PRBool includeCertChain)
{
    SECStatus rv = SECFailure;
    SGNDigestInfo *digest = NULL;
    void *mark = NULL;

    if (!p12ctxt || !certSafe || !keySafe || !cert) {
        return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    /* generate the thumbprint of the cert to use as a keyId */
    digest = sec_pkcs12_compute_thumbprint(&cert->derCert);
    if (!digest) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
        return SECFailure;
    }

    /* add the certificate */
    rv = SEC_PKCS12AddCert(p12ctxt, (SEC_PKCS12SafeInfo *)certSafe,
                           (SEC_PKCS12SafeInfo *)certNestedDest, cert, certDb,
                           &digest->digest, includeCertChain);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* add the key */
    rv = SEC_PKCS12AddKeyForCert(p12ctxt, (SEC_PKCS12SafeInfo *)keySafe,
                                 keyNestedDest, cert,
                                 shroudKey, algorithm, pwitem,
                                 &digest->digest, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }

    SGN_DestroyDigestInfo(digest);

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    SGN_DestroyDigestInfo(digest);
    PORT_ArenaRelease(p12ctxt->arena, mark);

    return SECFailure;
}

/* like SEC_PKCS12AddCertOrChainAndKey, but always adds cert chain */
SECStatus
SEC_PKCS12AddCertAndKey(SEC_PKCS12ExportContext *p12ctxt,
                        void *certSafe, void *certNestedDest,
                        CERTCertificate *cert, CERTCertDBHandle *certDb,
                        void *keySafe, void *keyNestedDest,
                        PRBool shroudKey, SECItem *pwItem, SECOidTag algorithm)
{
    return SEC_PKCS12AddCertOrChainAndKey(p12ctxt, certSafe, certNestedDest,
                                          cert, certDb, keySafe, keyNestedDest, shroudKey, pwItem,
                                          algorithm, PR_TRUE);
}

/* SEC_PKCS12CreateNestedSafeContents
 *      Allows nesting of safe contents to be implemented.  No limit imposed on
 *      depth.
 *
 *      p12ctxt - the export context
 *      baseSafe - the base safeInfo
 *      nestedDest - a parent safeContents (?)
 */
void *
SEC_PKCS12CreateNestedSafeContents(SEC_PKCS12ExportContext *p12ctxt,
                                   void *baseSafe, void *nestedDest)
{
    sec_PKCS12SafeContents *newSafe;
    sec_PKCS12SafeBag *safeContentsBag;
    void *mark;
    SECStatus rv;

    if (!p12ctxt || !baseSafe) {
        return NULL;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    newSafe = sec_PKCS12CreateSafeContents(p12ctxt->arena);
    if (!newSafe) {
        PORT_ArenaRelease(p12ctxt->arena, mark);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    /* create the safeContents safeBag */
    safeContentsBag = sec_PKCS12CreateSafeBag(p12ctxt,
                                              SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID,
                                              newSafe);
    if (!safeContentsBag) {
        goto loser;
    }

    /* append the safeContents to the appropriate area */
    if (nestedDest) {
        rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena,
                                                    (sec_PKCS12SafeContents *)nestedDest,
                                                    safeContentsBag);
    } else {
        rv = sec_pkcs12_append_bag(p12ctxt, (SEC_PKCS12SafeInfo *)baseSafe,
                                   safeContentsBag);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return newSafe;

loser:
    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/*********************************
 * Encoding routines
 *********************************/

/* Clean up the resources allocated by a sec_PKCS12EncoderContext. */
static void
sec_pkcs12_encoder_destroy_context(sec_PKCS12EncoderContext *p12enc)
{
    if (p12enc) {
        if (p12enc->outerA1ecx) {
            SEC_ASN1EncoderFinish(p12enc->outerA1ecx);
            p12enc->outerA1ecx = NULL;
        }
        if (p12enc->aSafeCinfo) {
            SEC_PKCS7DestroyContentInfo(p12enc->aSafeCinfo);
            p12enc->aSafeCinfo = NULL;
        }
        if (p12enc->middleP7ecx) {
            SEC_PKCS7EncoderFinish(p12enc->middleP7ecx, p12enc->p12exp->pwfn,
                                   p12enc->p12exp->pwfnarg);
            p12enc->middleP7ecx = NULL;
        }
        if (p12enc->middleA1ecx) {
            SEC_ASN1EncoderFinish(p12enc->middleA1ecx);
            p12enc->middleA1ecx = NULL;
        }
        if (p12enc->hmacCx) {
            PK11_DestroyContext(p12enc->hmacCx, PR_TRUE);
            p12enc->hmacCx = NULL;
        }
    }
}

/* set up the encoder context based on information in the export context
 * and return the newly allocated enocoder context.  A return of NULL
 * indicates an error occurred.
 */
static sec_PKCS12EncoderContext *
sec_pkcs12_encoder_start_context(SEC_PKCS12ExportContext *p12exp)
{
    sec_PKCS12EncoderContext *p12enc = NULL;
    unsigned int i, nonEmptyCnt;
    SECStatus rv;
    SECItem ignore = { 0 };
    void *mark;
    SECItem *salt = NULL;
    SECItem *params = NULL;

    if (!p12exp || !p12exp->safeInfos) {
        return NULL;
    }

    /* check for any empty safes and skip them */
    i = nonEmptyCnt = 0;
    while (p12exp->safeInfos[i]) {
        if (p12exp->safeInfos[i]->itemCount) {
            nonEmptyCnt++;
        }
        i++;
    }
    if (nonEmptyCnt == 0) {
        return NULL;
    }
    p12exp->authSafe.encodedSafes[nonEmptyCnt] = NULL;

    /* allocate the encoder context */
    mark = PORT_ArenaMark(p12exp->arena);
    p12enc = PORT_ArenaZNew(p12exp->arena, sec_PKCS12EncoderContext);
    if (!p12enc) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    p12enc->arena = p12exp->arena;
    p12enc->p12exp = p12exp;

    /* set up the PFX version and information */
    PORT_Memset(&p12enc->pfx, 0, sizeof(sec_PKCS12PFXItem));
    if (!SEC_ASN1EncodeInteger(p12exp->arena, &(p12enc->pfx.version),
                               SEC_PKCS12_VERSION)) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* set up the authenticated safe content info based on the
     * type of integrity being used.  this should be changed to
     * enforce integrity mode, but will not be implemented until
     * it is confirmed that integrity must be in place
     */
    if (p12exp->integrityEnabled && !p12exp->pwdIntegrity) {
        /* create public key integrity mode */
        p12enc->aSafeCinfo = SEC_PKCS7CreateSignedData(
            p12exp->integrityInfo.pubkeyInfo.cert,
            certUsageEmailSigner,
            p12exp->integrityInfo.pubkeyInfo.certDb,
            p12exp->integrityInfo.pubkeyInfo.algorithm,
            NULL,
            p12exp->pwfn,
            p12exp->pwfnarg);
        if (!p12enc->aSafeCinfo) {
            goto loser;
        }
        if (SEC_PKCS7IncludeCertChain(p12enc->aSafeCinfo, NULL) != SECSuccess) {
            goto loser;
        }
        PORT_CheckSuccess(SEC_PKCS7AddSigningTime(p12enc->aSafeCinfo));
    } else {
        p12enc->aSafeCinfo = SEC_PKCS7CreateData();

        /* init password pased integrity mode */
        if (p12exp->integrityEnabled) {
            SECItem pwd = { siBuffer, NULL, 0 };
            PK11SymKey *symKey;
            CK_MECHANISM_TYPE integrityMechType;
            CK_MECHANISM_TYPE hmacMechType;
            salt = sec_pkcs12_generate_salt();

            /* zero out macData and set values */
            PORT_Memset(&p12enc->mac, 0, sizeof(sec_PKCS12MacData));

            if (!salt) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto loser;
            }
            if (SECITEM_CopyItem(p12exp->arena, &(p12enc->mac.macSalt), salt) != SECSuccess) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto loser;
            }
            if (!SEC_ASN1EncodeInteger(p12exp->arena, &(p12enc->mac.iter),
                                       NSS_PBE_DEFAULT_ITERATION_COUNT)) {
                goto loser;
            }

            /* generate HMAC key */
            if (!sec_pkcs12_convert_item_to_unicode(NULL, &pwd,
                                                    p12exp->integrityInfo.pwdInfo.password, PR_TRUE,
                                                    PR_TRUE, PR_TRUE)) {
                goto loser;
            }
            /*
             * This code only works with PKCS #12 Mac using PKCS #5 v1
             * PBA keygens. PKCS #5 v2 support will require a change to
             * the PKCS #12 spec.
             */
            params = PK11_CreatePBEParams(salt, &pwd,
                                          NSS_PBE_DEFAULT_ITERATION_COUNT);
            SECITEM_ZfreeItem(salt, PR_TRUE);
            salt = NULL;
            SECITEM_ZfreeItem(&pwd, PR_FALSE);

            /* get the PBA Mechanism to generate the key */
            integrityMechType = sec_pkcs12_algtag_to_keygen_mech(
                p12exp->integrityInfo.pwdInfo.algorithm);
            if (integrityMechType == CKM_INVALID_MECHANISM) {
                goto loser;
            }

            /* generate the key */
            symKey = PK11_KeyGen(NULL, integrityMechType, params, 20, NULL);
            PK11_DestroyPBEParams(params);
            if (!symKey) {
                goto loser;
            }

            /* initialize HMAC */
            /* Get the HMAC mechanism from the hash OID */
            hmacMechType = sec_pkcs12_algtag_to_mech(
                p12exp->integrityInfo.pwdInfo.algorithm);

            p12enc->hmacCx = PK11_CreateContextBySymKey(hmacMechType,
                                                        CKA_SIGN, symKey, &ignore);

            PK11_FreeSymKey(symKey);
            if (!p12enc->hmacCx) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto loser;
            }
            rv = PK11_DigestBegin(p12enc->hmacCx);
            if (rv != SECSuccess)
                goto loser;
        }
    }

    if (!p12enc->aSafeCinfo) {
        goto loser;
    }

    PORT_ArenaUnmark(p12exp->arena, mark);

    return p12enc;

loser:
    sec_pkcs12_encoder_destroy_context(p12enc);
    if (p12exp->arena != NULL)
        PORT_ArenaRelease(p12exp->arena, mark);
    if (salt) {
        SECITEM_ZfreeItem(salt, PR_TRUE);
    }
    if (params) {
        PK11_DestroyPBEParams(params);
    }

    return NULL;
}

/* The outermost ASN.1 encoder calls this function for output.
** This function calls back to the library caller's output routine,
** which typically writes to a PKCS12 file.
 */
static void
sec_P12A1OutputCB_Outer(void *arg, const char *buf, unsigned long len,
                        int depth, SEC_ASN1EncodingPart data_kind)
{
    struct sec_pkcs12_encoder_output *output;

    output = (struct sec_pkcs12_encoder_output *)arg;
    (*output->outputfn)(output->outputarg, buf, len);
}

/* The "middle" and "inner" ASN.1 encoders call this function to output.
** This function does HMACing, if appropriate, and then buffers the data.
** The buffered data is eventually passed down to the underlying PKCS7 encoder.
 */
static void
sec_P12A1OutputCB_HmacP7Update(void *arg, const char *buf,
                               unsigned long len,
                               int depth,
                               SEC_ASN1EncodingPart data_kind)
{
    sec_pkcs12OutputBuffer *bufcx = (sec_pkcs12OutputBuffer *)arg;

    if (!buf || !len)
        return;

    if (bufcx->hmacCx) {
        PK11_DigestOp(bufcx->hmacCx, (unsigned char *)buf, len);
    }

    /* buffer */
    if (bufcx->numBytes > 0) {
        int toCopy;
        if (len + bufcx->numBytes <= bufcx->bufBytes) {
            memcpy(bufcx->buf + bufcx->numBytes, buf, len);
            bufcx->numBytes += len;
            if (bufcx->numBytes < bufcx->bufBytes)
                return;
            SEC_PKCS7EncoderUpdate(bufcx->p7eCx, bufcx->buf, bufcx->bufBytes);
            bufcx->numBytes = 0;
            return;
        }
        toCopy = bufcx->bufBytes - bufcx->numBytes;
        memcpy(bufcx->buf + bufcx->numBytes, buf, toCopy);
        SEC_PKCS7EncoderUpdate(bufcx->p7eCx, bufcx->buf, bufcx->bufBytes);
        bufcx->numBytes = 0;
        len -= toCopy;
        buf += toCopy;
    }
    /* buffer is presently empty */
    if (len >= bufcx->bufBytes) {
        /* Just pass it through */
        SEC_PKCS7EncoderUpdate(bufcx->p7eCx, buf, len);
    } else {
        /* copy it all into the buffer, and return */
        memcpy(bufcx->buf, buf, len);
        bufcx->numBytes = len;
    }
}

void
sec_FlushPkcs12OutputBuffer(sec_pkcs12OutputBuffer *bufcx)
{
    if (bufcx->numBytes > 0) {
        SEC_PKCS7EncoderUpdate(bufcx->p7eCx, bufcx->buf, bufcx->numBytes);
        bufcx->numBytes = 0;
    }
}

/* Feeds the output of a PKCS7 encoder into the next outward ASN.1 encoder.
** This function is used by both the inner and middle PCS7 encoders.
*/
static void
sec_P12P7OutputCB_CallA1Update(void *arg, const char *buf, unsigned long len)
{
    SEC_ASN1EncoderContext *cx = (SEC_ASN1EncoderContext *)arg;

    if (!buf || !len)
        return;

    SEC_ASN1EncoderUpdate(cx, buf, len);
}

/* this function encodes content infos which are part of the
 * sequence of content infos labeled AuthenticatedSafes
 */
static SECStatus
sec_pkcs12_encoder_asafe_process(sec_PKCS12EncoderContext *p12ecx)
{
    SEC_PKCS7EncoderContext *innerP7ecx;
    SEC_PKCS7ContentInfo *cinfo;
    PK11SymKey *bulkKey = NULL;
    SEC_ASN1EncoderContext *innerA1ecx = NULL;
    SECStatus rv = SECSuccess;

    if (p12ecx->currentSafe < p12ecx->p12exp->authSafe.safeCount) {
        SEC_PKCS12SafeInfo *safeInfo;
        SECOidTag cinfoType;

        safeInfo = p12ecx->p12exp->safeInfos[p12ecx->currentSafe];

        /* skip empty safes */
        if (safeInfo->itemCount == 0) {
            return SECSuccess;
        }

        cinfo = safeInfo->cinfo;
        cinfoType = SEC_PKCS7ContentType(cinfo);

        /* determine the safe type and set the appropriate argument */
        switch (cinfoType) {
            case SEC_OID_PKCS7_DATA:
            case SEC_OID_PKCS7_ENVELOPED_DATA:
                break;
            case SEC_OID_PKCS7_ENCRYPTED_DATA:
                bulkKey = safeInfo->encryptionKey;
                PK11_SetSymKeyUserData(bulkKey, &safeInfo->pwitem, NULL);
                break;
            default:
                return SECFailure;
        }

        /* start the PKCS7 encoder */
        innerP7ecx = SEC_PKCS7EncoderStart(cinfo,
                                           sec_P12P7OutputCB_CallA1Update,
                                           p12ecx->middleA1ecx, bulkKey);
        if (!innerP7ecx) {
            goto loser;
        }

        /* encode safe contents */
        p12ecx->innerBuf.p7eCx = innerP7ecx;
        p12ecx->innerBuf.hmacCx = NULL;
        p12ecx->innerBuf.numBytes = 0;
        p12ecx->innerBuf.bufBytes = sizeof p12ecx->innerBuf.buf;

        innerA1ecx = SEC_ASN1EncoderStart(safeInfo->safe,
                                          sec_PKCS12SafeContentsTemplate,
                                          sec_P12A1OutputCB_HmacP7Update,
                                          &p12ecx->innerBuf);
        if (!innerA1ecx) {
            goto loser;
        }
        rv = SEC_ASN1EncoderUpdate(innerA1ecx, NULL, 0);
        SEC_ASN1EncoderFinish(innerA1ecx);
        sec_FlushPkcs12OutputBuffer(&p12ecx->innerBuf);
        innerA1ecx = NULL;
        if (rv != SECSuccess) {
            goto loser;
        }

        /* finish up safe content info */
        rv = SEC_PKCS7EncoderFinish(innerP7ecx, p12ecx->p12exp->pwfn,
                                    p12ecx->p12exp->pwfnarg);
    }
    memset(&p12ecx->innerBuf, 0, sizeof p12ecx->innerBuf);
    return SECSuccess;

loser:
    if (innerP7ecx) {
        SEC_PKCS7EncoderFinish(innerP7ecx, p12ecx->p12exp->pwfn,
                               p12ecx->p12exp->pwfnarg);
    }

    if (innerA1ecx) {
        SEC_ASN1EncoderFinish(innerA1ecx);
    }
    memset(&p12ecx->innerBuf, 0, sizeof p12ecx->innerBuf);
    return SECFailure;
}

/* finish the HMAC and encode the macData so that it can be
 * encoded.
 */
static SECStatus
sec_Pkcs12FinishMac(sec_PKCS12EncoderContext *p12ecx)
{
    unsigned char hmacData[HASH_LENGTH_MAX];
    unsigned int hmacLen;
    SECStatus rv;
    SGNDigestInfo *di = NULL;
    void *dummy;

    if (!p12ecx) {
        return SECFailure;
    }

    /* make sure we are using password integrity mode */
    if (!p12ecx->p12exp->integrityEnabled) {
        return SECSuccess;
    }

    if (!p12ecx->p12exp->pwdIntegrity) {
        return SECSuccess;
    }

    /* finish the hmac */

    rv = PK11_DigestFinal(p12ecx->hmacCx, hmacData, &hmacLen, HASH_LENGTH_MAX);

    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* create the digest info */
    di = SGN_CreateDigestInfo(p12ecx->p12exp->integrityInfo.pwdInfo.algorithm,
                              hmacData, hmacLen);
    if (!di) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        rv = SECFailure;
        goto loser;
    }

    rv = SGN_CopyDigestInfo(p12ecx->arena, &p12ecx->mac.safeMac, di);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* encode the mac data */
    dummy = SEC_ASN1EncodeItem(p12ecx->arena, &p12ecx->pfx.encodedMacData,
                               &p12ecx->mac, sec_PKCS12MacDataTemplate);
    if (!dummy) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        rv = SECFailure;
    }

loser:
    if (di) {
        SGN_DestroyDigestInfo(di);
    }
    PK11_DestroyContext(p12ecx->hmacCx, PR_TRUE);
    p12ecx->hmacCx = NULL;
    PORT_Memset(hmacData, 0, hmacLen);

    return rv;
}

/* pfx notify function for ASN1 encoder.
 * We want to stop encoding once we reach the authenticated safe.
 * At that point, the encoder will be updated via streaming
 * as the authenticated safe is  encoded.
 */
static void
sec_pkcs12_encoder_pfx_notify(void *arg, PRBool before, void *dest, int real_depth)
{
    sec_PKCS12EncoderContext *p12ecx;

    if (!before) {
        return;
    }

    /* look for authenticated safe */
    p12ecx = (sec_PKCS12EncoderContext *)arg;
    if (dest != &p12ecx->pfx.encodedAuthSafe) {
        return;
    }

    SEC_ASN1EncoderSetTakeFromBuf(p12ecx->outerA1ecx);
    SEC_ASN1EncoderSetStreaming(p12ecx->outerA1ecx);
    SEC_ASN1EncoderClearNotifyProc(p12ecx->outerA1ecx);
}

/* SEC_PKCS12Encode
 *      Encodes the PFX item and returns it to the output function, via
 *      callback.  the output function must be capable of multiple updates.
 *
 *      p12exp - the export context
 *      output - the output function callback, will be called more than once,
 *               must be able to accept streaming data.
 *      outputarg - argument for the output callback.
 */
SECStatus
SEC_PKCS12Encode(SEC_PKCS12ExportContext *p12exp,
                 SEC_PKCS12EncoderOutputCallback output, void *outputarg)
{
    sec_PKCS12EncoderContext *p12enc;
    struct sec_pkcs12_encoder_output outInfo;
    SECStatus rv;

    if (!p12exp || !output) {
        return SECFailure;
    }

    /* get the encoder context */
    p12enc = sec_pkcs12_encoder_start_context(p12exp);
    if (!p12enc) {
        return SECFailure;
    }

    outInfo.outputfn = output;
    outInfo.outputarg = outputarg;

    /* set up PFX encoder, the "outer" encoder.  Set it for streaming */
    p12enc->outerA1ecx = SEC_ASN1EncoderStart(&p12enc->pfx,
                                              sec_PKCS12PFXItemTemplate,
                                              sec_P12A1OutputCB_Outer,
                                              &outInfo);
    if (!p12enc->outerA1ecx) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        rv = SECFailure;
        goto loser;
    }
    SEC_ASN1EncoderSetStreaming(p12enc->outerA1ecx);
    SEC_ASN1EncoderSetNotifyProc(p12enc->outerA1ecx,
                                 sec_pkcs12_encoder_pfx_notify, p12enc);
    rv = SEC_ASN1EncoderUpdate(p12enc->outerA1ecx, NULL, 0);
    if (rv != SECSuccess) {
        rv = SECFailure;
        goto loser;
    }

    /* set up asafe cinfo - the output of the encoder feeds the PFX encoder */
    p12enc->middleP7ecx = SEC_PKCS7EncoderStart(p12enc->aSafeCinfo,
                                                sec_P12P7OutputCB_CallA1Update,
                                                p12enc->outerA1ecx, NULL);
    if (!p12enc->middleP7ecx) {
        rv = SECFailure;
        goto loser;
    }

    /* encode asafe */
    p12enc->middleBuf.p7eCx = p12enc->middleP7ecx;
    p12enc->middleBuf.hmacCx = NULL;
    p12enc->middleBuf.numBytes = 0;
    p12enc->middleBuf.bufBytes = sizeof p12enc->middleBuf.buf;

    /* Setup the "inner ASN.1 encoder for Authenticated Safes.  */
    if (p12enc->p12exp->integrityEnabled &&
        p12enc->p12exp->pwdIntegrity) {
        p12enc->middleBuf.hmacCx = p12enc->hmacCx;
    }
    p12enc->middleA1ecx = SEC_ASN1EncoderStart(&p12enc->p12exp->authSafe,
                                               sec_PKCS12AuthenticatedSafeTemplate,
                                               sec_P12A1OutputCB_HmacP7Update,
                                               &p12enc->middleBuf);
    if (!p12enc->middleA1ecx) {
        rv = SECFailure;
        goto loser;
    }
    SEC_ASN1EncoderSetStreaming(p12enc->middleA1ecx);
    SEC_ASN1EncoderSetTakeFromBuf(p12enc->middleA1ecx);

    /* encode each of the safes */
    while (p12enc->currentSafe != p12enc->p12exp->safeInfoCount) {
        sec_pkcs12_encoder_asafe_process(p12enc);
        p12enc->currentSafe++;
    }
    SEC_ASN1EncoderClearTakeFromBuf(p12enc->middleA1ecx);
    SEC_ASN1EncoderClearStreaming(p12enc->middleA1ecx);
    SEC_ASN1EncoderUpdate(p12enc->middleA1ecx, NULL, 0);
    SEC_ASN1EncoderFinish(p12enc->middleA1ecx);
    p12enc->middleA1ecx = NULL;

    sec_FlushPkcs12OutputBuffer(&p12enc->middleBuf);

    /* finish the encoding of the authenticated safes */
    rv = SEC_PKCS7EncoderFinish(p12enc->middleP7ecx, p12exp->pwfn,
                                p12exp->pwfnarg);
    p12enc->middleP7ecx = NULL;
    if (rv != SECSuccess) {
        goto loser;
    }

    SEC_ASN1EncoderClearTakeFromBuf(p12enc->outerA1ecx);
    SEC_ASN1EncoderClearStreaming(p12enc->outerA1ecx);

    /* update the mac, if necessary */
    rv = sec_Pkcs12FinishMac(p12enc);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* finish encoding the pfx */
    rv = SEC_ASN1EncoderUpdate(p12enc->outerA1ecx, NULL, 0);

    SEC_ASN1EncoderFinish(p12enc->outerA1ecx);
    p12enc->outerA1ecx = NULL;

loser:
    sec_pkcs12_encoder_destroy_context(p12enc);
    return rv;
}

void
SEC_PKCS12DestroyExportContext(SEC_PKCS12ExportContext *p12ecx)
{
    int i = 0;

    if (!p12ecx) {
        return;
    }

    if (p12ecx->safeInfos) {
        i = 0;
        while (p12ecx->safeInfos[i] != NULL) {
            if (p12ecx->safeInfos[i]->encryptionKey) {
                PK11_FreeSymKey(p12ecx->safeInfos[i]->encryptionKey);
            }
            if (p12ecx->safeInfos[i]->cinfo) {
                SEC_PKCS7DestroyContentInfo(p12ecx->safeInfos[i]->cinfo);
            }
            i++;
        }
    }

    PK11_FreeSlot(p12ecx->slot);

    PORT_FreeArena(p12ecx->arena, PR_TRUE);
}
