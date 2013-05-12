/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code for dealing with x.509 v3 crl and crl entries extensions.
 */

#include "cert.h"
#include "secitem.h"
#include "secoid.h"
#include "secoidt.h"
#include "secder.h"
#include "secasn1.h"
#include "certxutl.h"

SECStatus
CERT_FindCRLExtensionByOID(CERTCrl *crl, SECItem *oid, SECItem *value)
{
    return (cert_FindExtensionByOID (crl->extensions, oid, value));
}
    

SECStatus
CERT_FindCRLExtension(CERTCrl *crl, int tag, SECItem *value)
{
    return (cert_FindExtension (crl->extensions, tag, value));
}


/* Callback to set extensions and adjust verison */
static void
SetCrlExts(void *object, CERTCertExtension **exts)
{
    CERTCrl *crl = (CERTCrl *)object;

    crl->extensions = exts;
    DER_SetUInteger (crl->arena, &crl->version, SEC_CRL_VERSION_2);
}

void *
CERT_StartCRLExtensions(CERTCrl *crl)
{
    return (cert_StartExtensions ((void *)crl, crl->arena, SetCrlExts));
}

static void
SetCrlEntryExts(void *object, CERTCertExtension **exts)
{
    CERTCrlEntry *crlEntry = (CERTCrlEntry *)object;

    crlEntry->extensions = exts;
}

void *
CERT_StartCRLEntryExtensions(CERTCrl *crl, CERTCrlEntry *entry)
{
    return (cert_StartExtensions (entry, crl->arena, SetCrlEntryExts));
}

SECStatus CERT_FindCRLNumberExten (PLArenaPool *arena, CERTCrl *crl,
                                   SECItem *value)
{
    SECItem encodedExtenValue;
    SECItem *tmpItem = NULL;
    SECStatus rv;
    void *mark = NULL;

    encodedExtenValue.data = NULL;
    encodedExtenValue.len = 0;

    rv = cert_FindExtension(crl->extensions, SEC_OID_X509_CRL_NUMBER,
			  &encodedExtenValue);
    if ( rv != SECSuccess )
	return (rv);

    mark = PORT_ArenaMark(arena);

    tmpItem = SECITEM_ArenaDupItem(arena, &encodedExtenValue);
    if (tmpItem) {
        rv = SEC_QuickDERDecodeItem (arena, value,
                                     SEC_ASN1_GET(SEC_IntegerTemplate),
                                     tmpItem);
    } else {
        rv = SECFailure;
    }

    PORT_Free (encodedExtenValue.data);
    if (rv == SECFailure) {
        PORT_ArenaRelease(arena, mark);
    } else {
        PORT_ArenaUnmark(arena, mark);
    }
    return (rv);
}

SECStatus CERT_FindCRLEntryReasonExten (CERTCrlEntry *crlEntry,
                                        CERTCRLEntryReasonCode *value)
{
    SECItem wrapperItem = {siBuffer,0};
    SECItem tmpItem = {siBuffer,0};
    SECStatus rv;
    PLArenaPool *arena = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);   
    if ( ! arena ) {
	return(SECFailure);
    }
    
    rv = cert_FindExtension(crlEntry->extensions, SEC_OID_X509_REASON_CODE, 
                            &wrapperItem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(arena, &tmpItem,
                                SEC_ASN1_GET(SEC_EnumeratedTemplate),
                                &wrapperItem);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    *value = (CERTCRLEntryReasonCode) DER_GetInteger(&tmpItem);

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    if ( wrapperItem.data ) {
	PORT_Free(wrapperItem.data);
    }

    return (rv);
}

SECStatus CERT_FindInvalidDateExten (CERTCrl *crl, PRTime *value)
{
    SECItem encodedExtenValue;
    SECItem decodedExtenValue = {siBuffer,0};
    SECStatus rv;

    encodedExtenValue.data = decodedExtenValue.data = NULL;
    encodedExtenValue.len = decodedExtenValue.len = 0;

    rv = cert_FindExtension
	 (crl->extensions, SEC_OID_X509_INVALID_DATE, &encodedExtenValue);
    if ( rv != SECSuccess )
	return (rv);

    rv = SEC_ASN1DecodeItem (NULL, &decodedExtenValue,
			     SEC_ASN1_GET(SEC_GeneralizedTimeTemplate),
                             &encodedExtenValue);
    if (rv == SECSuccess)
	rv = DER_GeneralizedTimeToTime(value, &encodedExtenValue);
    PORT_Free (decodedExtenValue.data);
    PORT_Free (encodedExtenValue.data);
    return (rv);
}
