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

/* nsCacheObject is the class that holds the basic definition of the 
 * cache object.  A lot of changes are likely to occur before this 
 * goes on stage. 
 *
 * -Gagan Saksena 09/15/98
 */
#ifndef nsCacheObject_h__
#define nsCacheObject_h__

#if 0
#include "nsISupports.h"
#endif

#include "prtypes.h"
#include "prinrval.h"

static const PRUint32 kCACHE_VERSION = 5;

class nsStream;

class nsCacheObject //: public nsISupports
{

public:

    enum state_flags 
    {
        INIT=0x000000,
        PARTIAL=0x000001
    };

    nsCacheObject();
    nsCacheObject(const nsCacheObject& another);
    nsCacheObject(const char* i_url);

    virtual ~nsCacheObject();

/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);

*/

    void            Address(const char* i_url);
    const char*     Address(void) const;

    void            Charset(const char* i_Charset);
    const char*     Charset(void) const;

    void            ContentEncoding(const char* i_Encoding);
    const char*     ContentEncoding(void) const;

    void            ContentLength(PRUint32 i_len);
    PRUint32        ContentLength(void) const;
    
    void            ContentType(const char* i_Type);
    const char*     ContentType(void) const;

    void            Etag(const char* i_etag);
    const char*     Etag(void) const;

    void            Expires(PRIntervalTime i_Expires);
    PRIntervalTime  Expires(void) const;

    void            Filename(const char* i_Filename);
    const char*     Filename(void) const;

    PRUint16        Hits(void) const;

    /* Read and write info about this cache object */
    void*           Info(void) const;
    PRBool          Info(void* /*, PRUint32 len */);

    PRUint32        InfoSize(void) const;

    PRBool          IsCompleted(void) const;
    void            IsCompleted(PRBool bComplete);

    PRBool          IsExpired(void) const;

    PRBool          IsPartial(void) const;

    PRIntervalTime  LastAccessed(void) const;

    PRIntervalTime  LastModified(void) const;
    void            LastModified(const PRIntervalTime i_lastModified);

    PRInt16         Module(void) const;
    void            Module(PRUint16 i_m);

    void            PageServicesURL(const char* i_Url);
    const char*     PageServicesURL(void) const;

    const char*     PostData(void) const;
    void            PostData(const char* i_data, const PRUint32 i_Len);

    PRUint32        PostDataLen(void) const;

    PRUint32        Read(char* o_Buffer, PRUint32 len);
    
    /* Reset the streams/state, read/write locks, etc. */
    void            Reset(void);

    /* Accessor functions for the size of the cache object */
    PRUint32        Size(void) const;
    void            Size(PRUint32 i_Size);

    nsStream*       Stream(void) const;

    const char*     Trace() const;

    PRUint32        Write(const char* i_Buffer, const PRUint32 len);

protected:

    void            Init();

    char*           m_Charset;
    char*           m_ContentEncoding;
    PRUint32        m_ContentLength;
    char*           m_ContentType;
    char*           m_Etag;
    PRIntervalTime  m_Expires;
    int             m_Flags;
    char*           m_Filename;
    PRUint16        m_Hits;
    PRUint32        m_info_size;
    PRBool          m_bIsCompleted; /* Marked when the stream complete is called */
    PRIntervalTime  m_LastAccessed, m_LastModified;
    void*           m_pInfo;
    char*           m_PageServicesURL;
    char*           m_PostData;
    PRUint32        m_PostDataLen;
    PRInt16         m_Module;
    PRUint32        m_Size;
    nsStream*       m_pStream;
    char*           m_URL;

private:
    nsCacheObject& operator=(const nsCacheObject& x);
};

inline 
const char* nsCacheObject::Address(void) const
{
    return m_URL;
};

inline
const char* nsCacheObject::Charset(void) const
{
    return m_Charset;
}

inline
const char* nsCacheObject::ContentEncoding(void) const
{
    return m_ContentEncoding;
}

inline 
PRUint32 nsCacheObject::ContentLength(void) const
{
    return m_ContentLength;
};

inline 
void nsCacheObject::ContentLength(PRUint32 i_Size)
{
    m_ContentLength = i_Size;
};

inline 
const char* nsCacheObject::ContentType(void) const
{
    return m_ContentType;
};

inline 
const char* nsCacheObject::Etag(void) const 
{
    return m_Etag;
};

inline 
PRIntervalTime nsCacheObject::Expires(void) const
{
    return m_Expires;
};

inline 
void nsCacheObject::Expires(PRIntervalTime i_Expires)
{
    m_Expires = i_Expires;
};

inline
const char* nsCacheObject::Filename(void) const
{
    return m_Filename;
}

inline 
PRUint16 nsCacheObject::Hits(void) const
{
    return m_Hits;
};

inline
PRBool nsCacheObject::IsCompleted(void) const
{
    return m_bIsCompleted;
}

inline
void nsCacheObject::IsCompleted(PRBool bComplete) 
{
    m_bIsCompleted = bComplete;
}

inline 
PRBool nsCacheObject::IsExpired(void) const
{
    PRIntervalTime now = PR_IntervalNow();
    return (m_Expires <= now);
};

inline 
PRBool nsCacheObject::IsPartial(void) const
{
    return (m_Flags & nsCacheObject::PARTIAL);
};

inline 
PRIntervalTime nsCacheObject::LastAccessed(void) const
{
    return m_LastAccessed;
};

inline 
PRIntervalTime nsCacheObject::LastModified(void) const
{
    return m_LastModified;
};

inline 
void nsCacheObject::LastModified(const PRIntervalTime i_LastModified)
{
    m_LastModified = i_LastModified;
};

inline 
PRInt16 nsCacheObject::Module(void) const
{
    return m_Module;
};

inline 
void nsCacheObject::Module(PRUint16 i_Module) 
{
    m_Module = i_Module;
};

inline
const char* nsCacheObject::PageServicesURL(void) const
{
    return m_PageServicesURL;
}

inline
const char* nsCacheObject::PostData(void) const
{
    return m_PostData;
}

inline
PRUint32 nsCacheObject::PostDataLen(void) const
{
    return m_PostDataLen;
}

inline 
PRUint32 nsCacheObject::Size(void) const
{
    return m_Size;
};

inline 
void nsCacheObject::Size(PRUint32 i_Size)
{
    m_Size = i_Size;
};

inline
nsStream* nsCacheObject::Stream(void) const
{
    return m_pStream;
}
#endif // nsCacheObject_h__

