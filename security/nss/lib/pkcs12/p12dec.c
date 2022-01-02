/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pkcs12.h"
#include "plarena.h"
#include "secpkcs7.h"
#include "p12local.h"
#include "secoid.h"
#include "secitem.h"
#include "secport.h"
#include "secasn1.h"
#include "secder.h"
#include "secerr.h"
#include "cert.h"
#include "certdb.h"
#include "p12plcy.h"
#include "p12.h"
#include "secpkcs5.h"

/* PFX extraction and validation routines */

/* decode the DER encoded PFX item.  if unable to decode, check to see if it
 * is an older PFX item.  If that fails, assume the file was not a valid
 * pfx file.
 * the returned pfx structure should be destroyed using SEC_PKCS12DestroyPFX
 */
static SEC_PKCS12PFXItem *
sec_pkcs12_decode_pfx(SECItem *der_pfx)
{
    SEC_PKCS12PFXItem *pfx;
    SECStatus rv;

    if (der_pfx == NULL) {
        return NULL;
    }

    /* allocate the space for a new PFX item */
    pfx = sec_pkcs12_new_pfx();
    if (pfx == NULL) {
        return NULL;
    }

    rv = SEC_ASN1DecodeItem(pfx->poolp, pfx, SEC_PKCS12PFXItemTemplate,
                            der_pfx);

    /* if a failure occurred, check for older version...
     * we also get rid of the old pfx structure, because we don't
     * know where it failed and what data in may contain
     */
    if (rv != SECSuccess) {
        SEC_PKCS12DestroyPFX(pfx);
        pfx = sec_pkcs12_new_pfx();
        if (pfx == NULL) {
            return NULL;
        }
        rv = SEC_ASN1DecodeItem(pfx->poolp, pfx, SEC_PKCS12PFXItemTemplate_OLD,
                                der_pfx);
        if (rv != SECSuccess) {
            PORT_SetError(SEC_ERROR_PKCS12_DECODING_PFX);
            PORT_FreeArena(pfx->poolp, PR_TRUE);
            return NULL;
        }
        pfx->old = PR_TRUE;
        rv = SGN_CopyDigestInfo(pfx->poolp, &pfx->macData.safeMac, &pfx->old_safeMac);
        if (rv != SECSuccess) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_FreeArena(pfx->poolp, PR_TRUE);
            return NULL;
        }
        rv = SECITEM_CopyItem(pfx->poolp, &pfx->macData.macSalt, &pfx->old_macSalt);
        if (rv != SECSuccess) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_FreeArena(pfx->poolp, PR_TRUE);
            return NULL;
        }
    } else {
        pfx->old = PR_FALSE;
    }

    /* convert bit string from bits to bytes */
    pfx->macData.macSalt.len /= 8;

    return pfx;
}

/* validate the integrity MAC used in the PFX.  The MAC is generated
 * per the PKCS 12 document.  If the MAC is incorrect, it is most likely
 * due to an invalid password.
 * pwitem is the integrity password
 * pfx is the decoded pfx item
 */
static PRBool
sec_pkcs12_check_pfx_mac(SEC_PKCS12PFXItem *pfx,
                         SECItem *pwitem)
{
    SECItem *key = NULL, *mac = NULL, *data = NULL;
    SECItem *vpwd = NULL;
    SECOidTag algorithm;
    PRBool ret = PR_FALSE;

    if (pfx == NULL) {
        return PR_FALSE;
    }

    algorithm = SECOID_GetAlgorithmTag(&pfx->macData.safeMac.digestAlgorithm);
    switch (algorithm) {
        /* only SHA1 hashing supported as a MACing algorithm */
        case SEC_OID_SHA1:
            if (pfx->old == PR_FALSE) {
                pfx->swapUnicode = PR_FALSE;
            }

        recheckUnicodePassword:
            vpwd = sec_pkcs12_create_virtual_password(pwitem,
                                                      &pfx->macData.macSalt,
                                                      pfx->swapUnicode);
            if (vpwd == NULL) {
                return PR_FALSE;
            }

            key = sec_pkcs12_generate_key_from_password(algorithm,
                                                        &pfx->macData.macSalt,
                                                        (pfx->old ? pwitem : vpwd));
            /* free vpwd only for newer PFX */
            if (vpwd) {
                SECITEM_ZfreeItem(vpwd, PR_TRUE);
            }
            if (key == NULL) {
                return PR_FALSE;
            }

            data = SEC_PKCS7GetContent(&pfx->authSafe);
            if (data == NULL) {
                break;
            }

            /* check MAC */
            mac = sec_pkcs12_generate_mac(key, data, pfx->old);
            ret = PR_TRUE;
            if (mac) {
                SECItem *safeMac = &pfx->macData.safeMac.digest;
                if (SECITEM_CompareItem(mac, safeMac) != SECEqual) {

                    /* if we encounter an invalid mac, lets invert the
                     * password in case of unicode changes
                     */
                    if (((!pfx->old) && pfx->swapUnicode) || (pfx->old)) {
                        PORT_SetError(SEC_ERROR_PKCS12_INVALID_MAC);
                        ret = PR_FALSE;
                    } else {
                        SECITEM_ZfreeItem(mac, PR_TRUE);
                        pfx->swapUnicode = PR_TRUE;
                        goto recheckUnicodePassword;
                    }
                }
                SECITEM_ZfreeItem(mac, PR_TRUE);
            } else {
                ret = PR_FALSE;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM);
            ret = PR_FALSE;
            break;
    }

    /* let success fall through */
    if (key != NULL)
        SECITEM_ZfreeItem(key, PR_TRUE);

    return ret;
}

/* check the validity of the pfx structure.  we currently only support
 * password integrity mode, so we check the MAC.
 */
static PRBool
sec_pkcs12_validate_pfx(SEC_PKCS12PFXItem *pfx,
                        SECItem *pwitem)
{
    SECOidTag contentType;

    contentType = SEC_PKCS7ContentType(&pfx->authSafe);
    switch (contentType) {
        case SEC_OID_PKCS7_DATA:
            return sec_pkcs12_check_pfx_mac(pfx, pwitem);
            break;
        case SEC_OID_PKCS7_SIGNED_DATA:
        default:
            PORT_SetError(SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE);
            break;
    }

    return PR_FALSE;
}

/* decode and return the valid PFX.  if the PFX item is not valid,
 * NULL is returned.
 */
static SEC_PKCS12PFXItem *
sec_pkcs12_get_pfx(SECItem *pfx_data,
                   SECItem *pwitem)
{
    SEC_PKCS12PFXItem *pfx;
    PRBool valid_pfx;

    if ((pfx_data == NULL) || (pwitem == NULL)) {
        return NULL;
    }

    pfx = sec_pkcs12_decode_pfx(pfx_data);
    if (pfx == NULL) {
        return NULL;
    }

    valid_pfx = sec_pkcs12_validate_pfx(pfx, pwitem);
    if (valid_pfx != PR_TRUE) {
        SEC_PKCS12DestroyPFX(pfx);
        pfx = NULL;
    }

    return pfx;
}

/* authenticated safe decoding, validation, and access routines
 */

/* convert dogbert beta 3 authenticated safe structure to a post
 * beta three structure, so that we don't have to change more routines.
 */
static SECStatus
sec_pkcs12_convert_old_auth_safe(SEC_PKCS12AuthenticatedSafe *asafe)
{
    SEC_PKCS12Baggage *baggage;
    SEC_PKCS12BaggageItem *bag;
    SECStatus rv = SECSuccess;

    if (asafe->old_baggage.espvks == NULL) {
        /* XXX should the ASN1 engine produce a single NULL element list
         * rather than setting the pointer to NULL?
         * There is no need to return an error -- assume that the list
         * was empty.
         */
        return SECSuccess;
    }

    baggage = sec_pkcs12_create_baggage(asafe->poolp);
    if (!baggage) {
        return SECFailure;
    }
    bag = sec_pkcs12_create_external_bag(baggage);
    if (!bag) {
        return SECFailure;
    }

    PORT_Memcpy(&asafe->baggage, baggage, sizeof(SEC_PKCS12Baggage));

    /* if there are shrouded keys, append them to the bag */
    rv = SECSuccess;
    if (asafe->old_baggage.espvks[0] != NULL) {
        int nEspvk = 0;
        rv = SECSuccess;
        while ((asafe->old_baggage.espvks[nEspvk] != NULL) &&
               (rv == SECSuccess)) {
            rv = sec_pkcs12_append_shrouded_key(bag,
                                                asafe->old_baggage.espvks[nEspvk]);
            nEspvk++;
        }
    }

    return rv;
}

/* decodes the authenticated safe item.  a return of NULL indicates
 * an error.  however, the error will have occurred either in memory
 * allocation or in decoding the authenticated safe.
 *
 * if an old PFX item has been found, we want to convert the
 * old authenticated safe to the new one.
 */
static SEC_PKCS12AuthenticatedSafe *
sec_pkcs12_decode_authenticated_safe(SEC_PKCS12PFXItem *pfx)
{
    SECItem *der_asafe = NULL;
    SEC_PKCS12AuthenticatedSafe *asafe = NULL;
    SECStatus rv;

    if (pfx == NULL) {
        return NULL;
    }

    der_asafe = SEC_PKCS7GetContent(&pfx->authSafe);
    if (der_asafe == NULL) {
        /* XXX set error ? */
        goto loser;
    }

    asafe = sec_pkcs12_new_asafe(pfx->poolp);
    if (asafe == NULL) {
        goto loser;
    }

    if (pfx->old == PR_FALSE) {
        rv = SEC_ASN1DecodeItem(pfx->poolp, asafe,
                                SEC_PKCS12AuthenticatedSafeTemplate,
                                der_asafe);
        asafe->old = PR_FALSE;
        asafe->swapUnicode = pfx->swapUnicode;
    } else {
        /* handle beta exported files */
        rv = SEC_ASN1DecodeItem(pfx->poolp, asafe,
                                SEC_PKCS12AuthenticatedSafeTemplate_OLD,
                                der_asafe);
        asafe->safe = &(asafe->old_safe);
        rv = sec_pkcs12_convert_old_auth_safe(asafe);
        asafe->old = PR_TRUE;
    }

    if (rv != SECSuccess) {
        goto loser;
    }

    asafe->poolp = pfx->poolp;

    return asafe;

loser:
    return NULL;
}

/* validates the safe within the authenticated safe item.
 * in order to be valid:
 *  1.  the privacy salt must be present
 *  2.  the encryption algorithm must be supported (including
 *      export policy)
 * PR_FALSE indicates an error, PR_TRUE indicates a valid safe
 */
static PRBool
sec_pkcs12_validate_encrypted_safe(SEC_PKCS12AuthenticatedSafe *asafe)
{
    PRBool valid = PR_FALSE;
    SECAlgorithmID *algid;

    if (asafe == NULL) {
        return PR_FALSE;
    }

    /* if mode is password privacy, then privacySalt is assumed
     * to be non-zero.
     */
    if (asafe->privacySalt.len != 0) {
        valid = PR_TRUE;
        asafe->privacySalt.len /= 8;
    } else {
        PORT_SetError(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE);
        return PR_FALSE;
    }

    /* until spec changes, content will have between 2 and 8 bytes depending
     * upon the algorithm used if certs are unencrypted...
     * also want to support case where content is empty -- which we produce
     */
    if (SEC_PKCS7IsContentEmpty(asafe->safe, 8) == PR_TRUE) {
        asafe->emptySafe = PR_TRUE;
        return PR_TRUE;
    }

    asafe->emptySafe = PR_FALSE;

    /* make sure that a pbe algorithm is being used */
    algid = SEC_PKCS7GetEncryptionAlgorithm(asafe->safe);
    if (algid != NULL) {
        if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
            valid = SEC_PKCS12DecryptionAllowed(algid);

            if (valid == PR_FALSE) {
                PORT_SetError(SEC_ERROR_BAD_EXPORT_ALGORITHM);
            }
        } else {
            PORT_SetError(SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM);
            valid = PR_FALSE;
        }
    } else {
        valid = PR_FALSE;
        PORT_SetError(SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM);
    }

    return valid;
}

/* validates authenticates safe:
 *  1.  checks that the version is supported
 *  2.  checks that only password privacy mode is used (currently)
 *  3.  further, makes sure safe has appropriate policies per above function
 * PR_FALSE indicates failure.
 */
static PRBool
sec_pkcs12_validate_auth_safe(SEC_PKCS12AuthenticatedSafe *asafe)
{
    PRBool valid = PR_TRUE;
    SECOidTag safe_type;
    int version;

    if (asafe == NULL) {
        return PR_FALSE;
    }

    /* check version, since it is default it may not be present.
     * therefore, assume ok
     */
    if ((asafe->version.len > 0) && (asafe->old == PR_FALSE)) {
        version = DER_GetInteger(&asafe->version);
        if (version > SEC_PKCS12_PFX_VERSION) {
            PORT_SetError(SEC_ERROR_PKCS12_UNSUPPORTED_VERSION);
            return PR_FALSE;
        }
    }

    /* validate password mode is being used */
    safe_type = SEC_PKCS7ContentType(asafe->safe);
    switch (safe_type) {
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            valid = sec_pkcs12_validate_encrypted_safe(asafe);
            break;
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        default:
            PORT_SetError(SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE);
            valid = PR_FALSE;
            break;
    }

    return valid;
}

/* retrieves the authenticated safe item from the PFX item
 *  before returning the authenticated safe, the validity of the
 *  authenticated safe is checked and if valid, returned.
 * a return of NULL indicates that an error occurred.
 */
static SEC_PKCS12AuthenticatedSafe *
sec_pkcs12_get_auth_safe(SEC_PKCS12PFXItem *pfx)
{
    SEC_PKCS12AuthenticatedSafe *asafe;
    PRBool valid_safe;

    if (pfx == NULL) {
        return NULL;
    }

    asafe = sec_pkcs12_decode_authenticated_safe(pfx);
    if (asafe == NULL) {
        return NULL;
    }

    valid_safe = sec_pkcs12_validate_auth_safe(asafe);
    if (valid_safe != PR_TRUE) {
        asafe = NULL;
    } else if (asafe) {
        asafe->baggage.poolp = asafe->poolp;
    }

    return asafe;
}

/* decrypts the authenticated safe.
 * a return of anything but SECSuccess indicates an error.  the
 * password is not known to be valid until the call to the
 * function sec_pkcs12_get_safe_contents.  If decoding the safe
 * fails, it is assumed the password was incorrect and the error
 * is set then.  any failure here is assumed to be due to
 * internal problems in SEC_PKCS7DecryptContents or below.
 */
static SECStatus
sec_pkcs12_decrypt_auth_safe(SEC_PKCS12AuthenticatedSafe *asafe,
                             SECItem *pwitem,
                             void *wincx)
{
    SECStatus rv = SECFailure;
    SECItem *vpwd = NULL;

    if ((asafe == NULL) || (pwitem == NULL)) {
        return SECFailure;
    }

    if (asafe->old == PR_FALSE) {
        vpwd = sec_pkcs12_create_virtual_password(pwitem, &asafe->privacySalt,
                                                  asafe->swapUnicode);
        if (vpwd == NULL) {
            return SECFailure;
        }
    }

    rv = SEC_PKCS7DecryptContents(asafe->poolp, asafe->safe,
                                  (asafe->old ? pwitem : vpwd), wincx);

    if (asafe->old == PR_FALSE) {
        SECITEM_ZfreeItem(vpwd, PR_TRUE);
    }

    return rv;
}

/* extract the safe from the authenticated safe.
 *  if we are unable to decode the safe, then it is likely that the
 *  safe has not been decrypted or the password used to decrypt
 *  the safe was invalid.  we assume that the password was invalid and
 *  set an error accordingly.
 * a return of NULL indicates that an error occurred.
 */
static SEC_PKCS12SafeContents *
sec_pkcs12_get_safe_contents(SEC_PKCS12AuthenticatedSafe *asafe)
{
    SECItem *src = NULL;
    SEC_PKCS12SafeContents *safe = NULL;
    SECStatus rv = SECFailure;

    if (asafe == NULL) {
        return NULL;
    }

    safe = (SEC_PKCS12SafeContents *)PORT_ArenaZAlloc(asafe->poolp,
                                                      sizeof(SEC_PKCS12SafeContents));
    if (safe == NULL) {
        return NULL;
    }
    safe->poolp = asafe->poolp;
    safe->old = asafe->old;
    safe->swapUnicode = asafe->swapUnicode;

    src = SEC_PKCS7GetContent(asafe->safe);
    if (src != NULL) {
        const SEC_ASN1Template *theTemplate;
        if (asafe->old != PR_TRUE) {
            theTemplate = SEC_PKCS12SafeContentsTemplate;
        } else {
            theTemplate = SEC_PKCS12SafeContentsTemplate_OLD;
        }

        rv = SEC_ASN1DecodeItem(asafe->poolp, safe, theTemplate, src);

        /* if we could not decode the item, password was probably invalid */
        if (rv != SECSuccess) {
            safe = NULL;
            PORT_SetError(SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT);
        }
    } else {
        PORT_SetError(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE);
        rv = SECFailure;
    }

    return safe;
}

/* import PFX item
 * der_pfx is the der encoded pfx structure
 * pbef and pbearg are the integrity/encryption password call back
 * ncCall is the nickname collision calllback
 * slot is the destination token
 * wincx window handler
 *
 * on error, error code set and SECFailure returned
 */
SECStatus
SEC_PKCS12PutPFX(SECItem *der_pfx, SECItem *pwitem,
                 SEC_PKCS12NicknameCollisionCallback ncCall,
                 PK11SlotInfo *slot,
                 void *wincx)
{
    SEC_PKCS12PFXItem *pfx;
    SEC_PKCS12AuthenticatedSafe *asafe;
    SEC_PKCS12SafeContents *safe_contents = NULL;
    SECStatus rv;

    if (!der_pfx || !pwitem || !slot) {
        return SECFailure;
    }

    /* decode and validate each section */
    rv = SECFailure;

    pfx = sec_pkcs12_get_pfx(der_pfx, pwitem);
    if (pfx != NULL) {
        asafe = sec_pkcs12_get_auth_safe(pfx);
        if (asafe != NULL) {

            /* decrypt safe -- only if not empty */
            if (asafe->emptySafe != PR_TRUE) {
                rv = sec_pkcs12_decrypt_auth_safe(asafe, pwitem, wincx);
                if (rv == SECSuccess) {
                    safe_contents = sec_pkcs12_get_safe_contents(asafe);
                    if (safe_contents == NULL) {
                        rv = SECFailure;
                    }
                }
            } else {
                safe_contents = sec_pkcs12_create_safe_contents(asafe->poolp);
                if (safe_contents == NULL) {
                    rv = SECFailure;
                } else {
                    safe_contents->swapUnicode = pfx->swapUnicode;
                    rv = SECSuccess;
                }
            }

            /* get safe contents and begin import */
            if (rv == SECSuccess) {
                SEC_PKCS12DecoderContext *p12dcx;

                p12dcx = sec_PKCS12ConvertOldSafeToNew(pfx->poolp, slot,
                                                       pfx->swapUnicode,
                                                       pwitem, wincx, safe_contents,
                                                       &asafe->baggage);
                if (!p12dcx) {
                    rv = SECFailure;
                    goto loser;
                }

                if (SEC_PKCS12DecoderValidateBags(p12dcx, ncCall) != SECSuccess) {
                    rv = SECFailure;
                    goto loser;
                }

                rv = SEC_PKCS12DecoderImportBags(p12dcx);
            }
        }
    }

loser:

    if (pfx) {
        SEC_PKCS12DestroyPFX(pfx);
    }

    return rv;
}

PRBool
SEC_PKCS12ValidData(char *buf, int bufLen, long int totalLength)
{
    int lengthLength;

    PRBool valid = PR_FALSE;

    if (buf == NULL) {
        return PR_FALSE;
    }

    /* check for constructed sequence identifier tag */
    if (*buf == (SEC_ASN1_CONSTRUCTED | SEC_ASN1_SEQUENCE)) {
        totalLength--; /* header byte taken care of */
        buf++;

        lengthLength = (long int)SEC_ASN1LengthLength(totalLength - 1);
        if (totalLength > 0x7f) {
            lengthLength--;
            *buf &= 0x7f; /* remove bit 8 indicator */
            if ((*buf - (char)lengthLength) == 0) {
                valid = PR_TRUE;
            }
        } else {
            lengthLength--;
            if ((*buf - (char)lengthLength) == 0) {
                valid = PR_TRUE;
            }
        }
    }

    return valid;
}
