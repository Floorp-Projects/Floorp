/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsUrl_h__
#define nsUrl_h__

#include "nsIUrl.h"
#include "nsAgg.h"

// XXX regenerate:
#define NS_TYPICALURL_CID                            \
{ /* 8ffae6d0-ee37-11d2-9322-000000000000 */         \
    0x8ffae6d0,                                      \
    0xee37,                                          \
    0x11d2,                                          \
    {0x93, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} \
}

class nsUrl : public nsIUrl
{
public:
    NS_DECL_AGGREGATED

    ////////////////////////////////////////////////////////////////////////////
    // nsIUrl methods:

    NS_IMETHOD GetScheme(const char* *result);
    NS_IMETHOD SetScheme(const char* scheme);

    NS_IMETHOD GetPreHost(const char* *result);
    NS_IMETHOD SetPreHost(const char* preHost);

    NS_IMETHOD GetHost(const char* *result);
    NS_IMETHOD SetHost(const char* host);

    NS_IMETHOD GetPort(PRInt32 *result);
    NS_IMETHOD SetPort(PRInt32 port);

    NS_IMETHOD GetPath(const char* *result);
    NS_IMETHOD SetPath(const char* path);

    NS_IMETHOD Equals(nsIUrl* other);
    NS_IMETHOD Clone(nsIUrl* *result);

    NS_IMETHOD ToNewCString(const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // nsUrl methods:

    nsUrl(nsISupports* outer);
    virtual ~nsUrl();

    nsresult Init(const char* aSpec,
                  nsIUrl* aBaseURL);

protected:
    nsresult Parse(const char* spec, nsIUrl* aBaseUrl);
    void ReconstructSpec();

protected:
    char*       mScheme;
    char*       mPreHost;
    char*       mHost;
    PRInt32     mPort;
    char*       mPath;
    char*       mRef;
    char*       mQuery;
    char*       mSpec;  // XXX go away
};

#endif // nsUrl_h__
