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
static const char CVS_ID[] = "@(#) $RCSfile: ckhelper.c,v $ $Revision: 1.8 $ $Date: 2001/10/19 18:10:57 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifdef NSS_3_4_CODE
#include "pkcs11.h"
#else
#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */
#endif /* NSS_3_4_CODE */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

static const CK_BBOOL s_true = CK_TRUE;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_true = { (CK_VOID_PTR)&s_true, sizeof(s_true) };

static const CK_BBOOL s_false = CK_FALSE;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_false = { (CK_VOID_PTR)&s_false, sizeof(s_false) };

static const CK_OBJECT_CLASS s_class_cert = CKO_CERTIFICATE;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_class_cert = { (CK_VOID_PTR)&s_class_cert, sizeof(s_class_cert) };

static const CK_OBJECT_CLASS s_class_pubkey = CKO_PUBLIC_KEY;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_class_pubkey = { (CK_VOID_PTR)&s_class_pubkey, sizeof(s_class_pubkey) };

static const CK_OBJECT_CLASS s_class_privkey = CKO_PRIVATE_KEY;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_class_privkey = { (CK_VOID_PTR)&s_class_privkey, sizeof(s_class_privkey) };

NSS_IMPLEMENT PRStatus 
nssCKObject_GetAttributes
(
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG count,
  NSSArena *arenaOpt,
  nssSession *session,
  NSSSlot *slot
)
{
   nssArenaMark *mark;
   CK_SESSION_HANDLE hSession;
   CK_ULONG i;
   CK_RV ckrv;
   PRStatus nssrv;
   PRBool alloced = PR_FALSE;
   hSession = session->handle; 
   if (arenaOpt) {
	mark = nssArena_Mark(arenaOpt);
	if (!mark) {
	    goto loser;
	}
   }
   nssSession_EnterMonitor(session);
   /* XXX kinda hacky, if the storage size is already in the first template
    * item, then skip the alloc portion
    */
   if (obj_template[0].ulValueLen == 0) {
	/* Get the storage size needed for each attribute */
	ckrv = CKAPI(slot)->C_GetAttributeValue(hSession,
	                                        object, obj_template, count);
	if (ckrv != CKR_OK) {
	    nssSession_ExitMonitor(session);
	    /* set an error here */
	    goto loser;
	}
	/* Allocate memory for each attribute. */
	for (i=0; i<count; i++) {
	    obj_template[i].pValue = nss_ZAlloc(arenaOpt, 
	                                        obj_template[i].ulValueLen);
	    if (!obj_template[i].pValue) {
		nssSession_ExitMonitor(session);
		goto loser;
	    }
	}
	alloced = PR_TRUE;
   }
   /* Obtain the actual attribute values. */
   ckrv = CKAPI(slot)->C_GetAttributeValue(hSession,
                                           object, obj_template, count);
   nssSession_ExitMonitor(session);
   if (ckrv != CKR_OK) {
	/* set an error here */
	goto loser;
   }
   if (alloced && arenaOpt) {
	nssrv = nssArena_Unmark(arenaOpt, mark);
	if (nssrv != PR_SUCCESS) {
	    goto loser;
	}
   }
   return PR_SUCCESS;
loser:
   if (alloced) {
	    if (arenaOpt) {
	    /* release all arena memory allocated before the failure. */
	    (void)nssArena_Release(arenaOpt, mark);
	} else {
	    CK_ULONG j;
	    /* free each heap object that was allocated before the failure. */
	    for (j=0; j<i; j++) {
		nss_ZFreeIf(obj_template[j].pValue);
	    }
	}
   }
   return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssCKObject_GetAttributeItem
(
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_TYPE attribute,
  NSSArena *arenaOpt,
  nssSession *session,
  NSSSlot *slot,
  NSSItem *rvItem
)
{
    CK_ATTRIBUTE attr = { 0, NULL, 0 };
    PRStatus nssrv;
    attr.type = attribute;
    nssrv = nssCKObject_GetAttributes(object, &attr, 1, 
                                      arenaOpt, session, slot);
    if (nssrv != PR_SUCCESS) {
	return nssrv;
    }
    rvItem->data = (void *)attr.pValue;
    rvItem->size = (PRUint32)attr.ulValueLen;
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRBool
nssCKObject_IsAttributeTrue
(
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_TYPE attribute,
  nssSession *session,
  NSSSlot *slot,
  PRStatus *rvStatus
)
{
    CK_BBOOL bool;
    CK_ATTRIBUTE attr = { 0, NULL, 0 };
    CK_RV ckrv;
    attr.type = attribute;
    NSS_CK_SET_ATTRIBUTE_VAR(&attr, 0, bool);
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(slot)->C_GetAttributeValue(session->handle, object, &attr, 1);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	*rvStatus = PR_FAILURE;
	return PR_FALSE;
    }
    *rvStatus = PR_SUCCESS;
    return (PRBool)(bool == CK_TRUE);
}

NSS_IMPLEMENT PRStatus 
nssCKObject_SetAttributes
(
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG count,
  nssSession *session,
  NSSSlot  *slot
)
{
    CK_RV ckrv;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(slot)->C_SetAttributeValue(session->handle, object, 
                                            obj_template, count);
    nssSession_ExitMonitor(session);
    if (ckrv == CKR_OK) {
	return PR_SUCCESS;
    } else {
	return PR_FAILURE;
    }
}

