/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
    NS_DECL_NSIINTERFACEINFO
    
public:
    virtual ~nsInterfaceInfo();

#ifdef DEBUG
    void NS_EXPORT print(FILE *fd);
#endif

private:
    NS_IMETHOD GetTypeInArray(const nsXPTParamInfo* param, 
                              uint16 dimension,
                              const XPTTypeDescriptor** type);
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
