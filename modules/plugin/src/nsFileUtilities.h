/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
