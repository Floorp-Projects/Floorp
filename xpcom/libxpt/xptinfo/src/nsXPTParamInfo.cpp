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

/*
 * Convienence bits of nsXPTParamInfo that don't fit into xpt_cpp.h
 * flyweight wrappers.
 */

#include "nsISupports.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"

#include "nsInterfaceInfoManager.h"

#include "xpt_cpp.h"
#include "xptinfo.h"

nsIInterfaceInfo*
nsXPTParamInfo::GetInterface(nsIInterfaceInfo *info) const
{
    NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE,
                    "not an interface");

    nsInterfaceRecord *record =
        ((nsInterfaceInfo *)info)->getInterfaceRecord();
    
    nsIInterfaceInfoManager* mgr;
    if(!(mgr = nsInterfaceInfoManager::GetInterfaceInfoManager()))
        return NULL;

    // what typelib did the entry come from?
    XPTHeader *which_header = record->which_header;
    NS_ASSERTION(which_header != NULL, "missing header info assoc'd with II");

    // can't use IID, because it could be null for this entry.
    char *interface_name;

    // offset is 1-based, so subtract 1 to use in interface_directory.
    interface_name =
        which_header->interface_directory[type.type.interface - 1].name;

    nsIInterfaceInfo *ii;
    nsresult nsr = mgr->GetInfoForName(interface_name, &ii);
    if (NS_IS_ERROR(nsr)) {
        NS_RELEASE(mgr);
        return NULL;
    }
    return ii;
}

const nsIID*
nsXPTParamInfo::GetInterfaceIID(nsIInterfaceInfo *info) const
{
    NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE,
                    "not an interface");

    nsInterfaceRecord *record =
        ((nsInterfaceInfo *)info)->getInterfaceRecord();

    nsIInterfaceInfoManager* mgr;
    if(!(mgr = nsInterfaceInfoManager::GetInterfaceInfoManager()))
        return NULL;

    // what typelib did the entry come from?
    XPTHeader *which_header = record->which_header;
    NS_ASSERTION(which_header != NULL, "header missing?");

    // can't use IID, because it could be null for this entry.
    char *interface_name;

    // offset is 1-based, so subtract 1 to use in interface_directory.
    interface_name =
        which_header->interface_directory[type.type.interface - 1].name;

    nsIID* iid;
    nsresult nsr = mgr->GetIIDForName(interface_name, &iid);
    if (NS_IS_ERROR(nsr)) {
        NS_RELEASE(mgr);
        return NULL;
    }
    return iid;
}







