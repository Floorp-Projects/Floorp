/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
