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

/* For keeping track of interface directory entries. */

#ifndef nsInterfaceRecord_h___
#define nsInterfaceRecord_h___

#include "nsInterfaceInfo.h"
#include "xpt_struct.h"

// resolve circular ref...
class nsInterfaceInfo;
class nsTypelibRecord;

// For references in the IIDTable, nameTable hashtables in the
// nsInterfaceInfoManager.
class nsInterfaceRecord {
public:
    // Accessors for these?

    nsID iid;
    char *name;
    char *name_space;

    nsTypelibRecord *typelibRecord;
    XPTInterfaceDescriptor *interfaceDescriptor;

    // adds ref
    nsresult GetInfo(nsInterfaceInfo **result);

    // makes allocator alloc'd copy.
    nsresult GetIID(nsIID **iid);
    nsresult GetName(char **name);

    // so that nsInterfaceInfo destructor can null it out.
    nsInterfaceInfo *info;
};    

#endif /* nsInterfaceRecord_h___ */
