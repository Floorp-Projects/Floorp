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
/*
 * Internal header file included only by files in pkcs11 dir, or in
 * pkcs11 specific client and server files.
 */
#ifndef _SECMODI_H_
#define _SECMODI_H_ 1
#include "pkcs11.h"
#include "nssilock.h"
#include "mcom_db.h"
#include "secoidt.h"
#include "secdert.h"
#include "certt.h"
#include "secmodti.h"

#ifdef PKCS11_USE_THREADS
#define PK11_USE_THREADS(x) x
#else
#define PK11_USE_THREADS(x)
#endif

SEC_BEGIN_PROTOS

/* proto-types */
SECMODModule * SECMOD_NewModule(void); /* create a new module */
SECMODModule * SECMOD_NewInternal(void); /* create an internal module */

/* Data base functions */
void SECMOD_InitDB(char *);
SECMODModuleList * SECMOD_ReadPermDB(void);

/*void SECMOD_ReferenceModule(SECMODModule *); */

/* Library functions */
SECStatus SECMOD_LoadModule(SECMODModule *);
SECStatus SECMOD_UnloadModule(SECMODModule *);
void SECMOD_SetInternalModule(SECMODModule *);

void SECMOD_SlotDestroyModule(SECMODModule *module, PRBool fromSlot);
CK_RV pk11_notify(CK_SESSION_HANDLE session, CK_NOTIFICATION event,
                                                         CK_VOID_PTR pdata);
void pk11_SignedToUnsigned(CK_ATTRIBUTE *attrib);
CK_OBJECT_HANDLE pk11_FindObjectByTemplate(PK11SlotInfo *slot,
					CK_ATTRIBUTE *inTemplate,int tsize);
SEC_END_PROTOS

#define PK11_GETTAB(x) ((CK_FUNCTION_LIST_PTR)((x)->functionList))
#define PK11_SETATTRS(x,id,v,l) (x)->type = (id); \
		(x)->pValue=(v); (x)->ulValueLen = (l);
#endif

