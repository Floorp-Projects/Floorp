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

/* Library-private header for nsInterfaceInfo implementation. */

#ifndef nsInterfaceInfo_h___
#define nsInterfaceInfo_h___

#include "nsIInterfaceInfo.h"
#include "xpt_struct.h"

#include "nsInterfaceRecord.h"
#include "nsInterfaceInfoManager.h"

#ifdef DEBUG
#include <stdio.h>
#endif

class nsInterfaceInfo : public nsIInterfaceInfo
{
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetName(char** name); // returns IAllocatator alloc'd copy
    NS_IMETHOD GetIID(nsIID** iid);  // returns IAllocatator alloc'd copy

    NS_IMETHOD IsScriptable(PRBool* result);

    NS_IMETHOD GetParent(nsIInterfaceInfo** parent);

    // these include counts of parents
    NS_IMETHOD GetMethodCount(uint16* count);
    NS_IMETHOD GetConstantCount(uint16* count);

    // These include methods and constants of parents.
    // There do *not* make copies ***explicit bending of XPCOM rules***
    NS_IMETHOD GetMethodInfo(uint16 index, const nsXPTMethodInfo** info);
    NS_IMETHOD GetMethodInfoForName(const char* methodName, uint16 *index,
                                    const nsXPTMethodInfo** info);
    NS_IMETHOD GetConstant(uint16 index, const nsXPTConstant** constant);

    // Get the interface information or iid associated with a param of some
    // method in this interface.
    NS_IMETHOD GetInfoForParam(const nsXPTParamInfo* param, 
                               nsIInterfaceInfo** info);
    // returns IAllocatator alloc'd copy
    NS_IMETHOD GetIIDForParam(const nsXPTParamInfo* param, nsIID** iid);

public:
    virtual ~nsInterfaceInfo();

#ifdef DEBUG
    void NS_EXPORT print(FILE *fd);
#endif

private:
    friend class nsInterfaceRecord;

    nsInterfaceInfo(nsInterfaceRecord *record, nsInterfaceInfo *parent);
    nsInterfaceRecord *mInterfaceRecord;
    nsInterfaceInfo* mParent;

    uint16 mMethodBaseIndex;
    uint16 mMethodCount;
    uint16 mConstantBaseIndex;
    uint16 mConstantCount;
};

#endif /* nsInterfaceInfo_h___ */
