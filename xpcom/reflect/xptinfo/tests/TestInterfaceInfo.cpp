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
#include "xptinfo.h"

#include <stdio.h>

#include "../src/nsInterfaceInfo.h"

static void RegAllocator();

int main (int argc, char **argv) {
    RegAllocator();

    nsIInterfaceInfoManager *iim = XPTI_GetInterfaceInfoManager();
    nsIID *iid1, *iid2, *iid3, *iid4;
    char *name1, *name2, *name3, *name4;
    nsIInterfaceInfo *info1, *info2, *info3, *info4;

    fprintf(stderr, "\ngetting iid for 'Interface'\n");
    iim->GetIIDForName("Interface", &iid1);
    iim->GetNameForIID(iid1, &name1);
    fprintf(stderr, "%s iid %s\n", name1, iid1->ToString());

    fprintf(stderr, "\ngetting iid for 'nsIBaseStream'\n");
    iim->GetIIDForName("nsIBaseStream", &iid2);
    iim->GetNameForIID(iid2, &name2);
    fprintf(stderr, "%s iid %s\n", name2, iid2->ToString());

    fprintf(stderr, "iid: %s, name: %s\n", iid1->ToString(), name1);
    fprintf(stderr, "iid: %s, name: %s\n", iid2->ToString(), name2);

    fprintf(stderr, "\ngetting info for iid2 from above\n");
    iim->GetInfoForIID(iid2, &info2);
#ifdef DEBUG
    ((nsInterfaceInfo *)info2)->print(stderr);
#endif

    fprintf(stderr, "\ngetting iid for 'nsIInputStream'\n");
    iim->GetIIDForName("nsIInputStream", &iid3);
    iim->GetNameForIID(iid3, &name3);
    fprintf(stderr, "%s iid %s\n", name3, iid2->ToString());
    iim->GetInfoForIID(iid3, &info3);
#ifdef DEBUG
    ((nsInterfaceInfo *)info3)->print(stderr);
#endif

    fprintf(stderr, "\ngetting info for name 'nsIBidirectionalEnumerator'\n");
    iim->GetInfoForName("nsIBidirectionalEnumerator", &info4);
#ifdef DEBUG
    ((nsInterfaceInfo *)info4)->print(stderr);
#endif

    return 0;
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
