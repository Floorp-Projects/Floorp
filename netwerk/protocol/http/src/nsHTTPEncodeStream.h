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

#ifndef nsHTTPEncodeStream_h__
#define nsHTTPEncodeStream_h__

#include "nsIInputStream.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsHTTPEncodeStream : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_DECL_NSIBASESTREAM

    // nsIInputStream methods:
    NS_DECL_NSIINPUTSTREAM
    
    // nsHTTPEncodeStream methods:
    nsHTTPEncodeStream();
    virtual ~nsHTTPEncodeStream();

    static NS_METHOD
    Create(nsIInputStream *rawStream, PRUint32 flags, 
           nsIInputStream **result);

    nsresult Init(nsIInputStream *rawStream, PRUint32 flags);
    nsresult GetData(char* buf, PRUint32 bufLen, PRUint32 *readCount);
    nsresult PushBack(nsString& data);

protected:
    nsCOMPtr<nsIInputStream>    mInput;
    PRUint32                    mFlags;
    nsString                    mPushBackBuffer;
    PRBool                      mLastLineComplete;
};

#endif // nsHTTPEncodeStream_h__
