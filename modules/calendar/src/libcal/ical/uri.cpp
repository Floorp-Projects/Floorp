/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * uri.cpp
 * John Sun
 * 4/3/98 11:33:48 AM
 */

#include "stdafx.h"
#include "uri.h"

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
        return TRUE;
    else
        return FALSE;
}

//---------------------------------------------------------------------
