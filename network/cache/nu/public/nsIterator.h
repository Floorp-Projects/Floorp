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

#ifndef nsIterator_h__
#define nsIterator_h__

//#include "nsISupports.h"
#include "prtypes.h"

class nsIterator //:public nsISupports
{

public:

    virtual void    First(void);
    virtual PRBool  IsDone(void) const = 0;
    virtual void    Next(void);
    virtual void*   CurrentItem(void) const;
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/

protected:
    nsIterator() {m_Index = 0;};

    PRUint32    m_Index;

};

inline 
void nsIterator::First(void)
{
    m_Index = 0;
}

inline
void nsIterator::Next(void)
{
    ++m_Index;
}

#endif // nsIterator_h__

