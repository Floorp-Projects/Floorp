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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: certdecode.c,v $ $Revision: 1.14 $ $Date: 2003/11/11 21:46:53 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifdef NSS_3_4_CODE
/* This is defined in pki3hack.c */
NSS_EXTERN nssDecodedCert *
nssDecodedPKIXCertificate_Create (
  NSSArena *arenaOpt,
  NSSDER *encoding
);

NSS_IMPLEMENT PRStatus
nssDecodedPKIXCertificate_Destroy (
  nssDecodedCert *dc
);
#else /* NSS_4_0_CODE */
/* This is where 4.0 PKIX code will handle the decoding */
static nssDecodedCert *
nssDecodedPKIXCertificate_Create (
  NSSArena *arenaOpt,
  NSSDER *encoding
)
{
    return (nssDecodedCert *)NULL;
}

static PRStatus
nssDecodedPKIXCertificate_Destroy (
  nssDecodedCert *dc
)
{
    return PR_FAILURE;
}
#endif /* not NSS_3_4_CODE */

NSS_IMPLEMENT nssDecodedCert *
nssDecodedCert_Create (
  NSSArena *arenaOpt,
  NSSDER *encoding,
  NSSCertificateType type
)
{
    nssDecodedCert *rvDC = NULL;
    switch(type) {
    case NSSCertificateType_PKIX:
	rvDC = nssDecodedPKIXCertificate_Create(arenaOpt, encoding);
	break;
    default:
#if 0
	nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
#endif
	return (nssDecodedCert *)NULL;
    }
    return rvDC;
}

NSS_IMPLEMENT PRStatus
nssDecodedCert_Destroy (
  nssDecodedCert *dc
)
{
    if (!dc) {
	return PR_FAILURE;
    }
    switch(dc->type) {
    case NSSCertificateType_PKIX:
	return nssDecodedPKIXCertificate_Destroy(dc);
    default:
#if 0
	nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
#endif
	break;
    }
    return PR_FAILURE;
}

