/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* A simple cache iterator for cache architecture. This is a temporary class
 * which will be (should be) replaced by more professional quality code, or 
 * at least moved to a more common area later on. 
 *
 * -Gagan Saksena 09/15/98 
 */
#ifndef nsCacheIterator_h__
#define nsCacheIterator_h__

#include "nsIterator.h"
#include "prtypes.h"
#include "nsICacheModule.h"
#include "nsICacheObject.h"

class nsCacheIterator: public nsIterator
{

public:
            nsCacheIterator(nsICacheModule* i_pModule);
    virtual ~nsCacheIterator();

    virtual PRBool          IsDone(void) const;
    virtual nsICacheObject*  Current(void) const;

private:
    nsCacheIterator(const nsCacheIterator& o);
    nsCacheIterator& operator=(const nsCacheIterator& o);
    nsICacheModule*    m_pModule;
};

inline
nsCacheIterator::nsCacheIterator(nsICacheModule* i_pModule): m_pModule(i_pModule)
{
}

inline
nsCacheIterator::~nsCacheIterator()
{
}

inline
PRBool nsCacheIterator::IsDone(void) const
{
	/*
    if (m_pModule)
    {
        return m_pModule->Entries() <= m_Index;
    }
    else
        return PR_FALSE;
		*/
}

inline
nsICacheObject* nsCacheIterator::Current(void) const
{
	nsICacheObject* pObj ;

    if (m_pModule)
    {
        m_pModule->GetObjectByIndex(m_Index, &pObj);
		return pObj ;
    }
    return 0;
}
#endif // nsCacheIterator_h__

