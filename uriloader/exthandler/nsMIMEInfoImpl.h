/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef __nsmimeinfoimpl_h___
#define __nsmimeinfoimpl_h___

#include "nsIMIMEInfo.h"
#include "nsIAtom.h"
#include "nsString2.h"

class nsMIMEInfoImpl : public nsIMIMEInfo {

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsMIMEInfoImpl methods
    nsMIMEInfoImpl(const char *aMIMEType, const char *aFileExtensions, const char *aDescription);
    virtual ~nsMIMEInfoImpl();

    PRBool InExtensions(nsIAtom* anIAtom);

    // nsIMIMEInfo methods
    NS_DECL_NSIMIMEINFO

    // member variables
    nsIAtom*            mMIMEType;
    nsString2           mFileExtensions;
    nsString2           mDescription;
};

#endif //__nsmimeinfoimpl_h___
