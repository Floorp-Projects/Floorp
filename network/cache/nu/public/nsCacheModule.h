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
	nsCacheModule(const PRUint32 i_size);

	virtual
		~nsCacheModule();

	virtual 
		PRBool          AddObject(nsCacheObject* i_pObject)=0;
    virtual
        PRBool          Contains(const char* i_url) const=0;
	virtual 
		PRBool		    Contains(nsCacheObject* i_pObject) const=0;
	void				Enable(PRBool i_Enable);
	const PRUint32      Entries() const;

	nsCacheObject*		GetFirstObject() const ;//TODO-?/

    virtual
	    nsCacheObject*	GetObject(const char* i_url) const=0;

    virtual
		nsCacheObject*	GetObject(PRUint32 i_index) const =0;

    PRBool				IsEnabled() const;

	nsCacheModule*		Next() const;
	void				Next(nsCacheModule*);

	const PRUint32		Size() const;
	void			    Size(const PRUint32 i_size);

	const char*		    Trace() const;

protected:

	PRUint32    m_Entries;
	PRUint32	m_Size;
	PRBool		m_Enabled;

	nsCacheModule* m_pNext;

private:
	nsCacheModule(const nsCacheModule& cm);
	nsCacheModule& operator=(const nsCacheModule& cm);	

};

inline void nsCacheModule::Enable(PRBool i_Enable)
{
	m_Enabled = i_Enable;
}

inline PRBool nsCacheModule::IsEnabled() const
{
	return m_Enabled;
}

inline const PRUint32 nsCacheModule::Entries() const 
{
	return m_Entries;
}

inline nsCacheObject* nsCacheModule::GetFirstObject() const 
{
	return this->GetObject((PRUint32)0);
}

inline nsCacheModule* nsCacheModule::Next() const 
{
	return m_pNext;
}

inline void nsCacheModule::Next(nsCacheModule* pNext) 
{
	m_pNext = pNext;
}

inline const PRUint32 nsCacheModule::Size() const
{
	return m_Size;
}

inline void nsCacheModule::Size(const PRUint32 size)
{
	m_Size = size;
}

#endif // nsCacheModule_h__
