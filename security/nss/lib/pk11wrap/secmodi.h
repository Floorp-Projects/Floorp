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
#include "secmodt.h"
#include "secmodti.h"

#ifdef PKCS11_USE_THREADS
#define PK11_USE_THREADS(x) x
#else
#define PK11_USE_THREADS(x)
#endif

SEC_BEGIN_PROTOS

/* proto-types */
extern SECStatus SECMOD_DeletePermDB(SECMODModule *module);
extern SECStatus SECMOD_AddPermDB(SECMODModule *module);

extern int secmod_PrivateModuleCount;

extern void SECMOD_Init(void);

/* list managment */
extern SECStatus SECMOD_AddModuleToList(SECMODModule *newModule);
extern SECStatus SECMOD_AddModuleToDBOnlyList(SECMODModule *newModule);
extern SECStatus SECMOD_AddModuleToUnloadList(SECMODModule *newModule);
extern void SECMOD_RemoveList(SECMODModuleList **,SECMODModuleList *);
extern void SECMOD_AddList(SECMODModuleList *,SECMODModuleList *,SECMODListLock *);

/* Operate on modules by name */
extern SECMODModule *SECMOD_FindModuleByID(SECMODModuleID);

/* database/memory management */
extern SECMODModuleList *SECMOD_NewModuleListElement(void);
extern SECMODModuleList *SECMOD_DestroyModuleListElement(SECMODModuleList *);
extern void SECMOD_DestroyModuleList(SECMODModuleList *);
extern SECStatus SECMOD_AddModule(SECMODModule *newModule);
SECStatus SECMOD_DeleteModuleEx(const char * name, SECMODModule *mod, int *type, PRBool permdb);

extern unsigned long SECMOD_PubCipherFlagstoInternal(unsigned long publicFlags);
extern unsigned long SECMOD_InternaltoPubCipherFlags(unsigned long internalFlags);

/* Library functions */
SECStatus SECMOD_LoadPKCS11Module(SECMODModule *);
SECStatus SECMOD_UnloadModule(SECMODModule *);
void SECMOD_SetInternalModule(SECMODModule *);

void SECMOD_SlotDestroyModule(SECMODModule *module, PRBool fromSlot);
CK_RV pk11_notify(CK_SESSION_HANDLE session, CK_NOTIFICATION event,
                                                         CK_VOID_PTR pdata);
void pk11_SignedToUnsigned(CK_ATTRIBUTE *attrib);
CK_OBJECT_HANDLE pk11_FindObjectByTemplate(PK11SlotInfo *slot,
					CK_ATTRIBUTE *inTemplate,int tsize);
CK_OBJECT_HANDLE *pk11_FindObjectsByTemplate(PK11SlotInfo *slot,
			CK_ATTRIBUTE *inTemplate,int tsize, int *objCount);
SECStatus PK11_UpdateSlotAttribute(PK11SlotInfo *slot,
				 PK11DefaultArrayEntry *entry, PRBool add);

#define PK11_GETTAB(x) ((CK_FUNCTION_LIST_PTR)((x)->functionList))
#define PK11_SETATTRS(x,id,v,l) (x)->type = (id); \
		(x)->pValue=(v); (x)->ulValueLen = (l);
SECStatus PK11_CreateNewObject(PK11SlotInfo *slot, CK_SESSION_HANDLE session,
                               CK_ATTRIBUTE *theTemplate, int count,
                                PRBool token, CK_OBJECT_HANDLE *objectID);

SECStatus pbe_PK11AlgidToParam(SECAlgorithmID *algid,SECItem *mech);
SECStatus PBE_PK11ParamToAlgid(SECOidTag algTag, SECItem *param, 
				PRArenaPool *arena, SECAlgorithmID *algId);

extern void pk11sdr_Init(void);
extern void pk11sdr_Shutdown(void);

PRBool pk11_LoginStillRequired(PK11SlotInfo *slot, void *wincx);

SEC_END_PROTOS

#endif

