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

#include "nsIBuffer.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsAutoLock.h"

class nsBufferInputStream;

////////////////////////////////////////////////////////////////////////////////

class nsBufferInputStream : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_IMETHOD Close(void);

    // nsIInputStream methods:
    NS_IMETHOD GetLength(PRUint32 *aLength);
    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount); 

    // nsBufferInputStream methods:
    nsBufferInputStream(nsIBuffer* buf, PRBool blocking);
    virtual ~nsBufferInputStream();

    PRUint32 ReadableAmount() {
        nsresult rv;
        PRUint32 amt;
        char* buf;
        rv = mBuffer->GetReadBuffer(&amt, &buf); // should never fail
        NS_ASSERTION(NS_SUCCEEDED(rv), "GetInputBuffer failed");
        return amt;
    }

    nsresult SetEOF();
    nsresult Fill();

protected:
    nsIBuffer*          mBuffer;
    PRBool              mBlocking;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferOutputStream : public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_IMETHOD Close(void);

    // nsIOutputStream methods:
    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount); 
    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount);
    NS_IMETHOD Flush(void);

    // nsBufferOutputStream methods:
    nsBufferOutputStream(nsIBuffer* buf, PRBool blocking);
    virtual ~nsBufferOutputStream();

protected:
    nsIBuffer*  mBuffer;
    PRBool      mBlocking;
};

////////////////////////////////////////////////////////////////////////////////
// nsBufferInputStream methods:
////////////////////////////////////////////////////////////////////////////////

nsBufferInputStream::nsBufferInputStream(nsIBuffer* buf, PRBool blocking)
    : mBuffer(buf), mBlocking(blocking)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mBuffer);
}

nsBufferInputStream::~nsBufferInputStream()
{
    (void)Close();
}

NS_IMPL_ADDREF(nsBufferInputStream);
NS_IMPL_RELEASE(nsBufferInputStream);

NS_IMETHODIMP
nsBufferInputStream::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIInputStream::GetIID()) ||
        aIID.Equals(nsIBaseStream::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aInstancePtr = this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsBufferInputStream::Close(void)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    if (mBlocking) {
        nsAutoMonitor mon(mBuffer);
        NS_RELEASE(mBuffer);
        mBuffer = nsnull;
        nsresult rv = mon.Notify();   // wake up the writer
        if (NS_FAILED(rv)) return rv;
    }
    else {
        NS_RELEASE(mBuffer);
        mBuffer = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBufferInputStream::GetLength(PRUint32 *aLength)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    char* buf;
    return mBuffer->GetReadBuffer(aLength, &buf);
}

NS_IMETHODIMP
nsBufferInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv = NS_OK;
    *aReadCount = 0;

    while (aCount > 0) {
        PRUint32 amt;
        rv = mBuffer->Read(aBuf, aCount, &amt);
        if (rv == NS_BASE_STREAM_EOF)
            return *aReadCount > 0 ? NS_OK : rv;
        if (NS_FAILED(rv)) return rv;
        if (amt == 0) {
            rv = Fill();
            if (NS_FAILED(rv)) return rv;
        }
        else {
            *aReadCount += amt;
            aBuf += amt;
            aCount -= amt;
        }
    }
    return rv;
}

nsresult
nsBufferInputStream::SetEOF()
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    if (mBlocking) {
        nsAutoMonitor mon(mBuffer);
        mBuffer->SetEOF();
        nsresult rv = mon.Notify();   // wake up the writer
        if (NS_FAILED(rv)) return rv;
    }
    else {
        mBuffer->SetEOF();
    }
    return NS_OK;
}

nsresult
nsBufferInputStream::Fill()
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    if (mBlocking) {
        nsAutoMonitor mon(mBuffer);
        while (PR_TRUE) {
            nsresult rv;

            // check read buffer again while in the monitor
            PRUint32 amt;
            char* buf;
            rv = mBuffer->GetReadBuffer(&amt, &buf);
            if (rv == NS_BASE_STREAM_EOF) return rv;
            if (NS_SUCCEEDED(rv) && amt > 0) return NS_OK;

            // else notify the writer and wait
            rv = mon.Notify();
            if (NS_FAILED(rv)) return rv;   // interrupted
            rv = mon.Wait();
            if (NS_FAILED(rv)) return rv;   // interrupted
        }
    }
    else {
        return NS_BASE_STREAM_WOULD_BLOCK;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewBufferInputStream(nsIInputStream* *result, nsIBuffer* buffer, PRBool blocking)
{
    nsBufferInputStream* str = new nsBufferInputStream(buffer, blocking);
    if (str == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(str);
    *result = str;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferOutputStream methods:
////////////////////////////////////////////////////////////////////////////////

nsBufferOutputStream::nsBufferOutputStream(nsIBuffer* buf, PRBool blocking)
    : mBuffer(buf), mBlocking(blocking)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mBuffer);
}

nsBufferOutputStream::~nsBufferOutputStream()
{
    (void)Close();
}

NS_IMPL_ADDREF(nsBufferOutputStream);
NS_IMPL_RELEASE(nsBufferOutputStream);

NS_IMETHODIMP
nsBufferOutputStream::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIOutputStream::GetIID()) ||
        aIID.Equals(nsIBaseStream::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aInstancePtr = this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsBufferOutputStream::Close(void)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    if (mBlocking) {
        nsAutoMonitor mon(mBuffer);
        mBuffer->SetEOF();
        NS_RELEASE(mBuffer);
        mBuffer = nsnull;
        nsresult rv = mon.Notify();   // wake up the writer
        if (NS_FAILED(rv)) return rv;
    }
    else {
        NS_RELEASE(mBuffer);
        mBuffer = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBufferOutputStream::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv = NS_OK;
    *aWriteCount = 0;

    while (aCount > 0) {
        PRUint32 amt;
        rv = mBuffer->Write(aBuf, aCount, &amt);
        if (rv == NS_BASE_STREAM_EOF)
            return *aWriteCount > 0 ? NS_OK : rv;
        if (NS_FAILED(rv)) return rv;
        if (amt == 0) {
            rv = Flush();
            if (NS_FAILED(rv)) return rv;
        }
        else {
            aBuf += amt;
            aCount -= amt;
            *aWriteCount += amt;
        }
    }
    return rv;
}

NS_IMETHODIMP
nsBufferOutputStream::Write(nsIInputStream* fromStream, PRUint32 *aWriteCount)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv = NS_OK;
    *aWriteCount = 0;

    while (PR_TRUE) {   // write until fromStream gets EOF
        PRUint32 amt;
        rv = mBuffer->Write(fromStream, &amt);
        if (rv == NS_BASE_STREAM_EOF)
            return *aWriteCount > 0 ? NS_OK : rv;
        if (NS_FAILED(rv)) return rv;
        if (amt == 0) {
            rv = Flush();
            if (NS_FAILED(rv)) return rv;
        }
        else {
            *aWriteCount += amt;
        }
    }
    return rv;
}

NS_IMETHODIMP
nsBufferOutputStream::Flush(void)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    if (mBlocking) {
        nsresult rv;
        nsAutoMonitor mon(mBuffer);

        // check write buffer again while in the monitor
        PRUint32 amt;
        char* buf;
        rv = mBuffer->GetWriteBuffer(&amt, &buf);
        if (rv == NS_BASE_STREAM_EOF) return rv;
        if (NS_SUCCEEDED(rv) && amt > 0) return NS_OK;

            // else notify the reader and wait
        rv = mon.Notify();
        if (NS_FAILED(rv)) return rv;   // interrupted
        rv = mon.Wait();
        if (NS_FAILED(rv)) return rv;   // interrupted
    }
    else {
        return NS_BASE_STREAM_WOULD_BLOCK;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewBufferOutputStream(nsIOutputStream* *result, nsIBuffer* buffer, PRBool blocking)
{
    nsBufferOutputStream* ostr = new nsBufferOutputStream(buffer, blocking);
    if (ostr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ostr);
    *result = ostr;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewPipe2(nsIInputStream* *inStrResult,
           nsIOutputStream* *outStrResult,
           PRUint32 growBySize, PRUint32 maxSize)
{
    nsresult rv;
    nsIInputStream* inStr = nsnull;
    nsIOutputStream* outStr = nsnull;
    nsIBuffer* buf = nsnull;
    
    rv = NS_NewPageBuffer(&buf, growBySize, maxSize);
    if (NS_FAILED(rv)) goto error;

    rv = NS_NewBufferInputStream(&inStr, buf, PR_TRUE);
    if (NS_FAILED(rv)) goto error;
    
    rv = NS_NewBufferOutputStream(&outStr, buf, PR_TRUE);
    if (NS_FAILED(rv)) goto error;
    
    NS_RELEASE(buf);
    *inStrResult = inStr;
    *outStrResult = outStr;
    return NS_OK;

  error:
    NS_IF_RELEASE(inStr);
    NS_IF_RELEASE(outStr);
    NS_IF_RELEASE(buf);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

