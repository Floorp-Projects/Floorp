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

#include "nsHTTPEncodeStream.h"

////////////////////////////////////////////////////////////////////////////////
// nsHTTPEncodeStream methods:

nsHTTPEncodeStream::nsHTTPEncodeStream()
    : mInput(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult
nsHTTPEncodeStream::Init(nsIInputStream* in)
{
    mInput = in;
    NS_ADDREF(mInput);
    return NS_OK;
}

nsHTTPEncodeStream::~nsHTTPEncodeStream()
{
    NS_IF_RELEASE(mInput);
}

NS_IMPL_ISUPPORTS(nsHTTPEncodeStream, nsIInputStream::GetIID());

NS_METHOD
nsHTTPEncodeStream::Create(nsIInputStream *rawStream, nsIInputStream **result)
{
    nsHTTPEncodeStream* str = new nsHTTPEncodeStream();
    if (str == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = str->Init(rawStream);
    if (NS_FAILED(rv)) {
        NS_RELEASE(str);
        return rv;
    }
    *result = str;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIBaseStream methods:

NS_IMETHODIMP
nsHTTPEncodeStream::Close(void)
{
    return mInput->Close();
}

////////////////////////////////////////////////////////////////////////////////
// nsIInputStream methods:

NS_IMETHODIMP
nsHTTPEncodeStream::GetLength(PRUint32 *result)
{
    // XXX Ugh! This requires buffering up the translation so that you can
    // count it, because to walk it consumes the input.
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPEncodeStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
#define BUF_SIZE        1024
    char buffer[BUF_SIZE];
    nsresult rv;

    PRUint32 amt;
    rv = mInput->Read(buffer, BUF_SIZE, &amt);
    if (NS_FAILED(rv)) return rv;
    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        return rv;

    

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
