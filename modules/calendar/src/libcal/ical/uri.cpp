/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* 
 * uri.cpp
 * John Sun
 * 4/3/98 11:33:48 AM
 */

#include "stdafx.h"
#include "uri.h"
#include "nspr.h"

//---------------------------------------------------------------------

URI::URI()
{
}

//---------------------------------------------------------------------

URI::URI(UnicodeString fulluri)
{
    m_URI = fulluri;
}

//---------------------------------------------------------------------

URI::URI(char * fulluri)
{
    m_URI = fulluri;
}

//---------------------------------------------------------------------

UnicodeString URI::getName()
{
    t_int32 i;
    
    i = m_URI.indexOf(':');
    //PR_ASSERT(i != 0);
    if (i < 0)
    {
        return m_URI;
    }
    else
    {
        UnicodeString u;
        u = m_URI.extractBetween(i + 1, m_URI.size(), u);
        return u;
    }
}

//---------------------------------------------------------------------

t_bool URI::IsValidURI(UnicodeString & s)
{
    if (s.indexOf(':') > 0)
        return PR_TRUE;
    else
        return PR_FALSE;
}

//---------------------------------------------------------------------
