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
 * The Original Code is Network Security Services.
 *
 * The Initial Developer of the Original Code is
 * Red Hat Inc.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Robert Relyea <rrelyea@redhat.com>
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
#define PORT_ArenaZAlloc  PORT_ArenaZAlloc_stub
#define PORT_Free PORT_Free_stub
#define PORT_FreeArena  PORT_FreeArena_stub
#define PORT_GetError  PORT_GetError_stub
#define PORT_NewArena  PORT_NewArena_stub
#define PORT_SetError  PORT_SetError_stub
#define PORT_ZAlloc PORT_ZAlloc_stub
#define PORT_ZFree  PORT_ZFree_stub

#define SECITEM_AllocItem  SECITEM_AllocItem_stub
#define SECITEM_CompareItem  SECITEM_CompareItem_stub
#define SECITEM_CopyItem  SECITEM_CopyItem_stub
#define SECITEM_FreeItem  SECITEM_FreeItem_stub
#define SECITEM_ZfreeItem  SECITEM_ZfreeItem_stub
#define NSS_SecureMemcmp NSS_SecureMemcmp_stub

#define PR_Assert  PR_Assert_stub
#define PR_CallOnce  PR_CallOnce_stub
#define PR_Close  PR_Close_stub
#define PR_DestroyCondVar PR_DestroyCondVar_stub
#define PR_DestroyLock  PR_DestroyLock_stub
#define PR_Free  PR_Free_stub
#define PR_GetLibraryFilePathname  PR_GetLibraryFilePathname_stub
#define PR_ImportPipe  PR_ImportPipe_stub
#define PR_Lock  PR_Lock_stub
#define PR_NewCondVar PR_NewCondVar_stub
#define PR_NewLock  PR_NewLock_stub
#define PR_NotifyCondVar PR_NotifyCondVar_stub
#define PR_NotifyAllCondVar PR_NotifyAllCondVar_stub
#define PR_Open  PR_Open_stub
#define PR_Read  PR_Read_stub
#define PR_Seek  PR_Seek_stub
#define PR_Sleep  PR_Sleep_stub
#define PR_Unlock  PR_Unlock_stub
#define PR_WaitCondVar PR_WaitCondVar_stub

extern int  FREEBL_InitStubs(void);

#endif
