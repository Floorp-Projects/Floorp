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
 *
 * Definition of Security Module Data Structure. There is a separate data
 * structure for each loaded PKCS #11 module.
 */
#ifndef _SECMOD_H_
#define _SEDMOD_H_
#include "seccomon.h"
#include "secmodt.h"

#define PKCS11_USE_THREADS

/* These mechanisms flags are visible to all other libraries. */
/* They must be converted to internal SECMOD_*_FLAG */
/* if used inside the functions of the security library */
#define PUBLIC_MECH_RSA_FLAG         0x00000001ul
#define PUBLIC_MECH_DSA_FLAG         0x00000002ul
#define PUBLIC_MECH_RC2_FLAG         0x00000004ul
#define PUBLIC_MECH_RC4_FLAG         0x00000008ul
#define PUBLIC_MECH_DES_FLAG         0x00000010ul
#define PUBLIC_MECH_DH_FLAG          0x00000020ul
#define PUBLIC_MECH_FORTEZZA_FLAG    0x00000040ul
#define PUBLIC_MECH_RC5_FLAG         0x00000080ul
#define PUBLIC_MECH_SHA1_FLAG        0x00000100ul
#define PUBLIC_MECH_MD5_FLAG         0x00000200ul
#define PUBLIC_MECH_MD2_FLAG         0x00000400ul
#define PUBLIC_MECH_SSL_FLAG         0x00000800ul
#define PUBLIC_MECH_TLS_FLAG         0x00001000ul

#define PUBLIC_MECH_RANDOM_FLAG      0x08000000ul
#define PUBLIC_MECH_FRIENDLY_FLAG    0x10000000ul
#define PUBLIC_OWN_PW_DEFAULTS       0X20000000ul
#define PUBLIC_DISABLE_FLAG          0x40000000ul

/* warning: reserved means reserved */
#define PUBLIC_MECH_RESERVED_FLAGS   0x87FFE000ul

/* These cipher flags are visible to all other libraries, */
/* But they must be converted before used in functions */
/* withing the security module */
#define PUBLIC_CIPHER_FORTEZZA_FLAG  0x00000001ul

/* warning: reserved means reserved */
#define PUBLIC_CIPHER_RESERVED_FLAGS 0xFFFFFFFEul

SEC_BEGIN_PROTOS

/* protoypes */
extern void SECMOD_init(char *dbname);
extern void SECMOD_Shutdown(void);
extern SECMODModuleList *SECMOD_GetDefaultModuleList(void);
extern SECMODListLock *SECMOD_GetDefaultModuleListLock(void);

/* lock management */
extern SECMODListLock *SECMOD_NewListLock(void);
extern void SECMOD_DestroyListLock(SECMODListLock *);
extern void SECMOD_GetReadLock(SECMODListLock *);
extern void SECMOD_ReleaseReadLock(SECMODListLock *);
extern void SECMOD_GetWriteLock(SECMODListLock *);
extern void SECMOD_ReleaseWriteLock(SECMODListLock *);

/* list managment */
extern void SECMOD_RemoveList(SECMODModuleList **,SECMODModuleList *);
extern void SECMOD_AddList(SECMODModuleList *,SECMODModuleList *,SECMODListLock *);

/* Operate on modules by name */
extern SECMODModule *SECMOD_FindModule(char *name);
extern SECMODModule *SECMOD_FindModuleByID(SECMODModuleID);
extern SECStatus SECMOD_DeleteModule(char *name, int *type);
extern SECStatus SECMOD_DeleteInternalModule(char *name);
extern SECStatus SECMOD_AddNewModule(char* moduleName, char* dllPath,
                              unsigned long defaultMechanismFlags,
                              unsigned long cipherEnableFlags);
/* database/memory management */
extern SECMODModule *SECMOD_NewModule(void);
extern SECMODModuleList *SECMOD_NewModuleListElement(void);
extern SECMODModule *SECMOD_GetInternalModule(void);
extern SECMODModule *SECMOD_GetFIPSInternal(void);
extern SECMODModule *SECMOD_ReferenceModule(SECMODModule *module);
extern void SECMOD_DestroyModule(SECMODModule *module);
extern SECMODModuleList *SECMOD_DestroyModuleListElement(SECMODModuleList *);
extern void SECMOD_DestroyModuleList(SECMODModuleList *);
extern SECMODModule *SECMOD_DupModule(SECMODModule *old);
extern SECStatus SECMOD_AddModule(SECMODModule *newModule);
extern PK11SlotInfo *SECMOD_FindSlot(SECMODModule *module,char *name);
extern PK11SlotInfo *SECMOD_LookupSlot(SECMODModuleID module,
							unsigned long slotID);
SECStatus  SECMOD_DeletePermDB(SECMODModule *);
SECStatus  SECMOD_AddPermDB(SECMODModule *);

/* Funtion reports true if at least one of the modules */
/* of modType has been installed */
PRBool SECMOD_IsModulePresent( unsigned long int pubCipherEnableFlags );

/* Functions used to convert between internal & public representation
 * of Mechanism Flags and Cipher Enable Flags */
extern unsigned long SECMOD_PubMechFlagstoInternal(unsigned long publicFlags);
extern unsigned long SECMOD_InternaltoPubMechFlags(unsigned long internalFlags);

extern unsigned long SECMOD_PubCipherFlagstoInternal(unsigned long publicFlags);
extern unsigned long SECMOD_InternaltoPubCipherFlags(unsigned long internalFlags);

SEC_END_PROTOS

#endif
