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

#ifndef nsCacheModule_h__
#define nsCacheModule_h__

/* 
 * nsCacheModule
 *
 * Gagan Saksena 02/03/98
 * 
 */

#include "nsCacheObject.h"
//class nsCacheObject;
/* Why the hell is forward decl. not working? */

class nsCacheModule 
{

public:
	nsCacheModule();
	nsCacheModule(const long i_size);

	virtual
		~nsCacheModule();

	virtual 
		int			    AddObject(nsCacheObject* i_pObject)=0;
    virtual
        int             Contains(const char* i_url) const=0;
	virtual 
		int			    Contains(nsCacheObject* i_pObject) const=0;
	void				Enable(int i_Enable);
	const int			Entries() const;
	nsCacheObject*		GetFirstObject() const ;

    virtual
	    nsCacheObject*	GetObject(const char* i_url) const=0;

    virtual
		nsCacheObject*	GetObject(long i_index) const =0;

    int					IsEnabled() const;
	nsCacheModule*		Next() const;
	void				Next(nsCacheModule*);
	const long			Size() const;
	const char*		    Trace() const;

protected:

	void			Size(const long i_size);

	int		m_Entries;
	long	m_Size;
	int		m_Enabled;

	nsCacheModule* m_pNext;

private:
	nsCacheModule(const nsCacheModule& cm);
	nsCacheModule& operator=(const nsCacheModule& cm);	

};

#if 0 // XXX remove
inline int  nsCacheModule::Contains(nsCacheObject* i_pObject) const 
{
	return 0;
}
#endif

inline void nsCacheModule::Enable(int i_Enable)
{
	m_Enabled = (i_Enable > 0) ? 1 : 0;
}

inline int nsCacheModule::IsEnabled() const
{
	return m_Enabled;
}

inline const int nsCacheModule::Entries() const 
{
	return m_Entries;
}

inline nsCacheObject* nsCacheModule::GetFirstObject() const 
{
	return this->GetObject((long)0);
}

inline nsCacheModule* nsCacheModule::Next() const 
{
	return m_pNext;
}

inline void nsCacheModule::Next(nsCacheModule* pNext) 
{
	m_pNext = pNext;
}

inline const long nsCacheModule::Size() const
{
	return m_Size;
}

inline void nsCacheModule::Size(const long size)
{
	m_Size = size;
}

#endif // nsCacheModule_h__
