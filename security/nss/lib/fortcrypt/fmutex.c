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
#include "fmutex.h"
#include "fpkmem.h"
#include <stdio.h>

typedef struct PKMutexFunctions {
  CK_CREATEMUTEX  CreateMutex;
  CK_DESTROYMUTEX DestroyMutex;
  CK_LOCKMUTEX    LockMutex;
  CK_UNLOCKMUTEX  UnlockMutex;
  int  useMutex;
} PKMutexFunctions;

static PKMutexFunctions   gMutex = {NULL, NULL, NULL, NULL, 0};
static int gInit = 0;

#define FMUTEX_CHECKS()                            \
    if (gInit == 0) {                              \
        return CKR_GENERAL_ERROR;                  \
    }                                              \
    if (!gMutex.useMutex) {                        \
        return CKR_GENERAL_ERROR;                  \
    }			 

CK_RV FMUTEX_Init(CK_C_INITIALIZE_ARGS_PTR pArgs) {
    if (gInit != 0) {
        return CKR_GENERAL_ERROR;
    }

    if (pArgs && pArgs->CreateMutex && pArgs->DestroyMutex &&
	pArgs->LockMutex   && pArgs->UnlockMutex) {

        gMutex.CreateMutex    = pArgs->CreateMutex;
	gMutex.DestroyMutex   = pArgs->DestroyMutex;
	gMutex.LockMutex      = pArgs->LockMutex;
	gMutex.UnlockMutex    = pArgs->UnlockMutex;
	gMutex.useMutex       = 1;
	gInit                 = 1;
    } else {
        gInit = 0;
	return CKR_GENERAL_ERROR;
    }
    
    return CKR_OK;
}


CK_RV FMUTEX_Create(CK_VOID_PTR_PTR pMutex) {
    CK_RV rv;
    FMUTEX_CHECKS()

    rv = gMutex.CreateMutex(pMutex);
    return rv;
}

CK_RV FMUTEX_Destroy(CK_VOID_PTR pMutex) {
    CK_RV rv;
    FMUTEX_CHECKS()

    rv = gMutex.DestroyMutex(pMutex);
    return rv;
}

CK_RV FMUTEX_Lock(CK_VOID_PTR pMutex) {
    CK_RV rv;
    FMUTEX_CHECKS()

    rv = gMutex.LockMutex(pMutex);
    return rv;
}

CK_RV FMUTEX_Unlock(CK_VOID_PTR pMutex) {
    CK_RV rv;
    FMUTEX_CHECKS()

    rv = gMutex.UnlockMutex(pMutex);
    return rv;
}

int FMUTEX_MutexEnabled (void) {
    return gMutex.useMutex;
}
