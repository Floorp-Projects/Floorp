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

#ifndef _nsIHeader_h_
#define _nsIHeader_h_

#include "nsISupports.h"
/* 
    nsIHeader. A class to encapsulate and share the header reading and 
    writing on requests/responses of several protocols. 
    
    I am not convinced the GetHeaderMultiple is the right way to 
    do multiple values, but give me a better way... TODO think.

    - Gagan Saksena 03/08/99
*/

class nsIHeader : public nsISupports
{
public:

    /* 
        SetHeader- set a particular header. The implementation is protocol 
        specific. For e.g. HTTP will put a colon and space to separate
        the header with the value and then trail it will an newline. 
        So SetHeader for HTTP with say ("Accept", "text/html") will 
        result in a string "Accept: text/html\n" being added to the header
        set.
    */
    NS_IMETHOD          SetHeader(const char* i_Header, const char* i_Value) = 0;

    /*
        Get the first occurence of the header and its corresponding value.
        Note that if you expect the possibility of multiple values, you 
        should use GetHeaderMultiple() version.
    */
    NS_IMETHOD          GetHeader(const char* i_Header, const char* *o_Value) const = 0;

    /*
        This version returns an array of values associated with this
        header. TODO think of a better way to do this...
    */
    NS_IMETHOD          GetHeaderMultiple(
                            const char* i_Header, 
                            const char** *o_ValueArray,
                            int o_Count) const = 0;

    static const nsIID& GetIID() { 
        // {4CD2C720-D5CF-11d2-B013-006097BFC036}
        static const nsIID NS_IHEADER_IID = 
            { 0x4cd2c720, 0xd5cf, 0x11d2, { 0xb0, 0x13, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };

        return NS_IHEADER_IID;
    }
}
#endif // _nsIHeader_h_