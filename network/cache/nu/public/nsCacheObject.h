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

#ifndef nsCacheObject_h__
#define nsCacheObject_h__

#if 0
#include "nsISupports.h"
#endif

#include <prtypes.h>
#include <prinrval.h>

static const PRUint32 kCACHE_VERSION = 5;

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

    void        Address(const char* i_url);
    const char* Address(void) const;

    void        Etag(const char* i_etag);
    const char* Etag(void) const;

    PRIntervalTime
                Expires(void) const;
    void        Expires(PRIntervalTime i_Expires);

    PRUint16    Hits(void) const;

    /* Read and write info about this cache object */
    void*       Info(void) const;
    PRBool      Info(void*);

    PRUint32    InfoSize(void) const;

    PRBool      IsExpired(void) const;

    PRBool      IsPartial(void) const;

    PRIntervalTime
                LastAccessed(void) const;

    PRIntervalTime
                LastModified(void) const;
    void        LastModified(PRIntervalTime i_lastModified);

    PRInt16     Module(void) const;
    void        Module(PRUint16 i_m);

    PRUint32    Size(void) const;
    void        Size(PRUint32 s);

    const char*     
                Trace() const;

//	virtual	void getReadStream();
//	virtual void getWriteStream();

protected:

    void        Init();

    char*       m_Etag;
    PRIntervalTime
                m_Expires;
    int         m_Flags;
    PRUint16    m_Hits;
    PRIntervalTime
                m_LastAccessed, m_LastModified;
    PRUint32    m_Size;
    char*       m_Url;

    PRInt16     m_Module;

    void*       m_pInfo;
    PRUint32    m_info_size;

private:
    nsCacheObject& operator=(const nsCacheObject& x);
};

inline const char* nsCacheObject::Address(void) const
{
	return m_Url;
};

inline const char* nsCacheObject::Etag(void) const 
{
	return m_Etag;
};

inline PRIntervalTime nsCacheObject::Expires(void) const
{
	return m_Expires;
};

inline void nsCacheObject::Expires(PRIntervalTime i_Expires)
{
	m_Expires = i_Expires;
};

inline PRUint16 nsCacheObject::Hits(void) const
{
	return m_Hits;
};

inline PRBool nsCacheObject::IsExpired(void) const
{
	PRIntervalTime now = PR_IntervalNow();
	return (m_Expires <= now);
};

inline PRBool nsCacheObject::IsPartial(void) const
{
	return (m_Flags & nsCacheObject::PARTIAL);
};

inline PRIntervalTime nsCacheObject::LastAccessed(void) const
{
	return m_LastAccessed;
};

inline PRIntervalTime nsCacheObject::LastModified(void) const
{
	return m_LastModified;
};

inline void nsCacheObject::LastModified(PRIntervalTime i_LastModified)
{
	m_LastModified = i_LastModified;
};

inline PRInt16 nsCacheObject::Module(void) const
{
    return m_Module;
};

inline void nsCacheObject::Module(PRUint16 i_Module) 
{
    m_Module = i_Module;
};

inline PRUint32 nsCacheObject::Size(void) const
{
	return m_Size;
};

inline void nsCacheObject::Size(PRUint32 i_Size)
{
	m_Size = i_Size;
};

#endif // nsCacheObject_h__

