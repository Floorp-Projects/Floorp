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
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsAutoLock.h"

class nsBufferInputStream;

////////////////////////////////////////////////////////////////////////////////

class nsBufferInputStream : public nsIBufferInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_DECL_NSIBASESTREAM

    // nsIInputStream methods:
    NS_DECL_NSIINPUTSTREAM

    // nsIBufferInputStream methods:
    NS_IMETHOD GetBuffer(nsIBuffer* *result);
    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void* closure, PRUint32 count,
                            PRUint32 *readCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD Search(const char *forString, PRBool ignoreCase, PRBool *found, PRUint32 *offsetSearchedTo);
    NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking);
    NS_IMETHOD SetNonBlocking(PRBool aNonBlocking);
#if 0
    NS_IMETHOD Fill(const char *buf, PRUint32 count, PRUint32 *_retval);
    NS_IMETHOD FillFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval);
#endif

    // nsBufferInputStream methods:
    nsBufferInputStream(nsIBuffer* buf, PRBool blocking);
    virtual ~nsBufferInputStream();

    nsresult Fill();

protected:
    nsIBuffer*          mBuffer;
    PRBool              mBlocking;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferOutputStream : public nsIBufferOutputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_DECL_NSIBASESTREAM

    // nsIOutputStream methods:
    NS_DECL_NSIOUTPUTSTREAM

    // nsIBufferOutputStream methods:
    NS_IMETHOD GetBuffer(nsIBuffer * *aBuffer);
    NS_IMETHOD WriteSegments(nsReadSegmentFun reader, void* closure, PRUint32 count,
                             PRUint32 *writeCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    } 
    NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 aCount,
                         PRUint32 *aWriteCount);
    NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking);
    NS_IMETHOD SetNonBlocking(PRBool aNonBlocking);

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
    NS_RELEASE(mBuffer);
}

NS_IMPL_ISUPPORTS3(nsBufferInputStream, nsIBufferInputStream, nsIInputStream, nsIBaseStream)

NS_IMETHODIMP
nsBufferInputStream::Close(void)
{
    nsresult rv;
    nsAutoCMonitor mon(mBuffer);

#ifdef DEBUG
    PRBool closed;
    rv = mBuffer->GetReaderClosed(&closed);
    NS_ASSERTION(NS_SUCCEEDED(rv) && !closed, "state change error");
#endif

    rv = mBuffer->ReaderClosed();
    // even if ReaderClosed fails, be sure to do the notify:
    nsresult rv2 = mon.Notify();   // wake up the writer
    if (NS_FAILED(rv2)) 
        return rv2;
    return rv;
}

NS_IMETHODIMP
nsBufferInputStream::GetLength(PRUint32 *aLength)
{
    nsAutoCMonitor mon(mBuffer);
#ifdef DEBUG
    nsresult rv;
    PRBool closed;
    rv = mBuffer->GetReaderClosed(&closed);
    NS_ASSERTION(NS_SUCCEEDED(rv) && !closed, "state change error");
#endif

    return mBuffer->GetReadableAmount(aLength);
}

NS_IMETHODIMP
nsBufferInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
    nsresult rv = NS_OK;
    nsAutoCMonitor mon(mBuffer);

#ifdef DEBUG
    PRBool closed;
    rv = mBuffer->GetReaderClosed(&closed);
    NS_ASSERTION(NS_SUCCEEDED(rv) && !closed, "state change error");
#endif

    *aReadCount = 0;
    while (aCount > 0) {
        PRUint32 amt;
        
        amt = 0;
        rv = mBuffer->Read(aBuf, aCount, &amt);
        if (rv == NS_BASE_STREAM_EOF) {
            rv = (*aReadCount == 0) ? rv : NS_OK;
            break;
        }
//        if (rv == NS_BASE_STREAM_WOULD_BLOCK) break;
        // If a blocking read fails just drop into Fill(.)
        if (!mBlocking && NS_FAILED(rv)) break;

        if (amt == 0) {
            if (*aReadCount > 0) {
                break;  // don't Fill if we've got something
            }
            rv = Fill();
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) break;
            if (NS_FAILED(rv)) break;
        }
        else {
            *aReadCount += amt;
            aBuf += amt;
            aCount -= amt;
        }
    }
    if (mBlocking && rv == NS_BASE_STREAM_EOF) {
        // all we're ever going to get -- so wake up anyone in Flush
#ifdef DEBUG
        PRUint32 amt;
        const char* buf;
        nsresult rv2 = mBuffer->GetReadSegment(0, &buf, &amt);
        NS_ASSERTION(rv2 == NS_BASE_STREAM_EOF ||
                     (NS_SUCCEEDED(rv2) && amt == 0), "Read failed");
#endif
        mon.Notify();   // wake up writer
    }
    return (*aReadCount == 0) ? rv : NS_OK;
}

NS_IMETHODIMP
nsBufferInputStream::GetBuffer(nsIBuffer* *result)
{
    nsAutoCMonitor mon(mBuffer);
    *result = mBuffer;
    NS_ADDREF(mBuffer);
    return NS_OK;
}

NS_IMETHODIMP
nsBufferInputStream::Search(const char *forString, PRBool ignoreCase, PRBool *found, PRUint32 *offsetSearchedTo)
{
    nsAutoCMonitor mon(mBuffer);
#ifdef DEBUG
    nsresult rv;
    PRBool closed;
    rv = mBuffer->GetReaderClosed(&closed);
    NS_ASSERTION(NS_SUCCEEDED(rv) && !closed, "state change error");
#endif

    return mBuffer->Search(forString, ignoreCase, found, offsetSearchedTo);
}

NS_IMETHODIMP
nsBufferInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = !mBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferInputStream::SetNonBlocking(PRBool aNonBlocking)
{
    mBlocking = !aNonBlocking;
    return NS_OK;
}

nsresult
nsBufferInputStream::Fill()
{
    nsAutoCMonitor mon(mBuffer);
    nsresult rv;
    while (PR_TRUE) {
        // check read buffer again while in the monitor
        PRUint32 amt;
        const char* buf;
        rv = mBuffer->GetReadSegment(0, &buf, &amt);
        if (NS_FAILED(rv) || amt > 0) return rv;

        // else notify the writer and wait
        rv = mon.Notify();
        if (NS_FAILED(rv)) return rv;   // interrupted
        if (mBlocking) {
            rv = mon.Wait();
            if (NS_FAILED(rv)) return rv;   // interrupted
        }
        else {
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        // loop again so that we end up exiting on EOF with
        // the right error
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewBufferInputStream(nsIBufferInputStream* *result,
                        nsIBuffer* buffer, PRBool blocking)
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
    NS_RELEASE(mBuffer);
}

NS_IMPL_ADDREF(nsBufferOutputStream);
NS_IMPL_RELEASE(nsBufferOutputStream);

NS_IMETHODIMP
nsBufferOutputStream::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIBufferOutputStream::GetIID()) ||
        aIID.Equals(nsIOutputStream::GetIID()) ||
        aIID.Equals(nsIBaseStream::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsBufferOutputStream::Close(void)
{
    nsAutoCMonitor mon(mBuffer);
    nsresult rv;
    rv = mBuffer->SetCondition(NS_BASE_STREAM_EOF);
    // even if SetCondition fails, be sure to do the notify:
    nsresult rv2 = mon.Notify();   // wake up the writer
    if (NS_FAILED(rv2))
        return rv2;
    return rv;
}

NS_IMETHODIMP
nsBufferOutputStream::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
{
    nsAutoCMonitor mon(mBuffer);
    nsresult rv = NS_OK;

#ifdef DEBUG
    nsresult condition;
    rv = mBuffer->GetCondition(&condition);
    NS_ASSERTION(NS_SUCCEEDED(rv) && condition != NS_BASE_STREAM_EOF, "state change error");
#endif

    *aWriteCount = 0;
    while (aCount > 0) {
        PRUint32 amt;
        amt = 0;
        rv = mBuffer->Write(aBuf, aCount, &amt);
        NS_ASSERTION(rv != NS_BASE_STREAM_EOF, "Write should not return EOF");
//        if (rv == NS_BASE_STREAM_WOULD_BLOCK) break;
        // If a blocking write fails just drop into Flush(...)
        if (!mBlocking && NS_FAILED(rv)) break;

        if (amt == 0) {
            rv = Flush();
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) break;
            if (NS_FAILED(rv)) break;
        }
        else {
            aBuf += amt;
            aCount -= amt;
            *aWriteCount += amt;
        }
    }
    if (mBlocking && rv == NS_BASE_STREAM_WOULD_BLOCK) {
        // all we're going to get for now -- so wake up anyone in Flush
#ifdef DEBUG
        PRUint32 amt;
        const char* buf;
        nsresult rv2 = mBuffer->GetReadSegment(0, &buf, &amt);
        NS_ASSERTION(rv2 == NS_BASE_STREAM_EOF ||
                     (NS_SUCCEEDED(rv2) && amt > 0), "Write failed");
#endif
        mon.Notify();   // wake up reader
    }
    return (*aWriteCount == 0) ? rv : NS_OK;
}

NS_IMETHODIMP
nsBufferOutputStream::WriteFrom(nsIInputStream* fromStream, PRUint32 aCount,
                                PRUint32 *aWriteCount)
{
    nsAutoCMonitor mon(mBuffer);
    nsresult rv = NS_OK;

#ifdef xDEBUG
    nsresult condition;
    rv = mBuffer->GetCondition(&condition);
    NS_ASSERTION(NS_SUCCEEDED(rv) && condition != NS_BASE_STREAM_EOF, "state change error");
#endif

    *aWriteCount = 0;
    while (aCount > 0) {
        PRUint32 amt;
        amt = 0;
        rv = mBuffer->WriteFrom(fromStream, aCount, &amt);
//        if (rv == NS_BASE_STREAM_WOULD_BLOCK) break;
        // If a blocking write fails just drop into Flush(...)
        if (/*!mBlocking && */NS_FAILED(rv))
            break;

        if (amt == 0) {
            rv = Flush();
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) break;
            if (NS_FAILED(rv)) break;
        }
        else {
            aCount -= amt;
            *aWriteCount += amt;
        }
    }
    if (mBlocking && rv == NS_BASE_STREAM_WOULD_BLOCK) {
        // all we're going to get for now -- so wake up anyone in Flush
#ifdef DEBUG
        PRUint32 amt;
        const char* buf;
        nsresult rv2 = mBuffer->GetReadSegment(0, &buf, &amt);
        NS_ASSERTION(rv2 == NS_BASE_STREAM_EOF ||
                     (NS_SUCCEEDED(rv2) && amt > 0), "WriteFrom failed");
#endif
        mon.Notify();   // wake up reader
    }
    return (*aWriteCount == 0) ? rv : NS_OK;
}

NS_IMETHODIMP
nsBufferOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = !mBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    mBlocking = !aNonBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferOutputStream::Flush(void)
{
    nsAutoCMonitor mon(mBuffer);
    nsresult rv = NS_OK;
    PRBool firstTime = PR_TRUE;
    while (PR_TRUE) {
        // check write buffer again while in the monitor
        PRUint32 amt;
        const char* buf;
        rv = mBuffer->GetReadSegment(0, &buf, &amt);
        if (firstTime && amt == 0) {
            // If we think we needed to flush, yet there's nothing
            // in the buffer to read, we must have not been able to 
            // allocate any segments. Return out of memory:
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (NS_FAILED(rv) || amt == 0) return rv;
        firstTime = PR_FALSE;

        // else notify the reader and wait
        rv = mon.Notify();
        if (NS_FAILED(rv)) return rv;   // interrupted
        if (mBlocking) {
            rv = mon.Wait();
            if (NS_FAILED(rv)) return rv;   // interrupted
        }
        else {
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBufferOutputStream::GetBuffer(nsIBuffer * *aBuffer)
{
    nsAutoCMonitor mon(mBuffer);
    *aBuffer = mBuffer;
    NS_ADDREF(mBuffer);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewBufferOutputStream(nsIBufferOutputStream* *result,
                         nsIBuffer* buffer, PRBool blocking)
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
NS_NewPipe(nsIBufferInputStream* *inStrResult,
           nsIBufferOutputStream* *outStrResult,
           PRUint32 growBySize, PRUint32 maxSize,
           PRBool blocking, nsIBufferObserver* observer)
{
    nsresult rv;
    nsIBufferInputStream* inStr = nsnull;
    nsIBufferOutputStream* outStr = nsnull;
    nsIBuffer* buf = nsnull;

#ifdef XP_MAC
    // Don't use page buffers on the mac because we don't really have
    // VM there, and they end up being more wasteful:
    rv = NS_NewBuffer(&buf, growBySize, maxSize, observer);
#else
    rv = NS_NewPageBuffer(&buf, growBySize, maxSize, observer);
#endif
    if (NS_FAILED(rv)) goto error;

    rv = NS_NewBufferInputStream(&inStr, buf, blocking);
    if (NS_FAILED(rv)) goto error;
    
    rv = NS_NewBufferOutputStream(&outStr, buf, blocking);
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

