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

// Convienence bits of nsXPTParamInfo that don't fit into xpt_cpp.h
// flyweight wrappers.

#include "nsISupports.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"

#include "nsInterfaceInfoManager.h"

#include "xpt_cpp.h"
#include "xptinfo.h"

// Placeholder - this implementation just returns NULL.

nsIInterfaceInfo*
nsXPTParamInfo::GetInterface() const
{
    NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE,"not an interface");

    nsIInterfaceInfoManager* mgr;
    if(!(mgr = nsInterfaceInfoManager::GetInterfaceInfoManager()))
        return NULL;

//    nsIInterfaceInfo* info;
//      mgr->GetInfoForIID(&InterfaceDirectoryEntryTable[type.type.interface].iid,
//                         &info);
    NS_RELEASE(mgr);
//    return info;
    return NULL;
}

const nsIID*
nsXPTParamInfo::GetInterfaceIID() const
{
    NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE,"not an interface");
//      return &InterfaceDirectoryEntryTable[type.type.interface].iid;
    return (const nsIID*) NULL;
}
