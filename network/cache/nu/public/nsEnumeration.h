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

#ifndef nsEnumeration_h__
#define nsEnumeration_h__

//#include "nsISupports.h"
#include "nsIterator.h"
#include "prlog.h"

class nsEnumeration //: public nsISupports
{

public:
            nsEnumeration(nsIterator* iter);
    virtual ~nsEnumeration();

    PRBool  HasMoreElements(void);
    void*   NextElement(void);
    void    Reset(void);
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
protected:
    nsIterator* m_pIter;
private:
    nsEnumeration(const nsEnumeration& o);
    nsEnumeration& operator=(const nsEnumeration& o);
};

inline 
nsEnumeration::nsEnumeration(nsIterator* iter):m_pIter(iter)
{
    PR_ASSERT(iter);
    if (m_pIter)
        m_pIter->First();
}

inline
PRBool nsEnumeration::HasMoreElements(void)
{
    if (m_pIter) //remove this check for optimization?
        return m_pIter->IsDone() ? PR_FALSE : PR_TRUE;
    return PR_FALSE;
}

inline
void* nsEnumeration::NextElement(void)
{
    if (m_pIter)
    {
        void* pTemp = m_pIter->CurrentItem();
        m_pIter->Next();
        return pTemp;
    }
    return 0;
}

inline
void nsEnumeration::Reset(void)
{
    if (m_pIter)
    {
        m_pIter->First();
    }
}

#endif // nsEnumeration_h__

