/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Code for dealing with x.509 v3 crl and crl entries extensions.
 *
 * $Id: crlv2.c,v 1.4 2005/06/30 20:53:57 wtchang%redhat.com Exp $
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

SECStatus CERT_FindCRLNumberExten (CERTCrl *crl, CERTCrlNumber *value)
{
    SECItem encodedExtenValue;
    SECStatus rv;

    encodedExtenValue.data = NULL;
    encodedExtenValue.len = 0;

    rv = cert_FindExtension
	 (crl->extensions, SEC_OID_X509_CRL_NUMBER, &encodedExtenValue);
    if ( rv != SECSuccess )
	return (rv);

    rv = SEC_ASN1DecodeItem (NULL, value, SEC_IntegerTemplate,
			     &encodedExtenValue);
    PORT_Free (encodedExtenValue.data);
    return (rv);
}

SECStatus CERT_FindCRLReasonExten (CERTCrl *crl, SECItem *value)
{
    return (CERT_FindBitStringExtension
	    (crl->extensions, SEC_OID_X509_REASON_CODE, value));    
}

SECStatus CERT_FindInvalidDateExten (CERTCrl *crl, int64 *value)
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
			     SEC_GeneralizedTimeTemplate, &encodedExtenValue);
    if (rv == SECSuccess)
	rv = DER_GeneralizedTimeToTime(value, &encodedExtenValue);
    PORT_Free (decodedExtenValue.data);
    PORT_Free (encodedExtenValue.data);
    return (rv);
}
