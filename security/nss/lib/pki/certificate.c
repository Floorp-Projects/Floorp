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
static const char CVS_ID[] = "@(#) $RCSfile: certificate.c,v $ $Revision: 1.3 $ $Date: 2001/09/18 20:54:57 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

/* I assume the following accessors into cert fields will be needed.
 * We need to be able to return basic cert info, however, these are
 * really PKCS#11 fields, so maybe not these in particular (mcgreer)
 */
NSS_IMPLEMENT NSSUTF8 *
NSSCertificate_GetLabel
(
  NSSCertificate *c
)
{
    NSSUTF8 *rvStr;
    rvStr = nssUTF8_Create(NULL, nssStringType_PrintableString, 
                           c->label.data, c->label.size);
    return rvStr;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_GetID
(
  NSSCertificate *c
)
{
    return &c->id;
}

/* NSS needs access to this function, but does anyone else? */
static NSSCertificate *
NSSCertificate_Create
(
 /* blah blah blah */
)
{
    NSSArena *arena;
    NSSCertificate *rvCert;
    arena = NSSArena_Create();
    if(!arena) {
	return (NSSCertificate *)NULL;
    }
    rvCert = nss_ZNEW(arena, NSSCertificate);
    if (!rvCert) {
	goto loser;
    }
    rvCert->refCount = 1;
    rvCert->arena = arena;
    return rvCert;
loser:
    if (arena) {
	nssArena_Destroy(arena);
    }
    return (NSSCertificate *)NULL;
}

/* Create a certificate from an object handle. */
NSS_IMPLEMENT NSSCertificate *
NSSCertificate_CreateFromHandle
(
  CK_OBJECT_HANDLE object,
  nssSession *session,
  NSSSlot *slot
)
{
    NSSCertificate *rvCert;
    PRStatus nssrv;
    CK_ULONG template_size;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_ID,    NULL, 0 },
	{ CKA_VALUE, NULL, 0 },
	{ CKA_LABEL, NULL, 0 },
    };
    template_size = sizeof(cert_template) / sizeof(cert_template[0]);
    rvCert = NSSCertificate_Create();
    if (!rvCert) {
	return (NSSCertificate *)NULL;
    }
    rvCert->handle = object;
    rvCert->slot = slot;
    nssrv = NSSCKObject_GetAttributes(object, cert_template, template_size,
                                      rvCert->arena, session, rvCert->slot);
    if (nssrv) {
	/* okay, but if failed because one of the attributes could not be
	 * found, do it gracefully (i.e., catch the error).
	 */
	goto loser;
    }
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[0], &rvCert->id);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[1], &rvCert->der);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[2], &rvCert->label);
    return rvCert;
loser:
    NSSCertificate_Destroy(rvCert);
    return (NSSCertificate *)NULL;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Destroy
(
  NSSCertificate *c
)
{
    if (--c->refCount == 0) {
	return NSSArena_Destroy(c->arena);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_DeleteStoredObject
(
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Validate
(
  NSSCertificate *c,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt /* NULL for none */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT void ** /* void *[] */
NSSCertificate_ValidateCompletely
(
  NSSCertificate *c,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt, /* NULL for none */
  void **rvOpt, /* NULL for allocate */
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt /* NULL for heap */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_ValidateAndDiscoverUsagesAndPolicies
(
  NSSCertificate *c,
  NSSTime **notBeforeOutOpt,
  NSSTime **notAfterOutOpt,
  void *allowedUsages,
  void *disallowedUsages,
  void *allowedPolicies,
  void *disallowedPolicies,
  /* more args.. work on this fgmr */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSDER *
NSSCertificate_Encode
(
  NSSCertificate *c,
  NSSDER *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSCertificate_BuildChain
(
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSCertificate_GetTrustDomain
(
  NSSCertificate *c
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSCertificate_GetToken
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSCertificate_GetSlot
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSModule *
NSSCertificate_GetModule
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_Encrypt
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Verify
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSItem *signature,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_VerifyRecover
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *signature,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_WrapSymmetricKey
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSSymmetricKey *keyToWrap,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSCertificate_CreateCryptoContext
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh  
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPublicKey *
NSSCertificate_GetPublicKey
(
  NSSCertificate *c
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPrivateKey *
NSSCertificate_FindPrivateKey
(
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRBool
NSSCertificate_IsPrivateKeyAvailable
(
  NSSCertificate *c,
  NSSCallback *uhh,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSS_IMPLEMENT PRBool
NSSUserCertificate_IsStillPresent
(
  NSSUserCertificate *uc,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_Decrypt
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_Sign
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_SignRecover
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSUserCertificate_UnwrapSymmetricKey
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *wrappedKey,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSUserCertificate_DeriveSymmetricKey
(
  NSSUserCertificate *uc, /* provides private key */
  NSSCertificate *c, /* provides public key */
  NSSAlgorithmAndParameters *apOpt,
  NSSOID *target,
  PRUint32 keySizeOpt, /* zero for best allowed */
  NSSOperations operations,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

