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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsISupports.h"
#include "nsIPrefBranch.h"
#include "nsWeakReference.h"

class nsPrefBranch : public nsIPrefBranch,
                     public nsSupportsWeakReference
{
public:
    nsPrefBranch(const char *aPrefRoot);
    virtual ~nsPrefBranch();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPREFBRANCH
    
private:
    nsCString& getPrefRoot();
    const char *getPrefName(const char *aPrefName);

    static nsresult convertUTF8ToUnicode(const char *utf8String,
                                  PRUnichar **result);

    static nsresult setFileSpecPrefInternal(const char *real_pref,
                                            nsIFileSpec* value,
                                            PRBool set_default);

    static nsresult getFileSpecPrefInternal(const char *real_pref,
                                            nsIFileSpec** result,
                                            PRBool get_default);
    
    nsCString mPrefRoot;
    PRInt32 mPrefRootLength;
    
};

