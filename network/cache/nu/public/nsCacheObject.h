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

	void		Address(const char* i_url);
	const char* Address(void) const;

	void		Etag(const char* i_etag);
	const char* Etag(void) const;

	long		Expires(void) const;
	void		Expires(long i_Expires);

	int			Hits(void) const;
	int			IsExpired(void) const;
	int			IsPartial(void) const;
	long		LastAccessed(void) const;

	long		LastModified(void) const;
	void		LastModified(long i_lastModified);

	long		Size(void) const;
	void		Size(long s);

	const char*		Trace() const;
	
//	virtual	void getReadStream();
//	virtual void getWriteStream();

protected:
	
	void	Init();

	char*	m_Etag;
	long	m_Expires;
	int		m_Flags;
	int		m_Hits;
	long	m_LastAccessed;
	long	m_LastModified;
	long	m_Size;
	char*	m_Url;

private:
    nsCacheObject& operator=(const nsCacheObject& x);	
};

inline const char* nsCacheObject::Address(void) const
{
	return m_Url;
}

inline const char* nsCacheObject::Etag(void) const 
{
	return m_Etag;
}

inline long nsCacheObject::Expires(void) const
{
	return m_Expires;
};

inline void nsCacheObject::Expires(long i_Expires)
{
	m_Expires = i_Expires;
}

inline int nsCacheObject::Hits(void) const
{
	return m_Hits;
}

#ifndef _DEBUG
inline int nsCacheObject::IsExpired(void) const
{
	time_t now = time();
	return (m_Expires <= now) ? 1 : 0;
}
#endif

inline int nsCacheObject::IsPartial(void) const
{
	return (m_Flags & nsCacheObject::PARTIAL) ? 1 : 0;
}

inline long nsCacheObject::LastAccessed(void) const
{
	return m_LastAccessed;
}

inline long nsCacheObject::LastModified(void) const
{
	return m_LastModified;
}

inline void nsCacheObject::LastModified(long i_LastModified)
{
	m_LastModified = i_LastModified;
}

inline long nsCacheObject::Size(void) const
{
	return m_Size;
}

inline void nsCacheObject::Size(long i_Size)
{
	m_Size = i_Size;
}

#endif // nsCacheObject_h__

