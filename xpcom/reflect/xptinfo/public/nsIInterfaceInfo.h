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

/* The nsIInterfaceInfo xpcom public declaration. */

#ifndef nsIInterfaceInfo_h___
#define nsIInterfaceInfo_h___

#include "nsISupports.h"

// forward declaration of non-XPCOM types
class nsXPTMethodInfo;
class nsXPTConstant;
class nsXPTParamInfo;

// {215DBE04-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IINTERFACEINFO_IID   \
{ 0x215dbe04, 0x94a7, 0x11d2,   \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIInterfaceInfo : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IINTERFACEINFO_IID)

    NS_IMETHOD GetName(char** name) = 0; // returns IAllocatator alloc'd copy
    NS_IMETHOD GetIID(nsIID** iid) = 0;  // returns IAllocatator alloc'd copy

    NS_IMETHOD GetParent(nsIInterfaceInfo** parent) = 0;

    // these include counts of parents
    NS_IMETHOD GetMethodCount(uint16* count) = 0;
    NS_IMETHOD GetConstantCount(uint16* count) = 0;

    // These include methods and constants of parents.
    // There do *not* make copies ***explicit bending of XPCOM rules***
    NS_IMETHOD GetMethodInfo(uint16 index, const nsXPTMethodInfo** info) = 0;
    NS_IMETHOD GetMethodInfoForName(const char* methodName, uint16 *index,
                                    const nsXPTMethodInfo** info) = 0;
    NS_IMETHOD GetConstant(uint16 index, const nsXPTConstant** constant) = 0;

    // Get the interface information or iid associated with a param of some
    // method in this interface.
    NS_IMETHOD GetInfoForParam(const nsXPTParamInfo* param, 
                               nsIInterfaceInfo** info) = 0;
    // returns IAllocatator alloc'd copy
    NS_IMETHOD GetIIDForParam(const nsXPTParamInfo* param, nsIID** iid) = 0;
};

#endif /* nsIInterfaceInfo_h___ */
