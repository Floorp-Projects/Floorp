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

#ifndef _nsHeaderPair_h_
#define _nsHeaderPair_h_

#include "nsIAtom.h"
#include "nsString.h"

/* 
    The nsHeaderPair is a struct for holding the atom corresponding to a header
    and the value associated with it. Note that this makes a copy of the value
    of the header. 

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/

struct nsHeaderPair
{
    // Converts header to an existing atom or creates a new one
    nsHeaderPair(const char* i_Header, const nsString* i_Value)
    {
        atom = NS_NewAtom(i_Header);
        NS_ASSERTION(atom, "Failed to create a new atom for nsHeaderPair!");
        if (i_Value)
        {
            value = new nsString(*i_Value);
        }
    };

    ~nsHeaderPair()
    {
        if (value)
        {
            delete value;
            value = 0;
        }
    };

    nsIAtom* atom;
    nsString* value;
};
#endif /* _nsHeaderPair_h_ */
