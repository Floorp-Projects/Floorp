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


/* XXX: Declare NET_PollSockets(...) for the blocking stream hack... */
extern "C" {
    XP_Bool NET_PollSockets(void);
};


NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);

#define BUFFER_BLOCK_SIZE   8192




nsConnectionInfo::nsConnectionInfo(nsIURL *aURL,
                                   nsNetlibStream *aStream, 
                                   nsIStreamListener *aNotify)
{
    NS_INIT_REFCNT();

    pURL       = aURL;
    pNetStream = aStream;
    pConsumer  = aNotify;

    if (NULL != pURL) {
        pURL->AddRef();
    }

    if (NULL != pNetStream) {
        pNetStream->AddRef();
    }

    if (NULL != pConsumer) {
        pConsumer->AddRef();
}
}


NS_IMPL_ISUPPORTS(nsConnectionInfo,kISupportsIID);


nsConnectionInfo::~nsConnectionInfo()
{
    TRACEMSG(("nsConnectionInfo is being destroyed...\n"));

    if (NULL != pURL) {
        pURL->Release();
    }

    if (NULL != pNetStream) {
        pNetStream->Close();
        pNetStream->Release();
    }

    if (NULL != pConsumer) {
        pConsumer->Release();
    }

    pURL       = NULL;
    pNetStream = NULL;
    pConsumer  = NULL;
}



nsNetlibStream::nsNetlibStream(void)
{
    NS_INIT_REFCNT();

    m_Lock = PR_NewMonitor();
    m_bIsClosed = PR_FALSE;
}


NS_IMPL_ADDREF(nsNetlibStream)
NS_IMPL_RELEASE(nsNetlibStream)

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
    return NS_NOINTERFACE;
}



nsNetlibStream::~nsNetlibStream()
{
    if (m_Lock) {
        PR_DestroyMonitor(m_Lock);
        m_Lock = NULL;
    }
}


void nsNetlibStream::Close()
{
    LockStream();
    m_bIsClosed = PR_TRUE;
    UnlockStream();
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
        free(m_Buffer);
        m_Buffer = NULL;
    }
}


PRInt32 nsBufferedStream::GetAvailableSpace(PRInt32 *aErrorCode)
{
    PRInt32 size = 0;

    if (m_bIsClosed) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
    } else {
        *aErrorCode = 0;

        LockStream();
        size = m_BufferLength - m_WriteOffset;
        UnlockStream();
    }
    return size;
}


PRInt32 nsBufferedStream::GetLength()
{
    PRInt32 size;

    LockStream();
    size = m_WriteOffset;
    UnlockStream();

    return size;
}


PRInt32 nsBufferedStream::Write(PRInt32 *aErrorCode, 
                                const char *aBuf, 
                                PRInt32 aOffset,
                                PRInt32 aLen)
{
    PRInt32 bytesWritten = 0;
    PRInt32 bytesFree;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");
    NS_PRECONDITION((m_WriteOffset >= m_ReadOffset), "Read past the end of buffer.");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
        goto done;
    } else {
        *aErrorCode = 0;
    }

    if (!m_bIsClosed && aBuf) {
        /* Grow the buffer if necessary */
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

        /* Skip the appropriate number of bytes in the input buffer... */
        if (aOffset) {
            aBuf += aOffset;
        }

        memcpy(&m_Buffer[m_WriteOffset], aBuf, aLen);
        m_WriteOffset += aLen;

        bytesWritten  = aLen;
        m_DataLength += aLen;
    }

done:
    UnlockStream();

    return bytesWritten;
}


PRInt32 nsBufferedStream::Read(PRInt32 *aErrorCode, 
                               char *aBuf, 
                               PRInt32 aOffset, 
                               PRInt32 aCount)
{
    PRInt32 bytesRead = 0;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");
    NS_PRECONDITION((m_WriteOffset >= m_ReadOffset), "Read past the end of buffer.");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed && (0 == m_DataLength)) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
        goto done;
    } else {
        *aErrorCode = 0;
    }

    if (m_Buffer && m_DataLength) {
        /* Skip the appropriate number of bytes in the input buffer... */
        if (aOffset) {
            aBuf += aOffset;
        }

        /* Do not read more data than there is available... */
        if (aCount > m_DataLength) {
            aCount = m_DataLength;
        }

        memcpy(aBuf, &m_Buffer[m_ReadOffset], aCount);
        m_ReadOffset += aCount;

        bytesRead     = aCount;
        m_DataLength -= aCount;
    }

done:
    UnlockStream();

    return bytesRead;
}






nsAsyncStream::nsAsyncStream(PRInt32 buffer_size)
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


PRInt32 nsAsyncStream::GetAvailableSpace(PRInt32 *aErrorCode)
{
    PRInt32 size = 0;

    if (m_bIsClosed) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
    } else {
        *aErrorCode = 0;

        LockStream();
        size = m_BufferLength - m_DataLength;
        UnlockStream();
    }
    return size;
}


PRInt32 nsAsyncStream::GetLength()
{
    PRInt32 size;

    LockStream();
    size = m_DataLength;
    UnlockStream();

    return size;
}


PRInt32 nsAsyncStream::Write(PRInt32 *aErrorCode,
                             const char *aBuf, 
                             PRInt32 aOffset,
                             PRInt32 aLen)
{
    PRInt32 bytesWritten = 0;
    PRInt32 bytesFree;

    LockStream();
    
    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
        goto done;
    } else {
        *aErrorCode = 0;
    }

    if (!m_bIsClosed && aBuf) {
        /* Skip the appropriate number of bytes in the input buffer... */
        if (aOffset) {
            aBuf += aOffset;
        }

        /* Do not store more data than there is space for... */
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

        bytesWritten  = aLen;
        m_DataLength += aLen;
    }

done:
    UnlockStream();

    return bytesWritten;
}


PRInt32 nsAsyncStream::Read(PRInt32 *aErrorCode, 
                            char *aBuf, 
                            PRInt32 aOffset, 
                            PRInt32 aCount)
{
    PRInt32 bytesRead   = 0;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed && (0 == m_DataLength)) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
        goto done;
    } else {
        *aErrorCode = 0;
    }

    if (m_Buffer && m_DataLength) {
        /* Skip the appropriate number of bytes in the input buffer... */
        if (aOffset) {
            aBuf += aOffset;
        }

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
    }

done:
    UnlockStream();

    return bytesRead;
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


PRInt32 nsBlockingStream::GetAvailableSpace(PRInt32 *aErrorCode)
{
    PRInt32 size = 0;

    if (m_bIsClosed) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
    } else {
        *aErrorCode = 0;

        LockStream();
        size = m_BufferLength - m_DataLength;
        UnlockStream();
    }
    return size;
}


PRInt32 nsBlockingStream::GetLength()
{
    PRInt32 size;

    LockStream();
    size = m_DataLength;
    UnlockStream();

    return size;
}


PRInt32 nsBlockingStream::Write(PRInt32 *aErrorCode,
                                const char *aBuf, 
                                PRInt32 aOffset,
                                PRInt32 aLen)
{
    PRInt32 bytesWritten = 0;
    PRInt32 bytesFree;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");
    
    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
        goto done;
    } else {
        *aErrorCode = 0;
    }

    if (!m_bIsClosed && aBuf) {
        /* Skip the appropriate number of bytes in the input buffer... */
        if (aOffset) {
            aBuf += aOffset;
        }

        /* Do not store more data than there is space for... */
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

        bytesWritten  = aLen;
        m_DataLength += aLen;
    }

done:
    UnlockStream();

    return bytesWritten;
}


PRInt32 nsBlockingStream::Read(PRInt32 *aErrorCode, 
                               char *aBuf, 
                               PRInt32 aOffset, 
                               PRInt32 aCount)
{
    PRInt32 bytesRead   = 0;

    LockStream();

    NS_PRECONDITION((m_Buffer || m_bIsClosed), "m_Buffer is NULL!");

    /* Check for initial error conditions... */
    if (NULL == aBuf) {
        *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
        goto done;
    } else if (m_bIsClosed && (0 == m_DataLength)) {
        *aErrorCode = NS_INPUTSTREAM_EOF;
        goto done;
    } else {
        *aErrorCode = 0;
    }

    if (m_Buffer) {
        /* Skip the appropriate number of bytes in the input buffer... */
        if (aOffset) {
            aBuf += aOffset;
        }

        /* Not enough data is available... Must block. */
        if (aCount > m_DataLength) {
            UnlockStream();
            do {
                NET_PollSockets();
                bytesRead += ReadBuffer(aBuf+bytesRead, aCount-bytesRead);
                /* XXX m_bIsClosed is checked outside of the lock! */
            } while ((aCount > bytesRead) && !m_bIsClosed); 
            LockStream();
            /* 
             * It is possible that the stream was closed during 
             * NET_PollSockets(...)...  In this case, return EOF if no data 
             * is available...
             */
            if ((0 == bytesRead) && m_bIsClosed) {
                *aErrorCode = NS_INPUTSTREAM_EOF;
            }
        } else {
            bytesRead = ReadBuffer(aBuf, aCount);
        }
    }
    
done:
    UnlockStream();

    return bytesRead;
}


PRInt32 nsBlockingStream::ReadBuffer(char *aBuf, PRInt32 aCount)
{
    PRInt32 bytesRead = 0;

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

