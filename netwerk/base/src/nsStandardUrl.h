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

#ifndef nsStandardUrl_h__
#define nsStandardUrl_h__

#include "nsIURL.h"
#include "nsAgg.h"

#define NS_THIS_STANDARDURL_IMPLEMENTATION_CID       \
{ /* e3939dc8-29ab-11d3-8cce-0060b0fc14a3 */         \
    0xe3939dc8,                                      \
    0x29ab,                                          \
    0x11d3,                                          \
    {0x8c, 0xce, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

class nsStandardURL : public nsIURL
{
public:
    NS_DECL_AGGREGATED

    ////////////////////////////////////////////////////////////////////////////
    // nsIURI methods:

    /* attribute string Spec; */
    NS_IMETHOD GetSpec(char * *aSpec);
    NS_IMETHOD SetSpec(char * aSpec);

    /* attribute string Scheme; */
    NS_IMETHOD GetScheme(char * *aScheme);
    NS_IMETHOD SetScheme(char * aScheme);

    /* attribute string PreHost; */
    NS_IMETHOD GetPreHost(char * *aPreHost);
    NS_IMETHOD SetPreHost(char * aPreHost);

    /* attribute string Host; */
    NS_IMETHOD GetHost(char * *aHost);
    NS_IMETHOD SetHost(char * aHost);

    /* attribute long Port; */
    NS_IMETHOD GetPort(PRInt32 *aPort);
    NS_IMETHOD SetPort(PRInt32 aPort);

    /* attribute string Path; */
    NS_IMETHOD GetPath(char * *aPath);
    NS_IMETHOD SetPath(char * aPath);

    /* boolean Equals (in nsIURI other); */
    NS_IMETHOD Equals(nsIURI *other, PRBool *_retval);

    /* nsIURI Clone (); */
    NS_IMETHOD Clone(nsIURI **_retval);

    ////////////////////////////////////////////////////////////////////////////
    // nsIURL methods:

    /* attribute string Directory; */
    NS_IMETHOD GetDirectory(char * *aDirectory);
    NS_IMETHOD SetDirectory(char * aDirectory);

    /* attribute string FileName; */
    NS_IMETHOD GetFileName(char * *aFileName);
    NS_IMETHOD SetFileName(char * aFileName);

    /* attribute string Query; */
    NS_IMETHOD GetQuery(char * *aQuery);
    NS_IMETHOD SetQuery(char * aQuery);

    /* attribute string Ref; */
    NS_IMETHOD GetRef(char * *aRef);
    NS_IMETHOD SetRef(char * aRef);

    ////////////////////////////////////////////////////////////////////////////
    // nsStandardURL methods:

    nsStandardURL(nsISupports* outer);
    virtual ~nsStandardURL();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsresult Parse(const char* spec, nsIURI* aBaseUrl);
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

#endif // nsStandardUrl_h__
