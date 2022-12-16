/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nssrenam.h"
#include "pkcs12.h"
#include "secpkcs7.h"
#include "secasn1.h"
#include "seccomon.h"
#include "secoid.h"
#include "sechash.h"
#include "secitem.h"
#include "secerr.h"
#include "pk11func.h"
#include "p12local.h"
#include "p12.h"

#define SALT_LENGTH 16

SEC_ASN1_MKSUB(SECKEY_PrivateKeyInfoTemplate)
SEC_ASN1_MKSUB(sgn_DigestInfoTemplate)

CK_MECHANISM_TYPE
sec_pkcs12_algtag_to_mech(SECOidTag algtag)
{
    switch (algtag) {
        case SEC_OID_MD2:
            return CKM_MD2_HMAC;
        case SEC_OID_MD5:
            return CKM_MD5_HMAC;
        case SEC_OID_SHA1:
            return CKM_SHA_1_HMAC;
        case SEC_OID_SHA224:
            return CKM_SHA224_HMAC;
        case SEC_OID_SHA256:
            return CKM_SHA256_HMAC;
        case SEC_OID_SHA384:
            return CKM_SHA384_HMAC;
        case SEC_OID_SHA512:
            return CKM_SHA512_HMAC;
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}

CK_MECHANISM_TYPE
sec_pkcs12_algtag_to_keygen_mech(SECOidTag algtag)
{
    switch (algtag) {
        case SEC_OID_SHA1:
            return CKM_NSS_PBE_SHA1_HMAC_KEY_GEN;
            break;
        case SEC_OID_MD5:
            return CKM_NSS_PBE_MD5_HMAC_KEY_GEN;
            break;
        case SEC_OID_MD2:
            return CKM_NSS_PBE_MD2_HMAC_KEY_GEN;
            break;
        case SEC_OID_SHA224:
            return CKM_NSS_PKCS12_PBE_SHA224_HMAC_KEY_GEN;
            break;
        case SEC_OID_SHA256:
            return CKM_NSS_PKCS12_PBE_SHA256_HMAC_KEY_GEN;
            break;
        case SEC_OID_SHA384:
            return CKM_NSS_PKCS12_PBE_SHA384_HMAC_KEY_GEN;
            break;
        case SEC_OID_SHA512:
            return CKM_NSS_PKCS12_PBE_SHA512_HMAC_KEY_GEN;
            break;
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}

/* helper functions */
/* returns proper bag type template based upon object type tag */
const SEC_ASN1Template *
sec_pkcs12_choose_bag_type_old(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS12SafeBag *safebag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    safebag = (SEC_PKCS12SafeBag *)src_or_dest;

    oiddata = safebag->safeBagTypeTag;
    if (oiddata == NULL) {
        oiddata = SECOID_FindOID(&safebag->safeBagType);
        safebag->safeBagTypeTag = oiddata;
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
            break;
        case SEC_OID_PKCS12_KEY_BAG_ID:
            theTemplate = SEC_PointerToPKCS12KeyBagTemplate;
            break;
        case SEC_OID_PKCS12_CERT_AND_CRL_BAG_ID:
            theTemplate = SEC_PointerToPKCS12CertAndCRLBagTemplate_OLD;
            break;
        case SEC_OID_PKCS12_SECRET_BAG_ID:
            theTemplate = SEC_PointerToPKCS12SecretBagTemplate;
            break;
    }
    return theTemplate;
}

const SEC_ASN1Template *
sec_pkcs12_choose_bag_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS12SafeBag *safebag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    safebag = (SEC_PKCS12SafeBag *)src_or_dest;

    oiddata = safebag->safeBagTypeTag;
    if (oiddata == NULL) {
        oiddata = SECOID_FindOID(&safebag->safeBagType);
        safebag->safeBagTypeTag = oiddata;
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
            break;
        case SEC_OID_PKCS12_KEY_BAG_ID:
            theTemplate = SEC_PKCS12PrivateKeyBagTemplate;
            break;
        case SEC_OID_PKCS12_CERT_AND_CRL_BAG_ID:
            theTemplate = SEC_PKCS12CertAndCRLBagTemplate;
            break;
        case SEC_OID_PKCS12_SECRET_BAG_ID:
            theTemplate = SEC_PKCS12SecretBagTemplate;
            break;
    }
    return theTemplate;
}

/* returns proper cert crl template based upon type tag */
const SEC_ASN1Template *
sec_pkcs12_choose_cert_crl_type_old(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS12CertAndCRL *certbag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    certbag = (SEC_PKCS12CertAndCRL *)src_or_dest;
    oiddata = certbag->BagTypeTag;
    if (oiddata == NULL) {
        oiddata = SECOID_FindOID(&certbag->BagID);
        certbag->BagTypeTag = oiddata;
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
            break;
        case SEC_OID_PKCS12_X509_CERT_CRL_BAG:
            theTemplate = SEC_PointerToPKCS12X509CertCRLTemplate_OLD;
            break;
        case SEC_OID_PKCS12_SDSI_CERT_BAG:
            theTemplate = SEC_PointerToPKCS12SDSICertTemplate;
            break;
    }
    return theTemplate;
}

const SEC_ASN1Template *
sec_pkcs12_choose_cert_crl_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS12CertAndCRL *certbag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    certbag = (SEC_PKCS12CertAndCRL *)src_or_dest;
    oiddata = certbag->BagTypeTag;
    if (oiddata == NULL) {
        oiddata = SECOID_FindOID(&certbag->BagID);
        certbag->BagTypeTag = oiddata;
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
            break;
        case SEC_OID_PKCS12_X509_CERT_CRL_BAG:
            theTemplate = SEC_PointerToPKCS12X509CertCRLTemplate;
            break;
        case SEC_OID_PKCS12_SDSI_CERT_BAG:
            theTemplate = SEC_PointerToPKCS12SDSICertTemplate;
            break;
    }
    return theTemplate;
}

/* returns appropriate shroud template based on object type tag */
const SEC_ASN1Template *
sec_pkcs12_choose_shroud_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS12ESPVKItem *espvk;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    espvk = (SEC_PKCS12ESPVKItem *)src_or_dest;
    oiddata = espvk->espvkTag;
    if (oiddata == NULL) {
        oiddata = SECOID_FindOID(&espvk->espvkOID);
        espvk->espvkTag = oiddata;
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
            break;
        case SEC_OID_PKCS12_PKCS8_KEY_SHROUDING:
            theTemplate =
                SEC_ASN1_GET(SECKEY_PointerToEncryptedPrivateKeyInfoTemplate);
            break;
    }
    return theTemplate;
}

/* generate SALT  placing it into the character array passed in.
 * it is assumed that salt_dest is an array of appropriate size
 * XXX We might want to generate our own random context
 */
SECItem *
sec_pkcs12_generate_salt(void)
{
    SECItem *salt;

    salt = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if (salt == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }
    salt->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) *
                                              SALT_LENGTH);
    salt->len = SALT_LENGTH;
    if (salt->data == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        SECITEM_ZfreeItem(salt, PR_TRUE);
        return NULL;
    }

    PK11_GenerateRandom(salt->data, salt->len);

    return salt;
}

/* generate KEYS -- as per PKCS12 section 7.
 * only used for MAC
 */
SECItem *
sec_pkcs12_generate_key_from_password(SECOidTag algorithm,
                                      SECItem *salt,
                                      SECItem *password)
{
    unsigned char *pre_hash = NULL;
    unsigned char *hash_dest = NULL;
    SECStatus res;
    PLArenaPool *poolp;
    SECItem *key = NULL;
    int key_len = 0;

    if ((salt == NULL) || (password == NULL)) {
        return NULL;
    }

    poolp = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (poolp == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    pre_hash = (unsigned char *)PORT_ArenaZAlloc(poolp, sizeof(char) * (salt->len + password->len));
    if (pre_hash == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    hash_dest = (unsigned char *)PORT_ArenaZAlloc(poolp,
                                                  sizeof(unsigned char) * SHA1_LENGTH);
    if (hash_dest == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    PORT_Memcpy(pre_hash, salt->data, salt->len);
    /* handle password of 0 length case */
    if (password->len > 0) {
        PORT_Memcpy(&(pre_hash[salt->len]), password->data, password->len);
    }

    res = PK11_HashBuf(SEC_OID_SHA1, hash_dest, pre_hash,
                       (salt->len + password->len));
    if (res == SECFailure) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    switch (algorithm) {
        case SEC_OID_SHA1:
            if (key_len == 0)
                key_len = 16;
            key = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
            if (key == NULL) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto loser;
            }
            key->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) * key_len);
            if (key->data == NULL) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto loser;
            }
            key->len = key_len;
            PORT_Memcpy(key->data, &hash_dest[SHA1_LENGTH - key->len], key->len);
            break;
        default:
            goto loser;
            break;
    }

    PORT_FreeArena(poolp, PR_TRUE);
    return key;

loser:
    PORT_FreeArena(poolp, PR_TRUE);
    if (key != NULL) {
        SECITEM_ZfreeItem(key, PR_TRUE);
    }
    return NULL;
}

/* MAC is generated per PKCS 12 section 6.  It is expected that key, msg
 * and mac_dest are pre allocated, non-NULL arrays.  msg_len is passed in
 * because it is not known how long the message actually is.  String
 * manipulation routines will not necessarily work because msg may have
 * imbedded NULLs
 */
static SECItem *
sec_pkcs12_generate_old_mac(SECItem *key,
                            SECItem *msg)
{
    SECStatus res;
    PLArenaPool *temparena = NULL;
    unsigned char *hash_dest = NULL, *hash_src1 = NULL, *hash_src2 = NULL;
    int i;
    SECItem *mac = NULL;

    if ((key == NULL) || (msg == NULL))
        goto loser;

    /* allocate return item */
    mac = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if (mac == NULL)
        return NULL;
    mac->data = (unsigned char *)PORT_ZAlloc(sizeof(unsigned char) * SHA1_LENGTH);
    mac->len = SHA1_LENGTH;
    if (mac->data == NULL)
        goto loser;

    /* allocate temporary items */
    temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (temparena == NULL)
        goto loser;

    hash_src1 = (unsigned char *)PORT_ArenaZAlloc(temparena,
                                                  sizeof(unsigned char) * (16 + msg->len));
    if (hash_src1 == NULL)
        goto loser;

    hash_src2 = (unsigned char *)PORT_ArenaZAlloc(temparena,
                                                  sizeof(unsigned char) * (SHA1_LENGTH + 16));
    if (hash_src2 == NULL)
        goto loser;

    hash_dest = (unsigned char *)PORT_ArenaZAlloc(temparena,
                                                  sizeof(unsigned char) * SHA1_LENGTH);
    if (hash_dest == NULL)
        goto loser;

    /* perform mac'ing as per PKCS 12 */

    /* first round of hashing */
    for (i = 0; i < 16; i++)
        hash_src1[i] = key->data[i] ^ 0x36;
    PORT_Memcpy(&(hash_src1[16]), msg->data, msg->len);
    res = PK11_HashBuf(SEC_OID_SHA1, hash_dest, hash_src1, (16 + msg->len));
    if (res == SECFailure)
        goto loser;

    /* second round of hashing */
    for (i = 0; i < 16; i++)
        hash_src2[i] = key->data[i] ^ 0x5c;
    PORT_Memcpy(&(hash_src2[16]), hash_dest, SHA1_LENGTH);
    res = PK11_HashBuf(SEC_OID_SHA1, mac->data, hash_src2, SHA1_LENGTH + 16);
    if (res == SECFailure)
        goto loser;

    PORT_FreeArena(temparena, PR_TRUE);
    return mac;

loser:
    if (temparena != NULL)
        PORT_FreeArena(temparena, PR_TRUE);
    if (mac != NULL)
        SECITEM_ZfreeItem(mac, PR_TRUE);
    return NULL;
}

/* MAC is generated per PKCS 12 section 6.  It is expected that key, msg
 * and mac_dest are pre allocated, non-NULL arrays.  msg_len is passed in
 * because it is not known how long the message actually is.  String
 * manipulation routines will not necessarily work because msg may have
 * imbedded NULLs
 */
SECItem *
sec_pkcs12_generate_mac(SECItem *key,
                        SECItem *msg,
                        PRBool old_method)
{
    SECStatus res = SECFailure;
    SECItem *mac = NULL;
    PK11Context *pk11cx = NULL;
    SECItem ignore = { 0 };

    if ((key == NULL) || (msg == NULL)) {
        return NULL;
    }

    if (old_method == PR_TRUE) {
        return sec_pkcs12_generate_old_mac(key, msg);
    }

    /* allocate return item */
    mac = SECITEM_AllocItem(NULL, NULL, SHA1_LENGTH);
    if (mac == NULL) {
        return NULL;
    }

    pk11cx = PK11_CreateContextByRawKey(NULL, CKM_SHA_1_HMAC, PK11_OriginDerive,
                                        CKA_SIGN, key, &ignore, NULL);
    if (pk11cx == NULL) {
        goto loser;
    }

    res = PK11_DigestBegin(pk11cx);
    if (res == SECFailure) {
        goto loser;
    }

    res = PK11_DigestOp(pk11cx, msg->data, msg->len);
    if (res == SECFailure) {
        goto loser;
    }

    res = PK11_DigestFinal(pk11cx, mac->data, &mac->len, SHA1_LENGTH);
    if (res == SECFailure) {
        goto loser;
    }

    PK11_DestroyContext(pk11cx, PR_TRUE);
    pk11cx = NULL;

loser:

    if (res != SECSuccess) {
        SECITEM_ZfreeItem(mac, PR_TRUE);
        mac = NULL;
        if (pk11cx) {
            PK11_DestroyContext(pk11cx, PR_TRUE);
        }
    }

    return mac;
}

/* compute the thumbprint of the DER cert and create a digest info
 * to store it in and return the digest info.
 * a return of NULL indicates an error.
 */
SGNDigestInfo *
sec_pkcs12_compute_thumbprint(SECItem *der_cert)
{
    SGNDigestInfo *thumb = NULL;
    SECItem digest;
    PLArenaPool *temparena = NULL;
    SECStatus rv = SECFailure;

    if (der_cert == NULL)
        return NULL;

    temparena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (temparena == NULL) {
        return NULL;
    }

    digest.data = (unsigned char *)PORT_ArenaZAlloc(temparena,
                                                    sizeof(unsigned char) *
                                                        SHA1_LENGTH);
    /* digest data and create digest info */
    if (digest.data != NULL) {
        digest.len = SHA1_LENGTH;
        rv = PK11_HashBuf(SEC_OID_SHA1, digest.data, der_cert->data,
                          der_cert->len);
        if (rv == SECSuccess) {
            thumb = SGN_CreateDigestInfo(SEC_OID_SHA1,
                                         digest.data,
                                         digest.len);
        } else {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
        }
    } else {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    PORT_FreeArena(temparena, PR_TRUE);

    return thumb;
}

/* create a virtual password per PKCS 12, the password is converted
 * to unicode, the salt is prepended to it, and then the whole thing
 * is returned */
SECItem *
sec_pkcs12_create_virtual_password(SECItem *password, SECItem *salt,
                                   PRBool swap)
{
    SECItem uniPwd = { siBuffer, NULL, 0 }, *retPwd = NULL;

    if ((password == NULL) || (salt == NULL)) {
        return NULL;
    }

    if (password->len == 0) {
        uniPwd.data = (unsigned char *)PORT_ZAlloc(2);
        uniPwd.len = 2;
        if (!uniPwd.data) {
            return NULL;
        }
    } else {
        uniPwd.data = (unsigned char *)PORT_ZAlloc(password->len * 3);
        uniPwd.len = password->len * 3;
        if (!PORT_UCS2_ASCIIConversion(PR_TRUE, password->data, password->len,
                                       uniPwd.data, uniPwd.len, &uniPwd.len, swap)) {
            SECITEM_ZfreeItem(&uniPwd, PR_FALSE);
            return NULL;
        }
    }

    retPwd = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if (retPwd == NULL) {
        goto loser;
    }

    /* allocate space and copy proper data */
    retPwd->len = uniPwd.len + salt->len;
    retPwd->data = (unsigned char *)PORT_Alloc(retPwd->len);
    if (retPwd->data == NULL) {
        PORT_Free(retPwd);
        goto loser;
    }

    PORT_Memcpy(retPwd->data, salt->data, salt->len);
    PORT_Memcpy((retPwd->data + salt->len), uniPwd.data, uniPwd.len);

    SECITEM_ZfreeItem(&uniPwd, PR_FALSE);

    return retPwd;

loser:
    PORT_SetError(SEC_ERROR_NO_MEMORY);
    SECITEM_ZfreeItem(&uniPwd, PR_FALSE);
    return NULL;
}

/* appends a shrouded key to a key bag.  this is used for exporting
 * to store externally wrapped keys.  it is used when importing to convert
 * old items to new
 */
SECStatus
sec_pkcs12_append_shrouded_key(SEC_PKCS12BaggageItem *bag,
                               SEC_PKCS12ESPVKItem *espvk)
{
    int size;
    void *mark = NULL, *dummy = NULL;

    if ((bag == NULL) || (espvk == NULL))
        return SECFailure;

    mark = PORT_ArenaMark(bag->poolp);

    /* grow the list */
    size = (bag->nEspvks + 1) * sizeof(SEC_PKCS12ESPVKItem *);
    dummy = (SEC_PKCS12ESPVKItem **)PORT_ArenaGrow(bag->poolp,
                                                   bag->espvks, size,
                                                   size + sizeof(SEC_PKCS12ESPVKItem *));
    bag->espvks = (SEC_PKCS12ESPVKItem **)dummy;
    if (dummy == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    bag->espvks[bag->nEspvks] = espvk;
    bag->nEspvks++;
    bag->espvks[bag->nEspvks] = NULL;

    PORT_ArenaUnmark(bag->poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(bag->poolp, mark);
    return SECFailure;
}

/* search a certificate list for a nickname, a thumbprint, or both
 * within a certificate bag.  if the certificate could not be
 * found or an error occurs, NULL is returned;
 */
static SEC_PKCS12CertAndCRL *
sec_pkcs12_find_cert_in_certbag(SEC_PKCS12CertAndCRLBag *certbag,
                                SECItem *nickname, SGNDigestInfo *thumbprint)
{
    PRBool search_both = PR_FALSE, search_nickname = PR_FALSE;
    int i, j;

    if ((certbag == NULL) || ((nickname == NULL) && (thumbprint == NULL))) {
        return NULL;
    }

    if (thumbprint && nickname) {
        search_both = PR_TRUE;
    }

    if (nickname) {
        search_nickname = PR_TRUE;
    }

search_again:
    i = 0;
    while (certbag->certAndCRLs[i] != NULL) {
        SEC_PKCS12CertAndCRL *cert = certbag->certAndCRLs[i];

        if (SECOID_FindOIDTag(&cert->BagID) == SEC_OID_PKCS12_X509_CERT_CRL_BAG) {

            /* check nicknames */
            if (search_nickname) {
                if (SECITEM_CompareItem(nickname, &cert->nickname) == SECEqual) {
                    return cert;
                }
            } else {
                /* check thumbprints */
                SECItem **derCertList;

                /* get pointer to certificate list, does not need to
                 * be freed since it is within the arena which will
                 * be freed later.
                 */
                derCertList = SEC_PKCS7GetCertificateList(&cert->value.x509->certOrCRL);
                j = 0;
                if (derCertList != NULL) {
                    while (derCertList[j] != NULL) {
                        SECComparison eq;
                        SGNDigestInfo *di;
                        di = sec_pkcs12_compute_thumbprint(derCertList[j]);
                        if (di) {
                            eq = SGN_CompareDigestInfo(thumbprint, di);
                            SGN_DestroyDigestInfo(di);
                            if (eq == SECEqual) {
                                /* copy the derCert for later reference */
                                cert->value.x509->derLeafCert = derCertList[j];
                                return cert;
                            }
                        } else {
                            /* an error occurred */
                            return NULL;
                        }
                        j++;
                    }
                }
            }
        }

        i++;
    }

    if (search_both) {
        search_both = PR_FALSE;
        search_nickname = PR_FALSE;
        goto search_again;
    }

    return NULL;
}

/* search a key list for a nickname, a thumbprint, or both
 * within a key bag.  if the key could not be
 * found or an error occurs, NULL is returned;
 */
static SEC_PKCS12PrivateKey *
sec_pkcs12_find_key_in_keybag(SEC_PKCS12PrivateKeyBag *keybag,
                              SECItem *nickname, SGNDigestInfo *thumbprint)
{
    PRBool search_both = PR_FALSE, search_nickname = PR_FALSE;
    int i, j;

    if ((keybag == NULL) || ((nickname == NULL) && (thumbprint == NULL))) {
        return NULL;
    }

    if (keybag->privateKeys == NULL) {
        return NULL;
    }

    if (thumbprint && nickname) {
        search_both = PR_TRUE;
    }

    if (nickname) {
        search_nickname = PR_TRUE;
    }

search_again:
    i = 0;
    while (keybag->privateKeys[i] != NULL) {
        SEC_PKCS12PrivateKey *key = keybag->privateKeys[i];

        /* check nicknames */
        if (search_nickname) {
            if (SECITEM_CompareItem(nickname, &key->pvkData.nickname) == SECEqual) {
                return key;
            }
        } else {
            /* check digests */
            SGNDigestInfo **assocCerts = key->pvkData.assocCerts;
            if ((assocCerts == NULL) || (assocCerts[0] == NULL)) {
                return NULL;
            }

            j = 0;
            while (assocCerts[j] != NULL) {
                SECComparison eq;
                eq = SGN_CompareDigestInfo(thumbprint, assocCerts[j]);
                if (eq == SECEqual) {
                    return key;
                }
                j++;
            }
        }
        i++;
    }

    if (search_both) {
        search_both = PR_FALSE;
        search_nickname = PR_FALSE;
        goto search_again;
    }

    return NULL;
}

/* seach the safe first then try the baggage bag
 *  safe and bag contain certs and keys to search
 *  objType is the object type to look for
 *  bagType is the type of bag that was found by sec_pkcs12_find_object
 *  index is the entity in safe->safeContents or bag->unencSecrets which
 *    is being searched
 *  nickname and thumbprint are the search criteria
 *
 * a return of null indicates no match
 */
static void *
sec_pkcs12_try_find(SEC_PKCS12SafeContents *safe,
                    SEC_PKCS12BaggageItem *bag,
                    SECOidTag objType, SECOidTag bagType, int index,
                    SECItem *nickname, SGNDigestInfo *thumbprint)
{
    PRBool searchSafe;
    int i = index;

    if ((safe == NULL) && (bag == NULL)) {
        return NULL;
    }

    searchSafe = (safe == NULL ? PR_FALSE : PR_TRUE);
    switch (objType) {
        case SEC_OID_PKCS12_CERT_AND_CRL_BAG_ID:
            if (objType == bagType) {
                SEC_PKCS12CertAndCRLBag *certBag;

                if (searchSafe) {
                    certBag = safe->contents[i]->safeContent.certAndCRLBag;
                } else {
                    certBag = bag->unencSecrets[i]->safeContent.certAndCRLBag;
                }
                return sec_pkcs12_find_cert_in_certbag(certBag, nickname,
                                                       thumbprint);
            }
            break;
        case SEC_OID_PKCS12_KEY_BAG_ID:
            if (objType == bagType) {
                SEC_PKCS12PrivateKeyBag *keyBag;

                if (searchSafe) {
                    keyBag = safe->contents[i]->safeContent.keyBag;
                } else {
                    keyBag = bag->unencSecrets[i]->safeContent.keyBag;
                }
                return sec_pkcs12_find_key_in_keybag(keyBag, nickname,
                                                     thumbprint);
            }
            break;
        default:
            break;
    }

    return NULL;
}

/* searches both the baggage and the safe areas looking for
 * object of specified type matching either the nickname or the
 * thumbprint specified.
 *
 * safe and baggage store certs and keys
 * objType is the OID for the bag type to be searched:
 *   SEC_OID_PKCS12_KEY_BAG_ID, or
 *   SEC_OID_PKCS12_CERT_AND_CRL_BAG_ID
 * nickname and thumbprint are the search criteria
 *
 * if no match found, NULL returned and error set
 */
void *
sec_pkcs12_find_object(SEC_PKCS12SafeContents *safe,
                       SEC_PKCS12Baggage *baggage,
                       SECOidTag objType,
                       SECItem *nickname,
                       SGNDigestInfo *thumbprint)
{
    int i, j;
    void *retItem;

    if (((safe == NULL) && (thumbprint == NULL)) ||
        ((nickname == NULL) && (thumbprint == NULL))) {
        return NULL;
    }

    i = 0;
    if ((safe != NULL) && (safe->contents != NULL)) {
        while (safe->contents[i] != NULL) {
            SECOidTag bagType = SECOID_FindOIDTag(&safe->contents[i]->safeBagType);
            retItem = sec_pkcs12_try_find(safe, NULL, objType, bagType, i,
                                          nickname, thumbprint);
            if (retItem != NULL) {
                return retItem;
            }
            i++;
        }
    }

    if ((baggage != NULL) && (baggage->bags != NULL)) {
        i = 0;
        while (baggage->bags[i] != NULL) {
            SEC_PKCS12BaggageItem *xbag = baggage->bags[i];
            j = 0;
            if (xbag->unencSecrets != NULL) {
                while (xbag->unencSecrets[j] != NULL) {
                    SECOidTag bagType;
                    bagType = SECOID_FindOIDTag(&xbag->unencSecrets[j]->safeBagType);
                    retItem = sec_pkcs12_try_find(NULL, xbag, objType, bagType,
                                                  j, nickname, thumbprint);
                    if (retItem != NULL) {
                        return retItem;
                    }
                    j++;
                }
            }
            i++;
        }
    }

    PORT_SetError(SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME);
    return NULL;
}

/* this function converts a password to UCS2 and ensures that the
 * required double 0 byte be placed at the end of the string (if zeroTerm
 * is set), or the 0 bytes at the end are dropped (if zeroTerm is not set).
 * If toUnicode is false, we convert from UCS2 to UTF8/ASCII (latter is a
 * proper subset of the former) depending on the state of the asciiCovert
 * flag)
 */
PRBool
sec_pkcs12_convert_item_to_unicode(PLArenaPool *arena, SECItem *dest,
                                   SECItem *src, PRBool zeroTerm,
                                   PRBool asciiConvert, PRBool toUnicode)
{
    PRBool success = PR_FALSE;
    int bufferSize;

    if (!src || !dest) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return PR_FALSE;
    }

    bufferSize = src->len * 3 + 2;
    dest->len = bufferSize;
    if (arena) {
        dest->data = (unsigned char *)PORT_ArenaZAlloc(arena, dest->len);
    } else {
        dest->data = (unsigned char *)PORT_ZAlloc(dest->len);
    }

    if (!dest->data) {
        dest->len = 0;
        return PR_FALSE;
    }

    if (!asciiConvert) {
        success = PORT_UCS2_UTF8Conversion(toUnicode, src->data, src->len, dest->data,
                                           dest->len, &dest->len);
    } else {
#ifndef IS_LITTLE_ENDIAN
        PRBool swapUnicode = PR_FALSE;
#else
        PRBool swapUnicode = PR_TRUE;
#endif
        success = PORT_UCS2_ASCIIConversion(toUnicode, src->data, src->len, dest->data,
                                            dest->len, &dest->len, swapUnicode);
    }

    if (!success) {
        if (!arena) {
            PORT_Free(dest->data);
            dest->data = NULL;
            dest->len = 0;
        }
        return PR_FALSE;
    }

    /* in some cases we need to add NULL terminations and in others
     * we need to drop null terminations */
    if (zeroTerm) {
        /* unicode adds two nulls at the end */
        if (toUnicode) {
            if ((dest->len < 2) || dest->data[dest->len - 1] || dest->data[dest->len - 2]) {
                /* we've already allocated space for these new NULLs */
                PORT_Assert(dest->len + 2 <= bufferSize);
                dest->len += 2;
                dest->data[dest->len - 1] = dest->data[dest->len - 2] = 0;
            }
            /* ascii/utf-8 adds just 1 */
        } else if (!dest->len || dest->data[dest->len - 1]) {
            PORT_Assert(dest->len + 1 <= bufferSize);
            dest->len++;
            dest->data[dest->len - 1] = 0;
        }
    } else {
        /* handle the drop case, no need to do any allocations here. */
        if (toUnicode) {
            while ((dest->len >= 2) && !dest->data[dest->len - 1] &&
                   !dest->data[dest->len - 2]) {
                dest->len -= 2;
            }
        } else {
            while (dest->len && !dest->data[dest->len - 1]) {
                dest->len--;
            }
        }
    }

    return PR_TRUE;
}

PRBool
sec_pkcs12_is_pkcs12_pbe_algorithm(SECOidTag algorithm)
{
    switch (algorithm) {
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
        /* those are actually PKCS #5 v1.5 PBEs, but we
         * historically treat them in the same way as PKCS #12
         * PBEs */
        case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
        case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
        case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
            return PR_TRUE;
        default:
            return PR_FALSE;
    }
}

/* this function decodes a password from Unicode if necessary,
 * according to the PBE algorithm.
 *
 * we assume that the pwitem is already encoded in Unicode by the
 * caller.  if the encryption scheme is not the one defined in PKCS
 * #12, decode the pwitem back into UTF-8. NOTE: UTF-8 strings are
 * used in the PRF without the trailing NULL */
PRBool
sec_pkcs12_decode_password(PLArenaPool *arena,
                           SECItem *result,
                           SECOidTag algorithm,
                           const SECItem *pwitem)
{
    if (!sec_pkcs12_is_pkcs12_pbe_algorithm(algorithm))
        return sec_pkcs12_convert_item_to_unicode(arena, result,
                                                  (SECItem *)pwitem,
                                                  PR_FALSE, PR_FALSE, PR_FALSE);

    return SECITEM_CopyItem(arena, result, pwitem) == SECSuccess;
}

/* this function encodes a password into Unicode if necessary,
 * according to the PBE algorithm.
 *
 * we assume that the pwitem holds a raw password.  if the encryption
 * scheme is the one defined in PKCS #12, encode the password into
 * BMPString. */
PRBool
sec_pkcs12_encode_password(PLArenaPool *arena,
                           SECItem *result,
                           SECOidTag algorithm,
                           const SECItem *pwitem)
{
    if (sec_pkcs12_is_pkcs12_pbe_algorithm(algorithm))
        return sec_pkcs12_convert_item_to_unicode(arena, result,
                                                  (SECItem *)pwitem,
                                                  PR_TRUE, PR_TRUE, PR_TRUE);

    return SECITEM_CopyItem(arena, result, pwitem) == SECSuccess;
}

/* pkcs 12 templates */
static const SEC_ASN1TemplateChooserPtr sec_pkcs12_shroud_chooser =
    sec_pkcs12_choose_shroud_type;

const SEC_ASN1Template SEC_PKCS12CodedSafeBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12SafeBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12SafeBag, safeBagType) },
    { SEC_ASN1_ANY, offsetof(SEC_PKCS12SafeBag, derSafeContent) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12CodedCertBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12CertAndCRL) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12CertAndCRL, BagID) },
    { SEC_ASN1_ANY, offsetof(SEC_PKCS12CertAndCRL, derValue) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12CodedCertAndCRLBagTemplate[] = {
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12CertAndCRLBag, certAndCRLs),
      SEC_PKCS12CodedCertBagTemplate },
};

const SEC_ASN1Template SEC_PKCS12ESPVKItemTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12ESPVKItem) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12ESPVKItem, espvkOID) },
    { SEC_ASN1_INLINE, offsetof(SEC_PKCS12ESPVKItem, espvkData),
      SEC_PKCS12PVKSupportingDataTemplate_OLD },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
          SEC_ASN1_DYNAMIC | 0,
      offsetof(SEC_PKCS12ESPVKItem, espvkCipherText),
      &sec_pkcs12_shroud_chooser },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12ESPVKItemTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12ESPVKItem) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12ESPVKItem, espvkOID) },
    { SEC_ASN1_INLINE, offsetof(SEC_PKCS12ESPVKItem, espvkData),
      SEC_PKCS12PVKSupportingDataTemplate },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
          SEC_ASN1_DYNAMIC | 0,
      offsetof(SEC_PKCS12ESPVKItem, espvkCipherText),
      &sec_pkcs12_shroud_chooser },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12PVKAdditionalDataTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PVKAdditionalData) },
    { SEC_ASN1_OBJECT_ID,
      offsetof(SEC_PKCS12PVKAdditionalData, pvkAdditionalType) },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(SEC_PKCS12PVKAdditionalData, pvkAdditionalContent) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12PVKSupportingDataTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PVKSupportingData) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
      offsetof(SEC_PKCS12PVKSupportingData, assocCerts),
      SEC_ASN1_SUB(sgn_DigestInfoTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,
      offsetof(SEC_PKCS12PVKSupportingData, regenerable) },
    { SEC_ASN1_PRINTABLE_STRING,
      offsetof(SEC_PKCS12PVKSupportingData, nickname) },
    { SEC_ASN1_ANY | SEC_ASN1_OPTIONAL,
      offsetof(SEC_PKCS12PVKSupportingData, pvkAdditionalDER) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12PVKSupportingDataTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PVKSupportingData) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
      offsetof(SEC_PKCS12PVKSupportingData, assocCerts),
      SEC_ASN1_SUB(sgn_DigestInfoTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,
      offsetof(SEC_PKCS12PVKSupportingData, regenerable) },
    { SEC_ASN1_BMP_STRING,
      offsetof(SEC_PKCS12PVKSupportingData, uniNickName) },
    { SEC_ASN1_ANY | SEC_ASN1_OPTIONAL,
      offsetof(SEC_PKCS12PVKSupportingData, pvkAdditionalDER) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12BaggageItemTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12BaggageItem) },
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12BaggageItem, espvks),
      SEC_PKCS12ESPVKItemTemplate },
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12BaggageItem, unencSecrets),
      SEC_PKCS12SafeBagTemplate },
    /*{ SEC_ASN1_SET_OF, offsetof(SEC_PKCS12BaggageItem, unencSecrets),
        SEC_PKCS12CodedSafeBagTemplate }, */
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12BaggageTemplate[] = {
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12Baggage, bags),
      SEC_PKCS12BaggageItemTemplate },
};

const SEC_ASN1Template SEC_PKCS12BaggageTemplate_OLD[] = {
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12Baggage_OLD, espvks),
      SEC_PKCS12ESPVKItemTemplate_OLD },
};

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_bag_chooser =
    sec_pkcs12_choose_bag_type;

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_bag_chooser_old =
    sec_pkcs12_choose_bag_type_old;

const SEC_ASN1Template SEC_PKCS12SafeBagTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12SafeBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12SafeBag, safeBagType) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT |
          SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(SEC_PKCS12SafeBag, safeContent),
      &sec_pkcs12_bag_chooser_old },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SafeBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12SafeBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12SafeBag, safeBagType) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_POINTER,
      offsetof(SEC_PKCS12SafeBag, safeContent),
      &sec_pkcs12_bag_chooser },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BMP_STRING,
      offsetof(SEC_PKCS12SafeBag, uniSafeBagName) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SafeContentsTemplate_OLD[] = {
    { SEC_ASN1_SET_OF,
      offsetof(SEC_PKCS12SafeContents, contents),
      SEC_PKCS12SafeBagTemplate_OLD }
};

const SEC_ASN1Template SEC_PKCS12SafeContentsTemplate[] = {
    { SEC_ASN1_SET_OF,
      offsetof(SEC_PKCS12SafeContents, contents),
      SEC_PKCS12SafeBagTemplate } /* here */
};

const SEC_ASN1Template SEC_PKCS12PrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PrivateKey) },
    { SEC_ASN1_INLINE, offsetof(SEC_PKCS12PrivateKey, pvkData),
      SEC_PKCS12PVKSupportingDataTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
      offsetof(SEC_PKCS12PrivateKey, pkcs8data),
      SEC_ASN1_SUB(SECKEY_PrivateKeyInfoTemplate) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12PrivateKeyBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PrivateKeyBag) },
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12PrivateKeyBag, privateKeys),
      SEC_PKCS12PrivateKeyTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12X509CertCRLTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12X509CertCRL) },
    { SEC_ASN1_INLINE, offsetof(SEC_PKCS12X509CertCRL, certOrCRL),
      sec_PKCS7ContentInfoTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
      offsetof(SEC_PKCS12X509CertCRL, thumbprint),
      SEC_ASN1_SUB(sgn_DigestInfoTemplate) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12X509CertCRLTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12X509CertCRL) },
    { SEC_ASN1_INLINE, offsetof(SEC_PKCS12X509CertCRL, certOrCRL),
      sec_PKCS7ContentInfoTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SDSICertTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12X509CertCRL) },
    { SEC_ASN1_IA5_STRING, offsetof(SEC_PKCS12SDSICert, value) },
    { 0 }
};

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_cert_crl_chooser_old =
    sec_pkcs12_choose_cert_crl_type_old;

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_cert_crl_chooser =
    sec_pkcs12_choose_cert_crl_type;

const SEC_ASN1Template SEC_PKCS12CertAndCRLTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12CertAndCRL) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12CertAndCRL, BagID) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_EXPLICIT |
          SEC_ASN1_DYNAMIC | SEC_ASN1_CONSTRUCTED | 0,
      offsetof(SEC_PKCS12CertAndCRL, value),
      &sec_pkcs12_cert_crl_chooser_old },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12CertAndCRLTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12CertAndCRL) },
    { SEC_ASN1_OBJECT_ID, offsetof(SEC_PKCS12CertAndCRL, BagID) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT |
          SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(SEC_PKCS12CertAndCRL, value),
      &sec_pkcs12_cert_crl_chooser },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12CertAndCRLBagTemplate[] = {
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12CertAndCRLBag, certAndCRLs),
      SEC_PKCS12CertAndCRLTemplate },
};

const SEC_ASN1Template SEC_PKCS12CertAndCRLBagTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12CertAndCRLBag) },
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12CertAndCRLBag, certAndCRLs),
      SEC_PKCS12CertAndCRLTemplate_OLD },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SecretAdditionalTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12SecretAdditional) },
    { SEC_ASN1_OBJECT_ID,
      offsetof(SEC_PKCS12SecretAdditional, secretAdditionalType) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_EXPLICIT,
      offsetof(SEC_PKCS12SecretAdditional, secretAdditionalContent) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SecretTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12Secret) },
    { SEC_ASN1_BMP_STRING, offsetof(SEC_PKCS12Secret, uniSecretName) },
    { SEC_ASN1_ANY, offsetof(SEC_PKCS12Secret, value) },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL,
      offsetof(SEC_PKCS12Secret, secretAdditional),
      SEC_PKCS12SecretAdditionalTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SecretItemTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12Secret) },
    { SEC_ASN1_INLINE | SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(SEC_PKCS12SecretItem, secret), SEC_PKCS12SecretTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(SEC_PKCS12SecretItem, subFolder), SEC_PKCS12SafeBagTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12SecretBagTemplate[] = {
    { SEC_ASN1_SET_OF, offsetof(SEC_PKCS12SecretBag, secrets),
      SEC_PKCS12SecretItemTemplate },
};

const SEC_ASN1Template SEC_PKCS12MacDataTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PFXItem) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(SEC_PKCS12MacData, safeMac),
      SEC_ASN1_SUB(sgn_DigestInfoTemplate) },
    { SEC_ASN1_BIT_STRING, offsetof(SEC_PKCS12MacData, macSalt) },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12PFXItemTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PFXItem) },
    { SEC_ASN1_OPTIONAL |
          SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(SEC_PKCS12PFXItem, macData), SEC_PKCS12MacDataTemplate },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(SEC_PKCS12PFXItem, authSafe),
      sec_PKCS7ContentInfoTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12PFXItemTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12PFXItem) },
    { SEC_ASN1_OPTIONAL |
          SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
      offsetof(SEC_PKCS12PFXItem, old_safeMac),
      SEC_ASN1_SUB(sgn_DigestInfoTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BIT_STRING,
      offsetof(SEC_PKCS12PFXItem, old_macSalt) },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(SEC_PKCS12PFXItem, authSafe),
      sec_PKCS7ContentInfoTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12AuthenticatedSafeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12AuthenticatedSafe) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(SEC_PKCS12AuthenticatedSafe, version) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OBJECT_ID,
      offsetof(SEC_PKCS12AuthenticatedSafe, transportMode) },
    { SEC_ASN1_BIT_STRING | SEC_ASN1_OPTIONAL,
      offsetof(SEC_PKCS12AuthenticatedSafe, privacySalt) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SET_OF,
      offsetof(SEC_PKCS12AuthenticatedSafe, baggage.bags),
      SEC_PKCS12BaggageItemTemplate },
    { SEC_ASN1_POINTER,
      offsetof(SEC_PKCS12AuthenticatedSafe, safe),
      sec_PKCS7ContentInfoTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PKCS12AuthenticatedSafeTemplate_OLD[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS12AuthenticatedSafe) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(SEC_PKCS12AuthenticatedSafe, version) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(SEC_PKCS12AuthenticatedSafe, transportMode) },
    { SEC_ASN1_BIT_STRING,
      offsetof(SEC_PKCS12AuthenticatedSafe, privacySalt) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(SEC_PKCS12AuthenticatedSafe, old_baggage),
      SEC_PKCS12BaggageTemplate_OLD },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(SEC_PKCS12AuthenticatedSafe, old_safe),
      sec_PKCS7ContentInfoTemplate },
    { 0 }
};

const SEC_ASN1Template SEC_PointerToPKCS12KeyBagTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12PrivateKeyBagTemplate }
};

const SEC_ASN1Template SEC_PointerToPKCS12CertAndCRLBagTemplate_OLD[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12CertAndCRLBagTemplate_OLD }
};

const SEC_ASN1Template SEC_PointerToPKCS12CertAndCRLBagTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12CertAndCRLBagTemplate }
};

const SEC_ASN1Template SEC_PointerToPKCS12SecretBagTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12SecretBagTemplate }
};

const SEC_ASN1Template SEC_PointerToPKCS12X509CertCRLTemplate_OLD[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12X509CertCRLTemplate_OLD }
};

const SEC_ASN1Template SEC_PointerToPKCS12X509CertCRLTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12X509CertCRLTemplate }
};

const SEC_ASN1Template SEC_PointerToPKCS12SDSICertTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS12SDSICertTemplate }
};
