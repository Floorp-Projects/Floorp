/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "certt.h"

typedef struct PKUPEncodedContext{
    SECItem notBefore;
    SECItem notAfter;
    /*    SECItem encodedValue; */
    PRArenaPool *arena;
}PKUPEncodedContext;

typedef struct AltNameEncodedContext{
    SECItem **encodedGenName;
}AltNameEncodedContext;


typedef struct NameConstraint{
    CERTGeneralName  generalName;
    int              min;
    int              max;
}NameConstraint;



extern SECStatus
CERT_EncodePublicKeyUsagePeriod(PRArenaPool *arena, PKUPEncodedContext *pkup,
				SECItem *encodedValue);

extern SECStatus
CERT_EncodeNameConstraintsExtension(PRArenaPool *arena, CERTNameConstraints  *value,
			    SECItem *encodedValue);
extern CERTGeneralName *
CERT_DecodeAltNameExtension(PRArenaPool *arena, SECItem *EncodedAltName);

extern CERTNameConstraints *
CERT_DecodeNameConstraintsExtension(PRArenaPool *arena, SECItem *encodedConstraints);

extern SECStatus 
CERT_EncodeSubjectKeyID(PRArenaPool *arena, char *value, int len, SECItem *encodedValue);

extern SECStatus 
CERT_EncodeIA5TypeExtension(PRArenaPool *arena, char *value, SECItem *encodedValue);

CERTAuthInfoAccess **
cert_DecodeAuthInfoAccessExtension(PRArenaPool *arena,
				   SECItem     *encodedExtension);

SECStatus
cert_EncodeAuthInfoAccessExtension(PRArenaPool *arena,
				   CERTAuthInfoAccess **info,
				   SECItem *dest);
