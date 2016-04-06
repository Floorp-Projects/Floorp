/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code for dealing with X509.V3 extensions.
 */

#include "cert.h"
#include "secitem.h"
#include "secoid.h"
#include "secder.h"
#include "secasn1.h"
#include "certxutl.h"
#include "secerr.h"

SECStatus
CERT_FindCertExtensionByOID(CERTCertificate *cert, SECItem *oid, SECItem *value)
{
    return (cert_FindExtensionByOID(cert->extensions, oid, value));
}

SECStatus
CERT_FindCertExtension(const CERTCertificate *cert, int tag, SECItem *value)
{
    return (cert_FindExtension(cert->extensions, tag, value));
}

static void
SetExts(void *object, CERTCertExtension **exts)
{
    CERTCertificate *cert = (CERTCertificate *)object;

    cert->extensions = exts;
    DER_SetUInteger(cert->arena, &(cert->version), SEC_CERTIFICATE_VERSION_3);
}

void *
CERT_StartCertExtensions(CERTCertificate *cert)
{
    return (cert_StartExtensions((void *)cert, cert->arena, SetExts));
}

/*
 * get the value of the Netscape Certificate Type Extension
 */
SECStatus
CERT_FindNSCertTypeExtension(CERTCertificate *cert, SECItem *retItem)
{

    return (CERT_FindBitStringExtension(
        cert->extensions, SEC_OID_NS_CERT_EXT_CERT_TYPE, retItem));
}

/*
 * get the value of a string type extension
 */
char *
CERT_FindNSStringExtension(CERTCertificate *cert, int oidtag)
{
    SECItem wrapperItem, tmpItem = { siBuffer, 0 };
    SECStatus rv;
    PLArenaPool *arena = NULL;
    char *retstring = NULL;

    wrapperItem.data = NULL;
    tmpItem.data = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if (!arena) {
        goto loser;
    }

    rv = cert_FindExtension(cert->extensions, oidtag, &wrapperItem);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SEC_QuickDERDecodeItem(
        arena, &tmpItem, SEC_ASN1_GET(SEC_IA5StringTemplate), &wrapperItem);

    if (rv != SECSuccess) {
        goto loser;
    }

    retstring = (char *)PORT_Alloc(tmpItem.len + 1);
    if (retstring == NULL) {
        goto loser;
    }

    PORT_Memcpy(retstring, tmpItem.data, tmpItem.len);
    retstring[tmpItem.len] = '\0';

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    if (wrapperItem.data) {
        PORT_Free(wrapperItem.data);
    }

    return (retstring);
}

/*
 * get the value of the X.509 v3 Key Usage Extension
 */
SECStatus
CERT_FindKeyUsageExtension(CERTCertificate *cert, SECItem *retItem)
{

    return (CERT_FindBitStringExtension(cert->extensions,
                                        SEC_OID_X509_KEY_USAGE, retItem));
}

/*
 * get the value of the X.509 v3 Key Usage Extension
 */
SECStatus
CERT_FindSubjectKeyIDExtension(CERTCertificate *cert, SECItem *retItem)
{

    SECStatus rv;
    SECItem encodedValue = { siBuffer, NULL, 0 };
    SECItem decodedValue = { siBuffer, NULL, 0 };

    rv = cert_FindExtension(cert->extensions, SEC_OID_X509_SUBJECT_KEY_ID,
                            &encodedValue);
    if (rv == SECSuccess) {
        PORTCheapArenaPool tmpArena;
        PORT_InitCheapArena(&tmpArena, DER_DEFAULT_CHUNKSIZE);
        rv = SEC_QuickDERDecodeItem(&tmpArena.arena, &decodedValue,
                                    SEC_ASN1_GET(SEC_OctetStringTemplate),
                                    &encodedValue);
        if (rv == SECSuccess) {
            rv = SECITEM_CopyItem(NULL, retItem, &decodedValue);
        }
        PORT_DestroyCheapArena(&tmpArena);
    }
    SECITEM_FreeItem(&encodedValue, PR_FALSE);
    return rv;
}

SECStatus
CERT_FindBasicConstraintExten(CERTCertificate *cert,
                              CERTBasicConstraints *value)
{
    SECItem encodedExtenValue;
    SECStatus rv;

    encodedExtenValue.data = NULL;
    encodedExtenValue.len = 0;

    rv = cert_FindExtension(cert->extensions, SEC_OID_X509_BASIC_CONSTRAINTS,
                            &encodedExtenValue);
    if (rv != SECSuccess) {
        return (rv);
    }

    rv = CERT_DecodeBasicConstraintValue(value, &encodedExtenValue);

    /* free the raw extension data */
    PORT_Free(encodedExtenValue.data);
    encodedExtenValue.data = NULL;

    return (rv);
}

CERTAuthKeyID *
CERT_FindAuthKeyIDExten(PLArenaPool *arena, CERTCertificate *cert)
{
    SECItem encodedExtenValue;
    SECStatus rv;
    CERTAuthKeyID *ret;

    encodedExtenValue.data = NULL;
    encodedExtenValue.len = 0;

    rv = cert_FindExtension(cert->extensions, SEC_OID_X509_AUTH_KEY_ID,
                            &encodedExtenValue);
    if (rv != SECSuccess) {
        return (NULL);
    }

    ret = CERT_DecodeAuthKeyID(arena, &encodedExtenValue);

    PORT_Free(encodedExtenValue.data);
    encodedExtenValue.data = NULL;

    return (ret);
}

SECStatus
CERT_CheckCertUsage(CERTCertificate *cert, unsigned char usage)
{
    SECItem keyUsage;
    SECStatus rv;

    /* There is no extension, v1 or v2 certificate */
    if (cert->extensions == NULL) {
        return (SECSuccess);
    }

    keyUsage.data = NULL;

    /* This code formerly ignored the Key Usage extension if it was
    ** marked non-critical.  That was wrong.  Since we do understand it,
    ** we are obligated to honor it, whether or not it is critical.
    */
    rv = CERT_FindKeyUsageExtension(cert, &keyUsage);
    if (rv == SECFailure) {
        rv = (PORT_GetError() == SEC_ERROR_EXTENSION_NOT_FOUND) ? SECSuccess
                                                                : SECFailure;
    } else if (!(keyUsage.data[0] & usage)) {
        PORT_SetError(SEC_ERROR_CERT_USAGES_INVALID);
        rv = SECFailure;
    }
    PORT_Free(keyUsage.data);
    return (rv);
}
