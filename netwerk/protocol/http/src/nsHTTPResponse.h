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

#ifndef _nsHTTPResponse_h_
#define _nsHTTPResponse_h_

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"
#include "nsHTTPHeaderArray.h"

class nsIInputStream;

/* 
    The nsHTTPResponse class is the response object created by the response
    listener as it reads in data from the input stream.

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/
class nsHTTPResponse : public nsISupports
{

public:
    // Constructor and destructor
    nsHTTPResponse(nsIInputStream* i_InputStream);
    virtual ~nsHTTPResponse();

    // Methods from nsISupports
    NS_DECL_ISUPPORTS

    // Finally our own methods...

    nsresult            GetContentLength(PRInt32* o_Value);
    nsresult            GetStatus(PRUint32* o_Value);
    nsresult            GetStatusString(char* *o_String);
    nsresult            GetServer(char* *o_String);
    nsresult            SetServerVersion(const char* i_ServerVersion);

    nsresult            GetInputStream(nsIInputStream* *o_Stream);

    nsresult            GetHeader(nsIAtom* i_Header, char* *o_Value);
    nsresult            SetHeader(nsIAtom* i_Header, const char* o_Value);
    nsresult            GetHeaderEnumerator(nsISimpleEnumerator** aResult);

protected:

    NS_IMETHOD          SetStatus(PRInt32 i_Value) { mStatus = i_Value; return NS_OK;};
    NS_IMETHOD          SetStatusString(const char* i_Value);

    HTTPVersion                 mServerVersion;
    char*                       mStatusString;
    PRUint32                    mStatus;
    nsIInputStream*             mInputStream;

    nsHTTPHeaderArray           mHeaders;

    friend class nsHTTPResponseListener;
};

#endif /* _nsHTTPResponse_h_ */
