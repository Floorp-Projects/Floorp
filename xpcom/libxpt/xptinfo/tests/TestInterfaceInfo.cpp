/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Some simple smoke tests of the typelib loader. */

#include "nscore.h"

#include "nsISupports.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"

static void RegAllocator();

int main (int argc, char **argv) {
    RegAllocator();

    nsIInterfaceInfoManager *iim = XPT_GetInterfaceInfoManager();
    nsIID *iid;
    iim->GetIIDForName("Interface", &iid);
}    


// XXX remove following code when allocator autoregisters.
#include "nsRepository.h"
#include "nsIAllocator.h"

static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);

#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#define XPCOM_DLL  "XPCOM_DLL"
#else
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif

static void RegAllocator()
{
    nsRepository::RegisterComponent(kAllocatorCID, NULL, NULL, XPCOM_DLL, 
                                    PR_FALSE, PR_FALSE);
}


