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

#include "nsFFEntry.h"
#include <prmem.h>
#include <prlog.h>

nsFFEntry::nsFFEntry():
    m_ID(0),
    m_pFirstObject(0),
    m_pNextEntry(0),
    m_Objects(0)
{
}

nsFFEntry::nsFFEntry(const PRUint32 i_ID):
    m_ID(i_ID),
    m_pFirstObject(0),
    m_pNextEntry(0),
    m_Objects(0)
{
}

nsFFEntry::nsFFEntry(const PRUint32 i_ID, const PRUint32 i_offset, const PRUint32 i_size):
    m_ID(i_ID),
    m_pFirstObject(new nsFFObject(i_ID, i_offset, i_size)),
    m_pNextEntry(0),
    m_Objects(1)
{
    //PR_ASSERT(i_offset > nsFlatFile::HeaderSize());
}

nsFFEntry::~nsFFEntry()
{
    PR_FREEIF(m_pFirstObject);
    PR_FREEIF(m_pNextEntry);
}


/*
nsrefcnt nsFFEntry::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsFFEntry::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsFFEntry::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}

nsFFEntry::
*/

PRBool nsFFEntry::AddObject(const nsFFObject* i_object)
{
    PR_ASSERT(i_object);
    if (!i_object)
        return PR_FALSE;
    if (!m_pFirstObject)
    {
        m_pFirstObject = (nsFFObject*) i_object;
        m_Objects++;
    }
    return m_pFirstObject->Add(i_object);
    return PR_FALSE;
}
