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

#include "nsNetStream.h"
#include "net.h"
#include "mktrace.h"

#if defined(XP_PC)
#include <windows.h>  // Needed for Interlocked APIs defined in nsISupports.h
#endif /* XP_PC */

/* XXX: Declare NET_PollSockets(...) for the blocking stream hack... */
extern "C" {
    XP_Bool NET_PollSockets(void);
}


NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);
NS_DEFINE_IID(kIConnectionInfoIID, NS_ICONNECTIONINFO_IID);

#define BUFFER_BLOCK_SIZE   8192




nsConnectionInfo::nsConnectionInfo(nsIURL *aURL,
                                   nsNetlibStream *aStream, 
                                   nsIStreamListener *aNotify)
{
    NS_INIT_REFCNT();

    /*
     * Cache the thread which is making the network request.  Any cross-thread
     * marshalling events will be sent to its event queue...
     *
     * XXX:  Currently, this assumes that the nsConnectionInfo is *always* 
     *       created on the requesting thread...  The requesting thread
     *       should really be passed in via the public APIs...
     */
    mRequestingThread = PR_GetCurrentThread();

    mStatus    = nsConnectionActive;
    pURL       = aURL;
    pNetStream = aStream;
    pConsumer  = aNotify;

    NS_IF_ADDREF(pURL);
    NS_IF_ADDREF(pNetStream);
    NS_IF_ADDREF(pConsumer);

    NS_VERIFY_THREADSAFE_INTERFACE(pURL);
    NS_VERIFY_THREADSAFE_INTERFACE(pNetStream);
    NS_VERIFY_THREADSAFE_INTERFACE(pConsumer);
}


NS_IMPL_THREADSAFE_ISUPPORTS(nsConnectionInfo, kIConnectionInfoIID);


nsConnectionInfo::~nsConnectionInfo()
{
    TRACEMSG(("nsConnectionInfo is being destroyed...\n"));

    NS_IF_RELEASE(pURL);
    NS_IF_RELEASE(pNetStream);
    NS_IF_RELEASE(pConsumer);
}

NS_IMETHODIMP 
nsConnectionInfo::GetURL(nsIURL **aURL)
{
    *aURL = pURL;
    NS_IF_ADDREF(pURL);    

    return NS_OK;
}

NS_IMETHODIMP 
nsConnectionInfo::GetInputStream(nsIInputStream **aStream)
{
    *aStream = (nsIInputStream *)pNetStream;
    NS_IF_ADDREF(pNetStream);

    return NS_OK;
}

NS_IMETHODIMP 
nsConnectionInfo::GetOutputStream(nsIOutputStream **aStream)
{
    *aStream = (nsIOutputStream *)pNetStream;
    NS_IF_ADDREF(pNetStream);

    return NS_OK;
}

NS_IMETHODIMP 
nsConnectionInfo::GetConsumer(nsIStreamListener **aConsumer)
{
    *aConsumer = pConsumer;
    NS_IF_ADDREF(pConsumer);

    return NS_OK;
}

nsNetlibStream::nsNetlibStream(void)
{
    NS_INIT_REFCNT();

    m_Lock = PR_NewMonitor();
    m_bIsClosed = PR_FALSE;
}


NS_IMPL_THREADSAFE_ADDREF(nsNetlibStream)
NS_IMPL_THREADSAFE_RELEASE(nsNetlibStream)

nsresult nsNetlibStream::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
    static NS_DEFINE_IID(kIInputStreamIID,  NS_IINPUTSTREAM_IID);
    static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);
    if (aIID.Equals(kIInputStreamIID)) {
        *aInstancePtr = (void*) ((nsIInputStream*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIOutputStreamIID)) {
        *aInstancePtr = (void*) ((nsIOutputStream*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = ((nsISupports *)(nsIInputStream *)this);
        AddRef();
        return NS_OK;
    }

#if defined(NS_DEBUG) 
    /*
     * Check for the debug-only interface indicating thread-safety
     */
    static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
    if (aIID.Equals(kIsThreadsafeIID)) {
        return NS_OK;
    }
#endif /* NS_DEBUG */

    return NS_NOINTERFACE;
}



nsNetlibStream::~nsNetlibStream()
{
    if (m_Lock) {
        PR_DestroyMonitor(m_Lock);
        m_Lock = NULL;
    }
}


nsresult nsNetlibStream::Close()
{
    LockStream();
    m_bIsClosed = PR_TRUE;
    UnlockStream();

    return NS_OK;
}




nsBufferedStream::nsBufferedStream(void)
{
    m_BufferLength = BUFFER_BLOCK_SIZE;

    m_Buffer = new char[m_BufferLength];
    /* If the allocation failed, mark the stream as closed... */
    if (NULL == m_Buffer) {
        m_bIsClosed = PR_TRUE;
        m_BufferLength = 0;
    }

    m_DataLength = 0;
    m_ReadOffset = m_WriteOffset = 0;
}




nsBufferedStream::~nsBufferedStream()
{
    TRACEMSG(("nsBufferedStream is being destroyed...\n"));

    if (m_Buffer) {
        delete (m_Buffer);
        m_Buffer = NULL;
    }
}


nsresult nsBufferedStream::GetAvailableSpace(PRUint32 *aCount)
{
    nsresult err;
    PRUint32 size = 0;

    if (m_bIsClosed) {
        err = NS_BASE_STREAM_EOF;
    } else {
        err = NS_OK;

        LockStream();
        NS_ASSERTION(m_BufferLength >= m_WriteOffset, "unsigned madness");        
        size = m_BufferLength - m_WriteOffset;
        UnlockStream();
    }
    *aCount = size;
    return err;
}


nsresult nsBufferedStream::GetLength(PRUint32 *aLength)
{
    LockStream();
    *aLength = m_DataLength;
    UnlockStream();

    return NS_OK;
}


nsresult nsBufferedStream::Write(const char *aBuf, 
                                 PRUint32 aLen,
                                 PRUint32 *aWriteCount)
{
    PRUint32 bytesFree;
    nsresult  rv = NS_OK;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");
    NS_PRECONDITION((m_WriteOffset >= m_ReadOffset), "Read past the end of buffer.");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        rv = NS_BASE_STREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed) {
        rv = NS_BASE_STREAM_EOF;
        goto done;
    }

    if (!m_bIsClosed && aBuf) {
        /* Grow the buffer if necessary */
        NS_ASSERTION(m_BufferLength >= m_WriteOffset, "unsigned madness");
        bytesFree = m_BufferLength - m_WriteOffset;
        if (aLen > bytesFree) {
            char *newBuffer;

            m_BufferLength += (((aLen - bytesFree) / BUFFER_BLOCK_SIZE)+1) * BUFFER_BLOCK_SIZE;
            newBuffer = (char *)realloc(m_Buffer, m_BufferLength);
            /* If the allocation failed, close the stream and free the buffer... */
            if (NULL == newBuffer) {
                m_bIsClosed = PR_TRUE;
                free(m_Buffer);
                m_Buffer = NULL;
                m_BufferLength = 0;

                goto done;
            } else {
                m_Buffer = newBuffer;
            }
        }

        memcpy(&m_Buffer[m_WriteOffset], aBuf, aLen);
        m_WriteOffset += aLen;

        *aWriteCount = aLen;
        m_DataLength += aLen;
    }

done:
    UnlockStream();

    return rv;
}


nsresult nsBufferedStream::Read(char *aBuf, 
                                PRUint32 aCount,
                                PRUint32 *aReadCount)
{
    nsresult rv = NS_OK;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");
    NS_PRECONDITION((m_WriteOffset >= m_ReadOffset), "Read past the end of buffer.");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        rv = NS_BASE_STREAM_ILLEGAL_ARGS;
        *aReadCount = 0;
        goto done;
    } else if (m_bIsClosed && (0 == m_DataLength)) {
        rv = NS_BASE_STREAM_EOF;
        *aReadCount = 0;
        goto done;
    }

    if (m_Buffer && m_DataLength) {

        /* Do not read more data than there is available... */
        if (aCount > m_DataLength) {
            aCount = m_DataLength;
        }

        memcpy(aBuf, &m_Buffer[m_ReadOffset], aCount);
        m_ReadOffset += aCount;

        *aReadCount  = aCount;
        m_DataLength -= aCount;
    }
    else
        *aReadCount = 0;

done:
    UnlockStream();

    return rv;
}

nsAsyncStream::nsAsyncStream(PRUint32 buffer_size)
{
    m_BufferLength = buffer_size;

    m_Buffer = (char *)malloc(m_BufferLength);
    /* If the allocation failed, mark the stream as closed... */
    if (NULL == m_Buffer) {
        m_bIsClosed = PR_TRUE;
        m_BufferLength = 0;
    }

    m_DataLength = 0;
    m_ReadOffset = m_WriteOffset = 0;
}


nsAsyncStream::~nsAsyncStream()
{
    TRACEMSG(("nsAsyncStream is being destroyed...\n"));

    if (m_Buffer) {
        free(m_Buffer);
        m_Buffer = NULL;
    }
}


nsresult nsAsyncStream::GetAvailableSpace(PRUint32 *aCount)
{
    nsresult err;
    PRInt32 size = 0;

    if (m_bIsClosed) {
        err = NS_BASE_STREAM_EOF;
    } else {
        err = NS_OK;

        LockStream();
        size = m_BufferLength - m_DataLength;
        UnlockStream();
    }
    *aCount = size;
    return err;
}


nsresult nsAsyncStream::GetLength(PRUint32 *aLength)
{
    LockStream();
    *aLength = m_DataLength;
    UnlockStream();

    return NS_OK;
}


nsresult nsAsyncStream::Write(const char *aBuf, 
                              PRUint32 aLen,
                              PRUint32 *aWriteCount)
{
    PRUint32 bytesFree;
    nsresult rv = NS_OK;

    LockStream();
    
    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        rv = NS_BASE_STREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed) {
        rv = NS_BASE_STREAM_EOF;
        goto done;
    }

    if (!m_bIsClosed && aBuf) {

        /* Do not store more data than there is space for... */
        NS_ASSERTION(m_BufferLength >= m_DataLength, "unsigned madness");
        bytesFree = m_BufferLength - m_DataLength;
        if (aLen > bytesFree) {
            aLen = bytesFree;
        }

        /* Storing the data will cause m_WriteOffset to wrap */
        if (m_WriteOffset + aLen > m_BufferLength) {
            PRInt32 delta;

            /* Store the first chunk through the end of the buffer */
            delta = m_BufferLength - m_WriteOffset;
            memcpy(&m_Buffer[m_WriteOffset], aBuf, delta);

            /* Store the second chunk from the beginning of the buffer */
            m_WriteOffset = aLen-delta;
            memcpy(m_Buffer, &aBuf[delta], m_WriteOffset);
        } else {
            memcpy(&m_Buffer[m_WriteOffset], aBuf, aLen);
            m_WriteOffset += aLen;
        }

        *aWriteCount = aLen;
        m_DataLength += aLen;
    }

done:
    UnlockStream();

    return rv;
}


nsresult nsAsyncStream::Read(char *aBuf, 
                             PRUint32 aCount,
                             PRUint32 *aReadCount)
{
    nsresult  rv = NS_OK;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        rv = NS_BASE_STREAM_ILLEGAL_ARGS;
        *aReadCount = 0;
        goto done;
    } else if (m_bIsClosed && (0 == m_DataLength)) {
        rv = NS_BASE_STREAM_EOF;
        *aReadCount = 0;
        goto done;
    }

    if (m_Buffer && m_DataLength) {

        /* Do not read more data than there is available... */
        if (aCount > m_DataLength) {
            aCount = m_DataLength;
        }

        /* Reading the data will cause m_ReadOffset to wrap */
        if (m_ReadOffset + aCount > m_BufferLength) {
            PRInt32 delta;

            /* Read the first chunk through the end of the buffer */
            delta = m_BufferLength - m_ReadOffset;
            memcpy(aBuf, &m_Buffer[m_ReadOffset], delta);

            /* Read the second chunk from the beginning of the buffer */
            m_ReadOffset = aCount-delta;
            memcpy(&aBuf[delta], m_Buffer, m_ReadOffset);
        } else {
            memcpy(aBuf, &m_Buffer[m_ReadOffset], aCount);
            m_ReadOffset += aCount;
        }

        *aReadCount  = aCount;
        m_DataLength -= aCount;
    }
    else
        *aReadCount = 0;

done:
    UnlockStream();

    return rv;
}









nsBlockingStream::nsBlockingStream()
{
    m_BufferLength = BUFFER_BLOCK_SIZE;

    m_Buffer = (char *)malloc(m_BufferLength);
    /* If the allocation failed, mark the stream as closed... */
    if (NULL == m_Buffer) {
        m_bIsClosed = PR_TRUE;
        m_BufferLength = 0;
    }

    m_DataLength = 0;
    m_ReadOffset = m_WriteOffset = 0;
}


nsBlockingStream::~nsBlockingStream()
{
    TRACEMSG(("nsBlockingStream is being destroyed...\n"));

    if (m_Buffer) {
        free(m_Buffer);
        m_Buffer = NULL;
    }
}


nsresult nsBlockingStream::Close()
{
    LockStream();
    m_bIsClosed = PR_TRUE;

#if defined(NETLIB_THREAD)
    /* Wake up any waiting threads... */
    PR_Notify(m_Lock);
#endif /* NETLIB_THREAD */

    UnlockStream();

    return NS_OK;
}

nsresult nsBlockingStream::GetAvailableSpace(PRUint32 *aCount)
{
    nsresult err;
    PRInt32 size = 0;

    if (m_bIsClosed) {
        err = NS_BASE_STREAM_EOF;
    } else {
        err = NS_OK;

        LockStream();
        size = m_BufferLength - m_DataLength;
        UnlockStream();
    }
    *aCount = size;
    return err;
}


nsresult nsBlockingStream::GetLength(PRUint32 *aLength)
{
    LockStream();
    *aLength = m_DataLength;
    UnlockStream();

    return NS_OK;
}


nsresult nsBlockingStream::Write(const char *aBuf, 
                                 PRUint32 aLen,
                                 PRUint32 *aWriteCount)
{
    PRUint32 bytesFree;
    nsresult rv = NS_OK;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");
    
    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        rv = NS_BASE_STREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed) {
        rv = NS_BASE_STREAM_EOF;
        goto done;
    }

    if (!m_bIsClosed && aBuf) {

        /* Do not store more data than there is space for... */
        NS_ASSERTION(m_BufferLength >= m_DataLength, "unsigned madness");
        bytesFree = m_BufferLength - m_DataLength;
        if (aLen > bytesFree) {
            aLen = bytesFree;
        }

        /* Storing the data will cause m_WriteOffset to wrap */
        if (m_WriteOffset + aLen > m_BufferLength) {
            PRUint32 delta;

            /* Store the first chunk through the end of the buffer */
            NS_ASSERTION(m_BufferLength >= m_WriteOffset, "unsigned madness");
            delta = m_BufferLength - m_WriteOffset;
            memcpy(&m_Buffer[m_WriteOffset], aBuf, delta);

            /* Store the second chunk from the beginning of the buffer */
            m_WriteOffset = aLen-delta;
            memcpy(m_Buffer, &aBuf[delta], m_WriteOffset);
        } else {
            memcpy(&m_Buffer[m_WriteOffset], aBuf, aLen);
            m_WriteOffset += aLen;
        }

        *aWriteCount = aLen;
        m_DataLength += aLen;

#if defined(NETLIB_THREAD)
        /* Wake up any waiting threads... */
        PR_Notify(m_Lock);
#endif /* NETLIB_THREAD */
    }

done:
    UnlockStream();

    return rv;
}


nsresult nsBlockingStream::Read(char *aBuf, 
                                PRUint32 aCount,
                                PRUint32 *aReadCount)
{
    nsresult rv = NS_OK;

    LockStream();

    *aReadCount = 0;

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        rv = NS_BASE_STREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed && (0 == m_DataLength)) {
        rv = NS_BASE_STREAM_EOF;
        goto done;
    }

    if (m_Buffer) {

        /* 
         * There is either enough data, or some data left in the stream
         * after it has been closed...  So, read it and return.
         */
        if ((aCount <= m_DataLength) || m_bIsClosed) {
            *aReadCount = ReadBuffer(aBuf, aCount);
        }
        /* Not enough data is available... Must block. */
        else {
#if !defined(NETLIB_THREAD)
            UnlockStream();
            do {
                NET_PollSockets();
                *aReadCount += ReadBuffer(aBuf + *aReadCount, aCount - *aReadCount);
                /* XXX m_bIsClosed is checked outside of the lock! */
            } while ((aCount > *aReadCount) && !m_bIsClosed); 
            LockStream();
#else
            do {
                PR_Wait(m_Lock, PR_INTERVAL_NO_TIMEOUT);
                *aReadCount += ReadBuffer(aBuf + *aReadCount, aCount - *aReadCount);
            } while ((aCount > *aReadCount) && !m_bIsClosed); 
#endif /* NETLIB_THREAD */
            /* 
             * It is possible that the stream was closed during 
             * NET_PollSockets(...)...  In this case, return EOF if no data 
             * is available...
             */
            if ((0 == *aReadCount) && m_bIsClosed) {
                rv = NS_BASE_STREAM_EOF;
            }
        } 
    }
    
done:
    UnlockStream();

    return rv;
}


PRInt32 nsBlockingStream::ReadBuffer(char *aBuf, PRUint32 aCount)
{
    if (aCount == 0) return 0; /* avoid calling LockStream, memcpy, etc. */

    PRUint32 bytesRead = 0;

    LockStream();

    /* Do not read more data than there is available... */
    if (aCount > m_DataLength) {
        aCount = m_DataLength;
    }

    /* Reading the data will cause m_ReadOffset to wrap */
    if (m_ReadOffset + aCount > m_BufferLength) {
        PRInt32 delta;

        /* Read the first chunk through the end of the buffer */
        delta = m_BufferLength - m_ReadOffset;
        memcpy(aBuf, &m_Buffer[m_ReadOffset], delta);

        /* Read the second chunk from the beginning of the buffer */
        m_ReadOffset = aCount-delta;
        memcpy(&aBuf[delta], m_Buffer, m_ReadOffset);
    } else {
        memcpy(aBuf, &m_Buffer[m_ReadOffset], aCount);
        m_ReadOffset += aCount;
    }

    bytesRead     = aCount;
    m_DataLength -= aCount;

    UnlockStream();

    return bytesRead;
}
