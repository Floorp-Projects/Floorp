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
 * unistrto.cpp
 * John Sun
 * 2/3/98 12:07:12 PM
 */

#include "stdafx.h"
#include "jdefines.h"

#include "unistring.h"
#include "unistrto.h"
#include "keyword.h"

//static UnicodeString s_DefaultDelims = "\t\n\r";
//---------------------------------------------------------------------

void 
UnicodeStringTokenizer::skipDelimeters()
{
    while ((m_CurrentPosition < m_MaxPosition) &&
        (m_StringDelimeters.indexOf(m_String[(TextOffset)m_CurrentPosition]) >= 0))
    {
        m_CurrentPosition++;
    }
}
//---------------------------------------------------------------------

UnicodeStringTokenizer::UnicodeStringTokenizer(UnicodeString & str, 
                                               UnicodeString & delim)                        
: m_CurrentPosition(0)                                            
{
    m_String = str;
    m_MaxPosition = m_String.size();
    m_StringDelimeters = delim;
}
//---------------------------------------------------------------------

UnicodeStringTokenizer::UnicodeStringTokenizer(UnicodeString & str)
: m_CurrentPosition (0)
{
    m_String = str;
    m_MaxPosition = m_String.size();
    m_StringDelimeters = JulianKeyword::Instance()->ms_sDEFAULT_DELIMS;
}

//---------------------------------------------------------------------

t_bool 
UnicodeStringTokenizer::hasMoreTokens()
{
    skipDelimeters();
    return (m_CurrentPosition < m_MaxPosition);
}

//---------------------------------------------------------------------

UnicodeString & 
UnicodeStringTokenizer::nextToken(UnicodeString & out,
                                  ErrorCode & status)
{
    t_int32 start;

    skipDelimeters();
    if (m_CurrentPosition >= m_MaxPosition)
    {
        status = 1;
        return out;
    }
    start = m_CurrentPosition;
    while ((m_CurrentPosition < m_MaxPosition) && 
           (m_StringDelimeters.indexOf(m_String[(TextOffset) m_CurrentPosition]) < 0)) 
    {
	    m_CurrentPosition++;
	}
	out = m_String.extractBetween(start, m_CurrentPosition, out);
    return out;
}

//---------------------------------------------------------------------

UnicodeString & 
UnicodeStringTokenizer::nextToken(UnicodeString & out, 
                                  UnicodeString & sDelim, 
                                  ErrorCode & status)
{
    status = ZERO_ERROR;
    m_StringDelimeters = sDelim;
    return nextToken(out, status);
}
//---------------------------------------------------------------------

