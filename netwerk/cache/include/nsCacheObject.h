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

// #if 0
#include "nsISupports.h"
// #endif
#include "pratom.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsStream.h"
#include "prtypes.h"
#include "prinrval.h"
#include "nsICacheObject.h"

static const PRUint32 kCACHE_VERSION = 5;

// class nsStream;

class nsCacheObject : public nsICacheObject
{

public:

    enum state_flags 
    {
        INIT=0,
        PARTIAL=1,
        COMPLETE=2,
        ABORTED=3,
        EXPIRED=4,
        CORRUPT=5
    };

    nsCacheObject();
    nsCacheObject(const nsCacheObject& another);
    nsCacheObject(const char* i_url);

    virtual ~nsCacheObject();

    NS_DECL_ISUPPORTS


/* public interface */

//    NS_IMETHOD Create(const char * i_url, void * aObj) = 0 ;

//    NS_IMETHOD Destroy(void * pThis) = 0 ;

    NS_IMETHOD GetAddress(char ** Addr) const ;
    NS_IMETHOD SetAddress(const char* i_Address) ;

    NS_IMETHOD GetCharset(char ** CSet) const ;
    NS_IMETHOD SetCharset(const char* i_CharSet) ;

    NS_IMETHOD GetContentEncoding(char ** Encoding) const ;
    NS_IMETHOD SetContentEncoding(const char* i_Encoding) ;

    NS_IMETHOD GetContentLength(PRUint32 * CLeng) const ;
    NS_IMETHOD SetContentLength(PRUint32 i_Len) ;

    NS_IMETHOD GetContentType(char ** CType) const ;
    NS_IMETHOD SetContentType(const char* i_Type) ;

    NS_IMETHOD GetEtag(char ** Etag) const ;
    NS_IMETHOD SetEtag(const char* i_Etag) ;

    NS_IMETHOD GetExpires(PRIntervalTime * iTime) const ;
    NS_IMETHOD SetExpires(const PRIntervalTime i_Time) ;

    NS_IMETHOD GetFilename(char ** Filename) const ;
    NS_IMETHOD SetFilename(const char* i_Filename) ;

    NS_IMETHOD GetIsCompleted(PRBool *bComplete) const ;
    NS_IMETHOD SetIsCompleted(PRBool bComplete) ;

    NS_IMETHOD GetLastAccessed(PRIntervalTime * iTime) const ;

    NS_IMETHOD GetLastModified(PRIntervalTime * iTime) const ;
    NS_IMETHOD SetLastModified(const PRIntervalTime i_Time) ;

    NS_IMETHOD GetModuleIndex(PRInt16 * m_index) const ;
    NS_IMETHOD SetModuleIndex(const PRUint16 m_index) ;

    NS_IMETHOD GetPageServicesURL(char ** o_url) const ;
    NS_IMETHOD SetPageServicesURL(const char* i_Url) ;

    NS_IMETHOD GetPostData(char ** pData) const ;
    NS_IMETHOD SetPostData(const char* i_PostData, const PRUint32 i_Len) ;

    NS_IMETHOD GetPostDataLen(PRUint32 * dLeng) const ;

    /* Accessor functions for the size of the cache object */
    NS_IMETHOD GetSize(PRUint32 * pSize) const ;
    NS_IMETHOD SetSize(const PRUint32 i_Size) ;

    /* Accessor functions for the state of the cache object.
     * Some states are changed internally.
     */
    NS_IMETHOD GetState(PRUint32 * pState) const ;
    NS_IMETHOD SetState(const PRUint32 i_State) ;

    NS_IMETHOD GetStream(nsStream ** pStream) const ;
//	NS_IMETHOD MakeStream(void) ;

    NS_IMETHOD Hits(PRUint32 * pHits) const ;

    NS_IMETHOD IsExpired(PRBool *bGet) const ;

    NS_IMETHOD IsPartial(PRBool *bGet) const ;

    NS_IMETHOD Read(char* o_Destination, PRUint32 i_Len, 
                                                       PRUint32 * pLeng) ;

    /* Reset the streams/state, read/write locks, etc. */
    NS_IMETHOD Reset(void) ;

//    NS_IMETHOD Synch(void) = 0 ;

    NS_IMETHOD Write(const char* i_buffer, const PRUint32 i_length, 
                                                        PRUint32 * oLeng) ;

//	NS_IMETHOD InitUrl(const char* i_url) ;

    /* Read and write info about this cache object */
    NS_IMETHOD GetInfo(void** o_info) ;
    NS_IMETHOD SetInfo(void* i_info /*, PRUint32 len */);

    NS_IMETHOD GetInfoSize(PRUint32* o_size) ;

/* end public interface */

    const char*     Trace() const;

protected:

    void            Init();

    char*           m_Charset;
    char*           m_ContentEncoding;
    PRUint32        m_ContentLength;
    char*           m_ContentType;
    char*           m_Etag;
    PRIntervalTime  m_Expires;
    char*           m_Filename;
    PRUint32        m_State;
    PRUint16        m_Hits;
    PRUint32        m_info_size;
    PRBool          m_bIsCompleted; /* Marked when the stream complete is called */
    PRIntervalTime  m_LastAccessed, m_LastModified;
    PRInt16         m_Module;
    void*           m_pInfo;
    char*           m_PageServicesURL;
    char*           m_PostData;
    PRUint32        m_PostDataLen;
    PRUint32        m_Size;
    nsStream*       m_pStream;
    char*           m_URL;

private:
    nsCacheObject& operator=(const nsCacheObject& x);
};

inline NS_IMETHODIMP
nsCacheObject::GetAddress(char ** Addr) const
{
	if (!Addr)
	  return NS_ERROR_NULL_POINTER;

	*Addr = m_URL ;
    return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetCharset(char ** CSet) const
{
	if (!CSet)
	  return NS_ERROR_NULL_POINTER;

	*CSet = m_Charset;
    return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::GetContentEncoding(char ** Encoding) const
{
	if (!Encoding)
	  return NS_ERROR_NULL_POINTER;

    *Encoding=m_ContentEncoding;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::GetContentLength(PRUint32 * CLeng) const
{
	if (!CLeng)
	  return NS_ERROR_NULL_POINTER;

    *CLeng = m_ContentLength;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::SetContentLength(PRUint32 i_Size)
{
    m_ContentLength = i_Size;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetContentType(char ** CType) const
{
	if (!CType)
	  return NS_ERROR_NULL_POINTER;

    *CType = m_ContentType;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetEtag(char ** Etag) const 
{
	if (!Etag)
	  return NS_ERROR_NULL_POINTER;

    *Etag = m_Etag;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetExpires(PRIntervalTime * iTime) const
{
	if (!iTime)
	  return NS_ERROR_NULL_POINTER;

    *iTime = m_Expires;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::SetExpires(PRIntervalTime i_Expires)
{
    m_Expires = i_Expires;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetFilename(char ** Filename) const
{
	if (!Filename)
	  return NS_ERROR_NULL_POINTER;

    *Filename = m_Filename;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::Hits(PRUint32 * pHits) const
{
    *pHits = m_Hits;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetIsCompleted(PRBool *bComplete) const
{
    if (m_bIsCompleted) {
	  *bComplete=PR_TRUE ;
	}
	else {
	  *bComplete=PR_FALSE ;
	}
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::SetIsCompleted(PRBool bComplete) 
{
    m_bIsCompleted = bComplete;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::IsExpired(PRBool *bExpired) const
{
    if (nsCacheObject::EXPIRED==m_State)
	  *bExpired = PR_TRUE ;

    if (m_Expires <= PR_IntervalNow()) {
        ((nsCacheObject*)this)->m_State = nsCacheObject::EXPIRED;
		*bExpired = PR_TRUE ;
	}
	else 
	  *bExpired = PR_FALSE ;

	return NS_OK ;

};

inline NS_IMETHODIMP
nsCacheObject::IsPartial(PRBool *bPartial) const
{
    if (m_ContentLength != m_Size) 
	  *bPartial = PR_TRUE ;
	else 
	  *bPartial = PR_FALSE ;

	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetLastAccessed(PRIntervalTime * iTime) const
{
    *iTime = m_LastAccessed;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetLastModified(PRIntervalTime * iTime) const
{
    *iTime = m_LastModified;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::SetLastModified(const PRIntervalTime i_LastModified)
{
    m_LastModified = i_LastModified;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetModuleIndex(PRInt16 * m_index) const
{
    *m_index = m_Module;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::SetModuleIndex(PRUint16 m_index) 
{
    m_Module = m_index;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetPageServicesURL(char ** o_url) const
{
    if (!o_url)
	  return NS_ERROR_NULL_POINTER;

    *o_url = m_PageServicesURL;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::GetPostData(char ** pData) const
{
	if (!pData)
	  return NS_ERROR_NULL_POINTER;

    *pData = m_PostData;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::GetPostDataLen(PRUint32 * dLeng) const
{
	if (!dLeng)
	  return NS_ERROR_NULL_POINTER;

    *dLeng = m_PostDataLen;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCacheObject::GetSize(PRUint32 * pSize) const
{
	if (!pSize)
	  return NS_ERROR_NULL_POINTER;

    *pSize = m_Size;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::SetSize(PRUint32 i_Size)
{
    m_Size = i_Size;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetState(PRUint32 * pState) const
{
	if (!pState)
	  return NS_ERROR_NULL_POINTER;

    *pState = m_State;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::SetState(PRUint32 i_State)
{
    m_State = i_State;
	return NS_OK ;
};

inline NS_IMETHODIMP
nsCacheObject::GetStream(nsStream** pStream) const
{
    if (!pStream)
	  return NS_ERROR_NULL_POINTER;

    *pStream = m_pStream;
	return NS_OK ;
}

/////////////////////////////////////////////////////////////////////
// nsCacheObjectFactory

class nsCacheObjectFactory : public nsIFactory {
public:
    nsCacheObjectFactory();
    virtual ~nsCacheObjectFactory();
            
    NS_DECL_ISUPPORTS
                  
    NS_IMETHOD CreateInstance(nsISupports *aOuter,  
                             const nsIID &aIID,  
                             void **aResult);  
                
    NS_IMETHOD LockFactory(PRBool aLock);

} ;

#endif // nsCacheObject_h__
