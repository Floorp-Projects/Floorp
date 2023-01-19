/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Allow freebl and softoken to be loaded without util or NSPR.
 *
 * These symbols are overridden once real NSPR, and libutil are attached.
 */

#ifndef _STUBS_H
#define _STUBS_H_ 1

#ifdef _LIBUTIL_H_
/* must be included before util */
/*#error stubs.h included too late */
#define MP_DIGITES(x) "stubs included too late"
#endif

/* hide libutil rename */
#define _LIBUTIL_H_ 1

#define PORT_Alloc PORT_Alloc_stub
#define PORT_ArenaAlloc PORT_ArenaAlloc_stub
#define PORT_ArenaZAlloc PORT_ArenaZAlloc_stub
#define PORT_Free PORT_Free_stub
#define PORT_FreeArena PORT_FreeArena_stub
#define PORT_GetError PORT_GetError_stub
#define PORT_NewArena PORT_NewArena_stub
#define PORT_SetError PORT_SetError_stub
#define PORT_ZAlloc PORT_ZAlloc_stub
#define PORT_ZFree PORT_ZFree_stub
#define PORT_ZAllocAligned PORT_ZAllocAligned_stub
#define PORT_ZAllocAlignedOffset PORT_ZAllocAlignedOffset_stub

#define SECITEM_AllocItem SECITEM_AllocItem_stub
#define SECITEM_CompareItem SECITEM_CompareItem_stub
#define SECITEM_ItemsAreEqual SECITEM_ItemsAreEqual_stub
#define SECITEM_CopyItem SECITEM_CopyItem_stub
#define SECITEM_FreeItem SECITEM_FreeItem_stub
#define SECITEM_ZfreeItem SECITEM_ZfreeItem_stub
#define SECOID_FindOIDTag SECOID_FindOIDTag_stub
#define NSS_SecureMemcmp NSS_SecureMemcmp_stub
#define NSS_SecureMemcmpZero NSS_SecureMemcmpZero_stub

#define PR_Assert PR_Assert_stub
#define PR_Access PR_Access_stub
#define PR_CallOnce PR_CallOnce_stub
#define PR_Close PR_Close_stub
#define PR_DestroyCondVar PR_DestroyCondVar_stub
#define PR_DestroyLock PR_DestroyLock_stub
#define PR_Free PR_Free_stub
#define PR_GetLibraryFilePathname PR_GetLibraryFilePathname_stub
#define PR_ImportPipe PR_ImportPipe_stub
#define PR_Lock PR_Lock_stub
#define PR_NewCondVar PR_NewCondVar_stub
#define PR_NewLock PR_NewLock_stub
#define PR_NotifyCondVar PR_NotifyCondVar_stub
#define PR_NotifyAllCondVar PR_NotifyAllCondVar_stub
#define PR_Open PR_Open_stub
#define PR_Read PR_Read_stub
#define PR_Seek PR_Seek_stub
#define PR_Sleep PR_Sleep_stub
#define PR_Unlock PR_Unlock_stub
#define PR_WaitCondVar PR_WaitCondVar_stub
#define PR_GetEnvSecure PR_GetEnvSecure_stub

extern int FREEBL_InitStubs(void);

#endif
