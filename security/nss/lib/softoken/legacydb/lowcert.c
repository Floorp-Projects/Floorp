/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Certificate handling code
 */

#include "seccomon.h"
#include "secder.h"
#include "nssilock.h"
#include "lowkeyi.h"
#include "secasn1.h"
#include "secoid.h"
#include "secerr.h"
#include "pcert.h"

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

static const SEC_ASN1Template nsslowcert_SubjectPublicKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWCERTSubjectPublicKeyInfo) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
      offsetof(NSSLOWCERTSubjectPublicKeyInfo, algorithm),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_BIT_STRING,
      offsetof(NSSLOWCERTSubjectPublicKeyInfo, subjectPublicKey) },
    { 0 }
};

static const SEC_ASN1Template nsslowcert_RSAPublicKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPublicKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey, u.rsa.modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey, u.rsa.publicExponent) },
    { 0 }
};
static const SEC_ASN1Template nsslowcert_DSAPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey, u.dsa.publicValue) },
    { 0 }
};
static const SEC_ASN1Template nsslowcert_DHPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPublicKey, u.dh.publicValue) },
    { 0 }
};

/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */

static void
prepare_low_rsa_pub_key_for_asn1(NSSLOWKEYPublicKey *pubk)
{
    pubk->u.rsa.modulus.type = siUnsignedInteger;
    pubk->u.rsa.publicExponent.type = siUnsignedInteger;
}

static void
prepare_low_dsa_pub_key_for_asn1(NSSLOWKEYPublicKey *pubk)
{
    pubk->u.dsa.publicValue.type = siUnsignedInteger;
    pubk->u.dsa.params.prime.type = siUnsignedInteger;
    pubk->u.dsa.params.subPrime.type = siUnsignedInteger;
    pubk->u.dsa.params.base.type = siUnsignedInteger;
}

static void
prepare_low_dh_pub_key_for_asn1(NSSLOWKEYPublicKey *pubk)
{
    pubk->u.dh.prime.type = siUnsignedInteger;
    pubk->u.dh.base.type = siUnsignedInteger;
    pubk->u.dh.publicValue.type = siUnsignedInteger;
}

/*
 * simple cert decoder to avoid the cost of asn1 engine
 */
static unsigned char *
nsslowcert_dataStart(unsigned char *buf, unsigned int length,
                     unsigned int *data_length, PRBool includeTag,
                     unsigned char *rettag)
{
    unsigned char tag;
    unsigned int used_length = 0;

    /* need at least a tag and a 1 byte length */
    if (length < 2) {
        return NULL;
    }

    tag = buf[used_length++];

    if (rettag) {
        *rettag = tag;
    }

    /* blow out when we come to the end */
    if (tag == 0) {
        return NULL;
    }

    *data_length = buf[used_length++];

    if (*data_length & 0x80) {
        int len_count = *data_length & 0x7f;

        if (len_count + used_length > length) {
            return NULL;
        }

        *data_length = 0;

        while (len_count-- > 0) {
            *data_length = (*data_length << 8) | buf[used_length++];
        }
    }

    if (*data_length > (length - used_length)) {
        *data_length = length - used_length;
        return NULL;
    }
    if (includeTag)
        *data_length += used_length;

    return (buf + (includeTag ? 0 : used_length));
}

static void
SetTimeType(SECItem *item, unsigned char tagtype)
{
    switch (tagtype) {
        case SEC_ASN1_UTC_TIME:
            item->type = siUTCTime;
            break;

        case SEC_ASN1_GENERALIZED_TIME:
            item->type = siGeneralizedTime;
            break;

        default:
            PORT_Assert(0);
            break;
    }
}

static int
nsslowcert_GetValidityFields(unsigned char *buf, int buf_length,
                             SECItem *notBefore, SECItem *notAfter)
{
    unsigned char tagtype;
    notBefore->data = nsslowcert_dataStart(buf, buf_length,
                                           &notBefore->len, PR_FALSE, &tagtype);
    if (notBefore->data == NULL)
        return SECFailure;
    SetTimeType(notBefore, tagtype);
    buf_length -= (notBefore->data - buf) + notBefore->len;
    buf = notBefore->data + notBefore->len;
    notAfter->data = nsslowcert_dataStart(buf, buf_length,
                                          &notAfter->len, PR_FALSE, &tagtype);
    if (notAfter->data == NULL)
        return SECFailure;
    SetTimeType(notAfter, tagtype);
    return SECSuccess;
}

static int
nsslowcert_GetCertFields(unsigned char *cert, int cert_length,
                         SECItem *issuer, SECItem *serial, SECItem *derSN, SECItem *subject,
                         SECItem *valid, SECItem *subjkey, SECItem *extensions)
{
    unsigned char *buf;
    unsigned int buf_length;
    unsigned char *dummy;
    unsigned int dummylen;

    /* get past the signature wrap */
    buf = nsslowcert_dataStart(cert, cert_length, &buf_length, PR_FALSE, NULL);
    if (buf == NULL)
        return SECFailure;
    /* get into the raw cert data */
    buf = nsslowcert_dataStart(buf, buf_length, &buf_length, PR_FALSE, NULL);
    if (buf == NULL)
        return SECFailure;
    /* skip past any optional version number */
    if ((buf[0] & 0xa0) == 0xa0) {
        dummy = nsslowcert_dataStart(buf, buf_length, &dummylen, PR_FALSE, NULL);
        if (dummy == NULL)
            return SECFailure;
        buf_length -= (dummy - buf) + dummylen;
        buf = dummy + dummylen;
    }
    /* serial number */
    if (derSN) {
        derSN->data = nsslowcert_dataStart(buf, buf_length, &derSN->len, PR_TRUE, NULL);
        /* derSN->data  doesn't need to be checked because if it fails so will
         * serial->data below. The only difference between the two calls is
         * whether or not the tags are included in the returned buffer */
    }
    serial->data = nsslowcert_dataStart(buf, buf_length, &serial->len, PR_FALSE, NULL);
    if (serial->data == NULL)
        return SECFailure;
    buf_length -= (serial->data - buf) + serial->len;
    buf = serial->data + serial->len;
    /* skip the OID */
    dummy = nsslowcert_dataStart(buf, buf_length, &dummylen, PR_FALSE, NULL);
    if (dummy == NULL)
        return SECFailure;
    buf_length -= (dummy - buf) + dummylen;
    buf = dummy + dummylen;
    /* issuer */
    issuer->data = nsslowcert_dataStart(buf, buf_length, &issuer->len, PR_TRUE, NULL);
    if (issuer->data == NULL)
        return SECFailure;
    buf_length -= (issuer->data - buf) + issuer->len;
    buf = issuer->data + issuer->len;

    /* only wanted issuer/SN */
    if (valid == NULL) {
        return SECSuccess;
    }
    /* validity */
    valid->data = nsslowcert_dataStart(buf, buf_length, &valid->len, PR_FALSE, NULL);
    if (valid->data == NULL)
        return SECFailure;
    buf_length -= (valid->data - buf) + valid->len;
    buf = valid->data + valid->len;
    /*subject */
    subject->data = nsslowcert_dataStart(buf, buf_length, &subject->len, PR_TRUE, NULL);
    if (subject->data == NULL)
        return SECFailure;
    buf_length -= (subject->data - buf) + subject->len;
    buf = subject->data + subject->len;
    /* subject  key info */
    subjkey->data = nsslowcert_dataStart(buf, buf_length, &subjkey->len, PR_TRUE, NULL);
    if (subjkey->data == NULL)
        return SECFailure;
    buf_length -= (subjkey->data - buf) + subjkey->len;
    buf = subjkey->data + subjkey->len;

    extensions->data = NULL;
    extensions->len = 0;
    while (buf_length > 0) {
        /* EXTENSIONS */
        if (buf[0] == 0xa3) {
            extensions->data = nsslowcert_dataStart(buf, buf_length,
                                                    &extensions->len, PR_FALSE, NULL);
            /* if the DER is bad, we should fail. Previously we accepted
             * bad DER here and treated the extension as missin */
            if (extensions->data == NULL ||
                (extensions->data - buf) + extensions->len != buf_length)
                return SECFailure;
            buf = extensions->data;
            buf_length = extensions->len;
            /* now parse the SEQUENCE holding the extensions. */
            dummy = nsslowcert_dataStart(buf, buf_length, &dummylen, PR_FALSE, NULL);
            if (dummy == NULL ||
                (dummy - buf) + dummylen != buf_length)
                return SECFailure;
            buf_length -= (dummy - buf);
            buf = dummy;
            /* Now parse the extensions inside this sequence */
        }
        dummy = nsslowcert_dataStart(buf, buf_length, &dummylen, PR_FALSE, NULL);
        if (dummy == NULL)
            return SECFailure;
        buf_length -= (dummy - buf) + dummylen;
        buf = dummy + dummylen;
    }
    return SECSuccess;
}

static SECStatus
nsslowcert_GetCertTimes(NSSLOWCERTCertificate *c, PRTime *notBefore, PRTime *notAfter)
{
    int rv;
    NSSLOWCERTValidity validity;

    rv = nsslowcert_GetValidityFields(c->validity.data, c->validity.len,
                                      &validity.notBefore, &validity.notAfter);
    if (rv != SECSuccess) {
        return rv;
    }

    /* convert DER not-before time */
    rv = DER_DecodeTimeChoice(notBefore, &validity.notBefore);
    if (rv) {
        return (SECFailure);
    }

    /* convert DER not-after time */
    rv = DER_DecodeTimeChoice(notAfter, &validity.notAfter);
    if (rv) {
        return (SECFailure);
    }

    return (SECSuccess);
}

/*
 * is certa newer than certb?  If one is expired, pick the other one.
 */
PRBool
nsslowcert_IsNewer(NSSLOWCERTCertificate *certa, NSSLOWCERTCertificate *certb)
{
    PRTime notBeforeA, notAfterA, notBeforeB, notAfterB, now;
    SECStatus rv;
    PRBool newerbefore, newerafter;

    rv = nsslowcert_GetCertTimes(certa, &notBeforeA, &notAfterA);
    if (rv != SECSuccess) {
        return (PR_FALSE);
    }

    rv = nsslowcert_GetCertTimes(certb, &notBeforeB, &notAfterB);
    if (rv != SECSuccess) {
        return (PR_TRUE);
    }

    newerbefore = PR_FALSE;
    if (LL_CMP(notBeforeA, >, notBeforeB)) {
        newerbefore = PR_TRUE;
    }

    newerafter = PR_FALSE;
    if (LL_CMP(notAfterA, >, notAfterB)) {
        newerafter = PR_TRUE;
    }

    if (newerbefore && newerafter) {
        return (PR_TRUE);
    }

    if ((!newerbefore) && (!newerafter)) {
        return (PR_FALSE);
    }

    /* get current time */
    now = PR_Now();

    if (newerbefore) {
        /* cert A was issued after cert B, but expires sooner */
        /* if A is expired, then pick B */
        if (LL_CMP(notAfterA, <, now)) {
            return (PR_FALSE);
        }
        return (PR_TRUE);
    } else {
        /* cert B was issued after cert A, but expires sooner */
        /* if B is expired, then pick A */
        if (LL_CMP(notAfterB, <, now)) {
            return (PR_TRUE);
        }
        return (PR_FALSE);
    }
}

#define SOFT_DEFAULT_CHUNKSIZE 2048

static SECStatus
nsslowcert_KeyFromIssuerAndSN(PLArenaPool *arena,
                              SECItem *issuer, SECItem *sn, SECItem *key)
{
    unsigned int len = sn->len + issuer->len;

    if (!arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    if (len > NSS_MAX_LEGACY_DB_KEY_SIZE) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        goto loser;
    }
    key->data = (unsigned char *)PORT_ArenaAlloc(arena, len);
    if (!key->data) {
        goto loser;
    }

    key->len = len;
    /* copy the serialNumber */
    PORT_Memcpy(key->data, sn->data, sn->len);

    /* copy the issuer */
    PORT_Memcpy(&key->data[sn->len], issuer->data, issuer->len);

    return (SECSuccess);

loser:
    return (SECFailure);
}

static SECStatus
nsslowcert_KeyFromIssuerAndSNStatic(unsigned char *space,
                                    int spaceLen, SECItem *issuer, SECItem *sn, SECItem *key)
{
    unsigned int len = sn->len + issuer->len;

    key->data = pkcs11_allocStaticData(len, space, spaceLen);
    if (!key->data) {
        goto loser;
    }

    key->len = len;
    /* copy the serialNumber */
    PORT_Memcpy(key->data, sn->data, sn->len);

    /* copy the issuer */
    PORT_Memcpy(&key->data[sn->len], issuer->data, issuer->len);

    return (SECSuccess);

loser:
    return (SECFailure);
}

static char *
nsslowcert_EmailName(SECItem *derDN, char *space, unsigned int len)
{
    unsigned char *buf;
    unsigned int buf_length;

    /* unwrap outer sequence */
    buf = nsslowcert_dataStart(derDN->data, derDN->len, &buf_length, PR_FALSE, NULL);
    if (buf == NULL)
        return NULL;

    /* Walk each RDN */
    while (buf_length > 0) {
        unsigned char *rdn;
        unsigned int rdn_length;

        /* grab next rdn */
        rdn = nsslowcert_dataStart(buf, buf_length, &rdn_length, PR_FALSE, NULL);
        if (rdn == NULL) {
            return NULL;
        }
        buf_length -= (rdn - buf) + rdn_length;
        buf = rdn + rdn_length;

        while (rdn_length > 0) {
            unsigned char *ava;
            unsigned int ava_length;
            unsigned char *oid;
            unsigned int oid_length;
            unsigned char *name;
            unsigned int name_length;
            SECItem oidItem;
            SECOidTag type;

            /* unwrap the ava */
            ava = nsslowcert_dataStart(rdn, rdn_length, &ava_length, PR_FALSE,
                                       NULL);
            if (ava == NULL)
                return NULL;
            rdn_length -= (ava - rdn) + ava_length;
            rdn = ava + ava_length;

            oid = nsslowcert_dataStart(ava, ava_length, &oid_length, PR_FALSE,
                                       NULL);
            if (oid == NULL) {
                return NULL;
            }
            ava_length -= (oid - ava) + oid_length;
            ava = oid + oid_length;

            name = nsslowcert_dataStart(ava, ava_length, &name_length, PR_FALSE,
                                        NULL);
            if (name == NULL) {
                return NULL;
            }
            ava_length -= (name - ava) + name_length;
            ava = name + name_length;

            oidItem.data = oid;
            oidItem.len = oid_length;
            type = SECOID_FindOIDTag(&oidItem);
            if ((type == SEC_OID_PKCS9_EMAIL_ADDRESS) ||
                (type == SEC_OID_RFC1274_MAIL)) {
                /* Email is supposed to be IA5String, so no
                 * translation necessary */
                char *emailAddr;
                emailAddr = (char *)pkcs11_copyStaticData(name, name_length + 1,
                                                          (unsigned char *)space, len);
                if (emailAddr) {
                    emailAddr[name_length] = 0;
                }
                return emailAddr;
            }
        }
    }
    return NULL;
}

static char *
nsslowcert_EmailAltName(NSSLOWCERTCertificate *cert, char *space,
                        unsigned int len)
{
    unsigned char *exts;
    unsigned int exts_length;

    /* unwrap the sequence */
    exts = nsslowcert_dataStart(cert->extensions.data, cert->extensions.len,
                                &exts_length, PR_FALSE, NULL);
    /* loop through extension */
    while (exts && exts_length > 0) {
        unsigned char *ext;
        unsigned int ext_length;
        unsigned char *oid;
        unsigned int oid_length;
        unsigned char *nameList;
        unsigned int nameList_length;
        SECItem oidItem;
        SECOidTag type;

        ext = nsslowcert_dataStart(exts, exts_length, &ext_length,
                                   PR_FALSE, NULL);
        if (ext == NULL) {
            break;
        }
        exts_length -= (ext - exts) + ext_length;
        exts = ext + ext_length;

        oid = nsslowcert_dataStart(ext, ext_length, &oid_length, PR_FALSE, NULL);
        if (oid == NULL) {
            break;
        }
        ext_length -= (oid - ext) + oid_length;
        ext = oid + oid_length;
        oidItem.data = oid;
        oidItem.len = oid_length;
        type = SECOID_FindOIDTag(&oidItem);

        /* get Alt Extension */
        if (type != SEC_OID_X509_SUBJECT_ALT_NAME) {
            continue;
        }

        /* skip passed the critical flag */
        if (ext[0] == 0x01) { /* BOOLEAN */
            unsigned char *dummy;
            unsigned int dummy_length;
            dummy = nsslowcert_dataStart(ext, ext_length, &dummy_length,
                                         PR_FALSE, NULL);
            if (dummy == NULL) {
                break;
            }
            ext_length -= (dummy - ext) + dummy_length;
            ext = dummy + dummy_length;
        }

        /* unwrap the name list */
        nameList = nsslowcert_dataStart(ext, ext_length, &nameList_length,
                                        PR_FALSE, NULL);
        if (nameList == NULL) {
            break;
        }
        ext_length -= (nameList - ext) + nameList_length;
        ext = nameList + nameList_length;
        nameList = nsslowcert_dataStart(nameList, nameList_length,
                                        &nameList_length, PR_FALSE, NULL);
        /* loop through the name list */
        while (nameList && nameList_length > 0) {
            unsigned char *thisName;
            unsigned int thisName_length;

            thisName = nsslowcert_dataStart(nameList, nameList_length,
                                            &thisName_length, PR_FALSE, NULL);
            if (thisName == NULL) {
                break;
            }
            if (nameList[0] == 0xa2) { /* DNS Name */
                SECItem dn;
                char *emailAddr;

                dn.data = thisName;
                dn.len = thisName_length;
                emailAddr = nsslowcert_EmailName(&dn, space, len);
                if (emailAddr) {
                    return emailAddr;
                }
            }
            if (nameList[0] == 0x81) { /* RFC 822name */
                char *emailAddr;
                emailAddr = (char *)pkcs11_copyStaticData(thisName,
                                                          thisName_length + 1, (unsigned char *)space, len);
                if (emailAddr) {
                    emailAddr[thisName_length] = 0;
                }
                return emailAddr;
            }
            nameList_length -= (thisName - nameList) + thisName_length;
            nameList = thisName + thisName_length;
        }
        break;
    }
    return NULL;
}

static char *
nsslowcert_GetCertificateEmailAddress(NSSLOWCERTCertificate *cert)
{
    char *emailAddr = NULL;
    char *str;

    emailAddr = nsslowcert_EmailName(&cert->derSubject, cert->emailAddrSpace,
                                     sizeof(cert->emailAddrSpace));
    /* couldn't find the email address in the DN, check the subject Alt name */
    if (!emailAddr && cert->extensions.data) {
        emailAddr = nsslowcert_EmailAltName(cert, cert->emailAddrSpace,
                                            sizeof(cert->emailAddrSpace));
    }

    /* make it lower case */
    str = emailAddr;
    while (str && *str) {
        *str = tolower(*str);
        str++;
    }
    return emailAddr;
}

/*
 * take a DER certificate and decode it into a certificate structure
 */
NSSLOWCERTCertificate *
nsslowcert_DecodeDERCertificate(SECItem *derSignedCert, char *nickname)
{
    NSSLOWCERTCertificate *cert;
    int rv;

    /* allocate the certificate structure */
    cert = nsslowcert_CreateCert();

    if (!cert) {
        goto loser;
    }

    /* point to passed in DER data */
    cert->derCert = *derSignedCert;
    cert->nickname = NULL;
    cert->certKey.data = NULL;
    cert->referenceCount = 1;

    /* decode the certificate info */
    rv = nsslowcert_GetCertFields(cert->derCert.data, cert->derCert.len,
                                  &cert->derIssuer, &cert->serialNumber, &cert->derSN, &cert->derSubject,
                                  &cert->validity, &cert->derSubjKeyInfo, &cert->extensions);

    if (rv != SECSuccess) {
        goto loser;
    }

    /* cert->subjectKeyID;   x509v3 subject key identifier */
    cert->subjectKeyID.data = NULL;
    cert->subjectKeyID.len = 0;
    cert->dbEntry = NULL;
    cert->trust = NULL;
    cert->dbhandle = NULL;

    /* generate and save the database key for the cert */
    rv = nsslowcert_KeyFromIssuerAndSNStatic(cert->certKeySpace,
                                             sizeof(cert->certKeySpace), &cert->derIssuer,
                                             &cert->serialNumber, &cert->certKey);
    if (rv) {
        goto loser;
    }

    /* set the nickname */
    if (nickname == NULL) {
        cert->nickname = NULL;
    } else {
        /* copy and install the nickname */
        cert->nickname = pkcs11_copyNickname(nickname, cert->nicknameSpace,
                                             sizeof(cert->nicknameSpace));
    }

#ifdef FIXME
    /* initialize the subjectKeyID */
    rv = cert_GetKeyID(cert);
    if (rv != SECSuccess) {
        goto loser;
    }
#endif

    /* set the email address */
    cert->emailAddr = nsslowcert_GetCertificateEmailAddress(cert);

    cert->referenceCount = 1;

    return (cert);

loser:
    if (cert) {
        nsslowcert_DestroyCertificate(cert);
    }

    return (0);
}

char *
nsslowcert_FixupEmailAddr(char *emailAddr)
{
    char *retaddr;
    char *str;

    if (emailAddr == NULL) {
        return (NULL);
    }

    /* copy the string */
    str = retaddr = PORT_Strdup(emailAddr);
    if (str == NULL) {
        return (NULL);
    }

    /* make it lower case */
    while (*str) {
        *str = tolower(*str);
        str++;
    }

    return (retaddr);
}

/*
 * Generate a database key, based on serial number and issuer, from a
 * DER certificate.
 */
SECStatus
nsslowcert_KeyFromDERCert(PLArenaPool *arena, SECItem *derCert, SECItem *key)
{
    int rv;
    NSSLOWCERTCertKey certkey;

    PORT_Memset(&certkey, 0, sizeof(NSSLOWCERTCertKey));

    rv = nsslowcert_GetCertFields(derCert->data, derCert->len,
                                  &certkey.derIssuer, &certkey.serialNumber, NULL, NULL,
                                  NULL, NULL, NULL);

    if (rv) {
        goto loser;
    }

    return (nsslowcert_KeyFromIssuerAndSN(arena, &certkey.derIssuer,
                                          &certkey.serialNumber, key));
loser:
    return (SECFailure);
}

NSSLOWKEYPublicKey *
nsslowcert_ExtractPublicKey(NSSLOWCERTCertificate *cert)
{
    NSSLOWCERTSubjectPublicKeyInfo spki;
    NSSLOWKEYPublicKey *pubk;
    SECItem os;
    SECStatus rv;
    PLArenaPool *arena;
    SECOidTag tag;
    SECItem newDerSubjKeyInfo;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        return NULL;

    pubk = (NSSLOWKEYPublicKey *)
        PORT_ArenaZAlloc(arena, sizeof(NSSLOWKEYPublicKey));
    if (pubk == NULL) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    pubk->arena = arena;
    PORT_Memset(&spki, 0, sizeof(spki));

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newDerSubjKeyInfo, &cert->derSubjKeyInfo);
    if (rv != SECSuccess) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    /* we haven't bothered decoding the spki struct yet, do it now */
    rv = SEC_QuickDERDecodeItem(arena, &spki,
                                nsslowcert_SubjectPublicKeyInfoTemplate, &newDerSubjKeyInfo);
    if (rv != SECSuccess) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    /* Convert bit string length from bits to bytes */
    os = spki.subjectPublicKey;
    DER_ConvertBitString(&os);

    tag = SECOID_GetAlgorithmTag(&spki.algorithm);
    switch (tag) {
        case SEC_OID_X500_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            pubk->keyType = NSSLOWKEYRSAKey;
            prepare_low_rsa_pub_key_for_asn1(pubk);
            rv = SEC_QuickDERDecodeItem(arena, pubk,
                                        nsslowcert_RSAPublicKeyTemplate, &os);
            if (rv == SECSuccess)
                return pubk;
            break;
        case SEC_OID_ANSIX9_DSA_SIGNATURE:
            pubk->keyType = NSSLOWKEYDSAKey;
            prepare_low_dsa_pub_key_for_asn1(pubk);
            rv = SEC_QuickDERDecodeItem(arena, pubk,
                                        nsslowcert_DSAPublicKeyTemplate, &os);
            if (rv == SECSuccess)
                return pubk;
            break;
        case SEC_OID_X942_DIFFIE_HELMAN_KEY:
            pubk->keyType = NSSLOWKEYDHKey;
            prepare_low_dh_pub_key_for_asn1(pubk);
            rv = SEC_QuickDERDecodeItem(arena, pubk,
                                        nsslowcert_DHPublicKeyTemplate, &os);
            if (rv == SECSuccess)
                return pubk;
            break;
#ifndef NSS_DISABLE_ECC
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            pubk->keyType = NSSLOWKEYECKey;
            /* Since PKCS#11 directly takes the DER encoding of EC params
             * and public value, we don't need any decoding here.
             */
            rv = SECITEM_CopyItem(arena, &pubk->u.ec.ecParams.DEREncoding,
                                  &spki.algorithm.parameters);
            if (rv != SECSuccess)
                break;

            /* Fill out the rest of the ecParams structure
             * based on the encoded params
             */
            if (LGEC_FillParams(arena, &pubk->u.ec.ecParams.DEREncoding,
                                &pubk->u.ec.ecParams) != SECSuccess)
                break;

            rv = SECITEM_CopyItem(arena, &pubk->u.ec.publicValue, &os);
            if (rv == SECSuccess)
                return pubk;
            break;
#endif /* NSS_DISABLE_ECC */
        default:
            rv = SECFailure;
            break;
    }

    lg_nsslowkey_DestroyPublicKey(pubk);
    return NULL;
}
