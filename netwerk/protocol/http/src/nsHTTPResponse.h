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
 * Original Author: Gagan Saksena <gagan@netscape.com>
 *
 * Contributor(s): 
 */

#ifndef _nsHTTPResponse_h_
#define _nsHTTPResponse_h_

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"
#include "nsHTTPHeaderArray.h"
#include "nsString.h"
#include "prtime.h"

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
    // Constructor
    nsHTTPResponse();

    // Methods from nsISupports
    NS_DECL_ISUPPORTS

    // Finally our own methods...

    nsresult            GetContentType(char* *o_ContentType);
    nsresult            SetContentType(const char* i_ContentType);
    nsresult            GetCharset(char* *o_Charset);
    nsresult            SetCharset(const char* i_Charset);
    nsresult            GetContentLength(PRInt32* o_Value);
    nsresult            SetContentLength(PRInt32 i_Value);
    nsresult            GetStatus(PRUint32* o_Value);
    nsresult            GetStatusString(char* *o_String);
    nsresult            GetServer(char* *o_String);
    nsresult            SetServerVersion(const char* i_ServerVersion);
    nsresult            GetServerVersion(HTTPVersion *aResult);

    nsresult            GetHeader(nsIAtom* i_Header, char* *o_Value);
    nsresult            SetHeader(nsIAtom* i_Header, const char* o_Value);
    nsresult            GetHeaderEnumerator(nsISimpleEnumerator** aResult);

    nsresult            SetStatus(PRInt32 i_Value) { mStatus = i_Value; return NS_OK;};
    nsresult            SetStatusString(const char* i_Value);

    nsresult            ParseStatusLine(nsCString& aStatusLine);
    nsresult            ParseHeader(nsCString& aHeaderString);
    nsresult            ParseHeaders(nsCString& aAllHeaders);
    nsresult            ProcessHeader(nsIAtom* aHeader, nsCString& aValue);
    nsresult            EmitHeaders(nsCString& aResult);
     
    PRBool              IsStale(PRBool aUseHeuristicExpiration);

    nsresult            UpdateHeaders(nsISimpleEnumerator *aEnumerator);        
 
    nsresult            ParseDateHeader(nsIAtom *aAtom, PRTime *aResultTime, PRBool *aHeaderIsPresent);

protected:
    virtual ~nsHTTPResponse();
    nsresult            ParseDateHeader(nsIAtom *aAtom, PRUint32 *aResultTime, PRBool *aHeaderIsPresent);
    nsresult            GetMaxAge(PRUint32* aMaxAge, PRBool* aMaxAgeIsPresent);

    HTTPVersion         mServerVersion;
    nsCString           mStatusString;
    nsCString           mContentType;
    nsCString           mCharset;
    PRUint32            mStatus;
    PRInt32             mContentLength;
    nsHTTPHeaderArray   mHeaders;
};

#endif /* _nsHTTPResponse_h_ */
