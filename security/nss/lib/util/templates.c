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
 * The Original Code is the Network Security Services libraries.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * Templates that are compiled and exported from both libnss3 and libnssutil3.
 * They have to be, because they were previously exported from libnss3, and
 * there is no way to create data forwarder symbols on Unix.
 *
 * Please do not add to this file. New shared templates should be exported
 * from libnssutil3 only.
 *
 */

#include "utilrename.h"
#include "secasn1.h"
#include "secder.h"
#include "secoid.h"
#include "secdig.h"

const SEC_ASN1Template SECOID_AlgorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SECAlgorithmID) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SECAlgorithmID,algorithm), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(SECAlgorithmID,parameters), },
    { 0, }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SECOID_AlgorithmIDTemplate)

const SEC_ASN1Template SEC_AnyTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_AnyTemplate)

const SEC_ASN1Template SEC_BMPStringTemplate[] = {
    { SEC_ASN1_BMP_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BMPStringTemplate)

const SEC_ASN1Template SEC_BitStringTemplate[] = {
    { SEC_ASN1_BIT_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BitStringTemplate)

const SEC_ASN1Template SEC_BooleanTemplate[] = {
    { SEC_ASN1_BOOLEAN, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BooleanTemplate)

const SEC_ASN1Template SEC_GeneralizedTimeTemplate[] = {
    { SEC_ASN1_GENERALIZED_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_GeneralizedTimeTemplate)

const SEC_ASN1Template SEC_IA5StringTemplate[] = {
    { SEC_ASN1_IA5_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_IA5StringTemplate)

const SEC_ASN1Template SEC_IntegerTemplate[] = {
    { SEC_ASN1_INTEGER, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_IntegerTemplate)

const SEC_ASN1Template SEC_NullTemplate[] = {
    { SEC_ASN1_NULL, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_NullTemplate)

const SEC_ASN1Template SEC_ObjectIDTemplate[] = {
    { SEC_ASN1_OBJECT_ID, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_ObjectIDTemplate)

const SEC_ASN1Template SEC_OctetStringTemplate[] = {
    { SEC_ASN1_OCTET_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_OctetStringTemplate)

const SEC_ASN1Template SEC_PointerToAnyTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_AnyTemplate }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_PointerToAnyTemplate)

const SEC_ASN1Template SEC_PointerToOctetStringTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_MAY_STREAM, 0, SEC_OctetStringTemplate }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_PointerToOctetStringTemplate)

const SEC_ASN1Template SEC_SetOfAnyTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SEC_AnyTemplate }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_SetOfAnyTemplate)

const SEC_ASN1Template SEC_UTCTimeTemplate[] = {
    { SEC_ASN1_UTC_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_UTCTimeTemplate)

const SEC_ASN1Template SEC_UTF8StringTemplate[] = {
    { SEC_ASN1_UTF8_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

SEC_ASN1_CHOOSER_IMPLEMENT(SEC_UTF8StringTemplate)

/* XXX See comment below about SGN_DecodeDigestInfo -- keep this static! */
/* XXX Changed from static -- need to change name? */
const SEC_ASN1Template sgn_DigestInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SGNDigestInfo) },
    { SEC_ASN1_INLINE,
	  offsetof(SGNDigestInfo,digestAlgorithm),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SGNDigestInfo,digest) },
    { 0 }
};

SEC_ASN1_CHOOSER_IMPLEMENT(sgn_DigestInfoTemplate)
