/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsFileUtilities_h__
#define nsFileUtilities_h__

#include "nsIFileUtilities.h"
#include "nsAgg.h"

////////////////////////////////////////////////////////////////////////////////

class nsFileUtilities : public nsIFileUtilities {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIFileUtilities:
    
    NS_IMETHOD
    GetProgramPath(const char* *result);

    NS_IMETHOD
    GetTempDirPath(const char* *result);

    NS_IMETHOD
    NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf);

    ////////////////////////////////////////////////////////////////////////////
    // nsFileUtilities specific methods:

    nsFileUtilities(nsISupports* outer);
    virtual ~nsFileUtilities(void);

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
    void SetProgramPath(const char* path) { fProgramPath = path; }

protected:
    const char*         fProgramPath;

};

#endif // nsFileUtilities_h__
