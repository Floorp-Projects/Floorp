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

#ifndef net_strm_h___
#define net_strm_h___

#include "nspr.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsIConnectionInfo.h"

/* Forward declaration... */
class nsNetlibStream;

/* Enumeration of the various connection states... */
typedef enum {
  nsConnectionActive = 0,
  nsConnectionSucceeded,
  nsConnectionAborted,
  nsConnectionFailed,
  nsConnectionMax
} nsConnectionStatus;


class nsConnectionInfo : public nsIConnectionInfo 
{
public:
    NS_DECL_ISUPPORTS

    nsConnectionInfo(nsIURL *aURL, 
                     nsNetlibStream *aStream, 
                     nsIStreamListener *aNotify);

    NS_IMETHOD GetURL(nsIURL **aURL);
    NS_IMETHOD GetInputStream(nsIInputStream **aStream);
    NS_IMETHOD GetOutputStream(nsIOutputStream **aStream);
    NS_IMETHOD GetConsumer(nsIStreamListener **aConsumer);

protected:
    virtual ~nsConnectionInfo();

public:
    nsIURL                *pURL;
    nsNetlibStream        *pNetStream;
    nsIStreamListener     *pConsumer;
    nsConnectionStatus    mStatus;
    PRThread*             mRequestingThread;
};


class nsNetlibStream : public nsIInputStream,
                       public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS

    nsNetlibStream(void);

    NS_IMETHOD GetAvailableSpace(PRUint32 *aCount) = 0;

    /* From nsIBaseStream interface */
    NS_IMETHOD Close(void);

protected:
    virtual ~nsNetlibStream();

    inline void LockStream  (void) { if (m_Lock) PR_EnterMonitor(m_Lock); }
    inline void UnlockStream(void) { if (m_Lock) PR_ExitMonitor (m_Lock); }

protected:
    PRBool     m_bIsClosed;
    PRMonitor* m_Lock;

};

/*
 * Variable size, buffered stream...
 */
class nsBufferedStream : public nsNetlibStream {

public:
    nsBufferedStream(void);

    NS_IMETHOD GetAvailableSpace(PRUint32 *aCount);
    
    /* nsIInputStream interface */
    NS_IMETHOD GetLength(PRUint32 *aLength);

    NS_IMETHOD Read(char *aBuf, 
                    PRUint32 aCount,
                    PRUint32 *aReadCount);

    /* nsIOutputStream interface */
    NS_IMETHOD Write(const char *aBuf, 
                     PRUint32 aLen,
                     PRUint32 *aWriteCount);

    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush(void) {
        return NS_OK;
    }

protected:
    virtual ~nsBufferedStream();

private:
    char *m_Buffer;
    PRUint32 m_BufferLength;

    PRUint32 m_DataLength;
    PRUint32 m_ReadOffset;
    PRUint32 m_WriteOffset;
};


/*
 * Fixed size stream...
 */

class nsAsyncStream : public nsNetlibStream {

public:
    nsAsyncStream(PRUint32 buffer_size);

    NS_IMETHOD GetAvailableSpace(PRUint32 *aCount);

    /* nsIInputStream interface */
    NS_IMETHOD GetLength(PRUint32 *aLength);

    NS_IMETHOD Read(char *aBuf, 
                    PRUint32 aCount,
                    PRUint32 *aReadLength);

    /* nsIOutputStream interface */
    NS_IMETHOD Write(const char *aBuf, 
                     PRUint32 aLen,
                     PRUint32 *aWriteLength);

    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush(void) {
        return NS_OK;
    }

protected:
    virtual ~nsAsyncStream();

private:
    char *m_Buffer;
    PRUint32 m_BufferLength;

    PRUint32 m_DataLength;
    PRUint32 m_ReadOffset;
    PRUint32 m_WriteOffset;
};


/*
 * Fixed size, blocking stream...
 */
class nsBlockingStream : public nsNetlibStream {

public:
    nsBlockingStream(void);

    NS_IMETHOD GetAvailableSpace(PRUint32 *aCount);

    /* nsIBaseStream interface */
    NS_IMETHOD Close(void);

    /* nsIInputStream interface */
    NS_IMETHOD GetLength(PRUint32 *aLength);

    NS_IMETHOD Read(char *aBuf, 
                    PRUint32 aCount,
                    PRUint32 *aReadLength);
  
    /* nsIOutputStream interface */
    NS_IMETHOD Write(const char *aBuf, 
                     PRUint32 aLen,
                     PRUint32 *aWriteLength);
  
    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush(void) {
        return NS_OK;
    }

protected:
    virtual ~nsBlockingStream();

    PRInt32 ReadBuffer(char *aBuf, PRUint32 aCount);

private:
    char *m_Buffer;
    PRUint32 m_BufferLength;
 
    PRUint32 m_DataLength;
    PRUint32 m_ReadOffset;
    PRUint32 m_WriteOffset;
};

#endif /* net_strm_h___ */
