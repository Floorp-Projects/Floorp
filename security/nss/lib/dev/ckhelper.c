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
static const char CVS_ID[] = "@(#) $RCSfile: ckhelper.c,v $ $Revision: 1.1 $ $Date: 2001/09/13 22:06:07 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

NSS_IMPLEMENT_DATA const CK_BBOOL 
g_ck_true = CK_TRUE;

NSS_IMPLEMENT_DATA const CK_BBOOL 
g_ck_false = CK_FALSE;

NSS_IMPLEMENT_DATA const CK_OBJECT_CLASS 
g_ck_obj_class_cert = CKO_CERTIFICATE;

NSS_IMPLEMENT_DATA const CK_OBJECT_CLASS 
g_ck_obj_class_pubkey = CKO_PUBLIC_KEY;

NSS_IMPLEMENT_DATA const CK_OBJECT_CLASS 
g_ck_obj_class_privkey = CKO_PRIVATE_KEY;

NSS_IMPLEMENT PRStatus 
NSSCKObject_GetAttributes
(
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG count,
  NSSArena *arenaOpt,
  NSSSlot  *slot
)
{
   nssArenaMark *mark;
   CK_SESSION_HANDLE session;
   CK_ULONG i;
   CK_RV ckrv;
   PRStatus nssrv;
   /* use the default session */
   session = slot->token->session.handle; 
#ifdef arena_mark_bug_fixed
   if (arenaOpt) {
	mark = nssArenaMark(arenaOpt);
	if (!mark) {
	    goto loser;
	}
   }
#endif
   /* Get the storage size needed for each attribute */
   ckrv = CKAPI(slot)->C_GetAttributeValue(session,
                                           object, obj_template, count);
   if (ckrv != CKR_OK) {
	/* set an error here */
	goto loser;
   }
   /* Allocate memory for each attribute. */
   for (i=0; i<count; i++) {
	obj_template[i].pValue = nss_ZAlloc(arenaOpt, 
	                                    obj_template[i].ulValueLen);
	if (!obj_template[i].pValue) {
	    goto loser;
	}
   }
   /* Obtain the actual attribute values. */
   ckrv = CKAPI(slot)->C_GetAttributeValue(session,
                                           object, obj_template, count);
   if (ckrv != CKR_OK) {
	/* set an error here */
	goto loser;
   }
#ifdef arena_mark_bug_fixed
   if (arenaOpt) {
	nssrv = nssArena_Unmark(arenaOpt, mark);
	if (nssrv != PR_SUCCESS) {
	    goto loser;
	}
   }
#endif
   return PR_SUCCESS;
loser:
   if (arenaOpt) {
	/* release all arena memory allocated before the failure. */
#ifdef arena_mark_bug_fixed
	(void)nssArena_Release(arenaOpt, mark);
#endif
   } else {
	CK_ULONG j;
	/* free each heap object that was allocated before the failure. */
	for (j=0; j<i; j++) {
	    nss_ZFreeIf(obj_template[j].pValue);
	}
   }
   return PR_FAILURE;
}
