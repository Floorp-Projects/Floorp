/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* Forward declaration... */
class nsNetlibStream;

class nsConnectionInfo : public nsISupports 
{
public:
    NS_DECL_ISUPPORTS

    nsConnectionInfo(nsIURL *aURL, 
                     nsNetlibStream *aStream, 
                     nsIStreamListener *aNotify);

protected:
    virtual ~nsConnectionInfo();

public:
    nsIURL                *pURL;
    nsNetlibStream        *pNetStream;
    nsIStreamListener     *pConsumer;
};


class nsNetlibStream : public nsIInputStream,
                       public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS

    nsNetlibStream(void);

    virtual PRInt32 GetAvailableSpace(PRInt32 *aErrorCode) = 0;

    /* From nsIBaseStream interface */
    virtual void Close(void);

protected:
    virtual ~nsNetlibStream();

    inline void LockStream  (void) { if (m_Lock) PR_EnterMonitor(m_Lock); }
    inline void UnlockStream(void) { if (m_Lock) PR_ExitMonitor (m_Lock); }

protected:
    PRBool m_bIsClosed;

private:
    PRMonitor *m_Lock;

};

/*
 * Variable size, buffered stream...
 */
class nsBufferedStream : public nsNetlibStream {

public:
    nsBufferedStream(void);

    virtual PRInt32 GetAvailableSpace(PRInt32 *aErrorCode);
    
    /* From nsIBaseStream interface */
    virtual PRInt32 GetLength(void);

    /* nsIInputStream interface */
    virtual PRInt32 Read(PRInt32 *aErrorCode, 
                         char *aBuf, 
                         PRInt32 aOffset, 
                         PRInt32 aCount);

    /* nsIOutputStream interface */
    virtual PRInt32 Write(PRInt32 *aErrorCode,
                          const char *aBuf, 
                          PRInt32 aOffset,
                          PRInt32 aLen);

protected:
    virtual ~nsBufferedStream();

private:
    char *m_Buffer;
    PRInt32 m_BufferLength;

    PRInt32 m_DataLength;
    PRInt32 m_ReadOffset;
    PRInt32 m_WriteOffset;
};


/*
 * Fixed size stream...
 */

class nsAsyncStream : public nsNetlibStream {

public:
    nsAsyncStream(PRInt32 buffer_size);

    virtual PRInt32 GetAvailableSpace(PRInt32 *aErrorCode);

    /* From nsIBaseStream interface */
    virtual PRInt32 GetLength(void);

    /* nsIInputStream interface */
    virtual PRInt32 Read(PRInt32 *aErrorCode, 
                         char *aBuf, 
                         PRInt32 aOffset, 
                         PRInt32 aCount);

    /* nsIOutputStream interface */
    virtual PRInt32 Write(PRInt32 *aErrorCode,
                          const char *aBuf, 
                          PRInt32 aOffset,
                          PRInt32 aLen);

protected:
    virtual ~nsAsyncStream();

private:
    char *m_Buffer;
    PRInt32 m_BufferLength;

    PRInt32 m_DataLength;
    PRInt32 m_ReadOffset;
    PRInt32 m_WriteOffset;
};


/*
 * Fixed size, blocking stream...
 */
class nsBlockingStream : public nsNetlibStream {

public:
    nsBlockingStream(void);

    virtual PRInt32 GetAvailableSpace(PRInt32 *aErrorCode);

    /* From nsIBaseStream interface */
    virtual PRInt32 GetLength(void);

    /* nsIInputStream interface */
    virtual PRInt32 Read(PRInt32 *aErrorCode, 
                         char *aBuf, 
                         PRInt32 aOffset, 
                         PRInt32 aCount);

    /* nsIOutputStream interface */
    virtual PRInt32 Write(PRInt32 *aErrorCode,
                          const char *aBuf, 
                          PRInt32 aOffset,
                          PRInt32 aLen);

protected:
    virtual ~nsBlockingStream();

    PRInt32 ReadBuffer(char *aBuf, PRInt32 aCount);

private:
    char *m_Buffer;
    PRInt32 m_BufferLength;

    PRInt32 m_DataLength;
    PRInt32 m_ReadOffset;
    PRInt32 m_WriteOffset;
};

#endif /* net_strm_h___ */
