/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsInterfaceInfoManager_h___
#define nsInterfaceInfoManager_h___

#include "nsIAllocator.h"

#include "xpt_struct.h"

#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"

#include "nsInterfaceInfo.h"

class nsInterfaceInfoManager : public nsIInterfaceInfoManager
{
    NS_DECL_ISUPPORTS;

    // nsIInformationInfo management services
    NS_IMETHOD GetInfoForIID(const nsIID* iid, nsIInterfaceInfo** info);
    NS_IMETHOD GetInfoForName(const char* name, nsIInterfaceInfo** info);

    // name <-> IID mapping services
    NS_IMETHOD GetIIDForName(const char* name, nsIID** iid);
    NS_IMETHOD GetNameForIID(const nsIID* iid, char** name);

    // should be private ?
    nsInterfaceInfoManager();
public:
    virtual ~nsInterfaceInfoManager();
    static nsInterfaceInfoManager* GetInterfaceInfoManager();
    static nsIAllocator* GetAllocator(nsInterfaceInfoManager* iim = NULL);

private:
    // temporary hack
    nsInterfaceInfo **mInfoArray;
    nsInterfaceInfo *buildII(XPTInterfaceDirectoryEntry *entry);

    XPTHeader *mHeader;
    nsInterfaceInfo *mParent;
    nsIAllocator* mAllocator;
};

#endif /* nsInterfaceInfoManager_h___ */
