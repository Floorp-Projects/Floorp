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

#ifndef _nsSocketKey_h_
#define _nsSocketKey_h_

#include "nsHashtable.h" // also defines nsCStringKey
#include "plstr.h"

class nsSocketKey : public nsStringKey 
{
  
public:
    // Constructor and Destructor
    nsSocketKey(const char* i_Host, const PRInt32 i_port)
        : nsStringKey(i_Host), m_Port(i_port)
    {
    }
  
    virtual ~nsSocketKey(void) 
    {
    }
  
    nsHashKey* Clone(void) const 
    {
        return new nsSocketKey(mStr, m_Port);
    }

    PRBool Equals(const nsHashKey* i_Key) const 
    {
        return (m_Port == ((nsSocketKey*)i_Key)->m_Port) && 
                nsStringKey::Equals(i_Key);
    }

    PRBool operator==(const nsSocketKey& i_Key) const
    {
        return Equals(&i_Key);
    }


private:
    nsSocketKey(const nsSocketKey& i_Key);
    nsSocketKey& operator=(const nsSocketKey& i_Key);

    PRInt32     m_Port;
};

#endif /* _nsSocketKey_h_ */
