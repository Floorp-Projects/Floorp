/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsJARURI_h__
#define nsJARURI_h__

#include "nsIJARURI.h"
#include "nsISerializable.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsJARURI : public nsIJARURI, nsISerializable
{
public:    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIJARURI
    NS_DECL_NSISERIALIZABLE

    // nsJARURI
    nsJARURI();
    virtual ~nsJARURI();
   
    nsresult Init(const char *charsetHint);
    nsresult FormatSpec(const nsACString &entryPath, nsACString &result);

protected:
    nsCOMPtr<nsIURI> mJARFile;
    nsCString        mJAREntry;
    nsCString        mCharsetHint;
};

#endif // nsJARURI_h__
