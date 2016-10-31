/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

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

