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

#ifndef nsFFEntry_h__
#define nsFFEntry_h__

#include "nsFFObject.h"
//#include "nsISupports.h"
/** An FFEntry consists of several objects, all io takes place 
  * through the FFEntry's read and write functions */

class nsFFEntry//: public nsISupports
{

public:
            nsFFEntry();
            nsFFEntry(const PRUint32 i_ID);
            //Single block entries can use this constructor
            nsFFEntry(const PRUint32 i_ID, const PRUint32 i_offset, const PRUint32 i_size);
    virtual ~nsFFEntry();

    /*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
    */

    PRBool      AddObject(const nsFFObject* i_object);

    //Appends an entry to the list
    PRBool      AddEntry(const nsFFEntry* i_pEntry);

    nsFFObject* FirstObject(void) const;
    void        FirstObject(const nsFFObject* i_object);

    PRUint32    ID(void) const;
    void        ID(const PRUint32 i_ID);

    nsFFEntry*  NextEntry(void) const;
    void        NextEntry(const nsFFEntry* i_pEntry);

    //Returns the number of objects in this entry. 
    PRUint32    Objects(void) const;

    PRBool      Remove(void);

    PRUint32    Size(void) const;

protected:

private:
    nsFFEntry(const nsFFEntry& o);
    nsFFEntry& operator=(const nsFFEntry& o);

    nsFFObject* m_pFirstObject;
    nsFFEntry* m_pNextEntry;

    PRUint32 m_ID;
    PRUint32 m_Objects;
};

inline
PRBool nsFFEntry::AddEntry(const nsFFEntry* i_Entry)
{
    if (!i_Entry)
        return PR_FALSE;
    if (m_pNextEntry)
        return m_pNextEntry->AddEntry(i_Entry);
    m_pNextEntry = (nsFFEntry*) i_Entry;
    return PR_TRUE;
}

inline
nsFFObject* nsFFEntry::FirstObject(void) const
{
    return m_pFirstObject;
}

inline
PRUint32 nsFFEntry::ID(void) const
{
    return m_ID;
}

inline
void nsFFEntry::ID(const PRUint32 i_id)
{
    m_ID = i_id;
}

inline
nsFFEntry* nsFFEntry::NextEntry(void) const
{
    return m_pNextEntry;
}

inline
void nsFFEntry::NextEntry(const nsFFEntry* i_pEntry)
{
    m_pNextEntry = (nsFFEntry*) i_pEntry;
}

inline
PRUint32 nsFFEntry::Objects(void) const
{
    return m_Objects;
}

inline
PRUint32 nsFFEntry::Size(void) const
{
    return m_pFirstObject ? m_pFirstObject->TotalSize() : 0; // Assumption that there is no zero length files //TODO
}

#endif // nsFFEntry_h__

