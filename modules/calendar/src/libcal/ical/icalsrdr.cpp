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
 * icalsrdr.cpp
 * John Sun
 * 2/10/98 11:37:56 PM
 */

/* TODO: remove Unistring dependency.  There is a bug if the
target string is encoded with 2byte character.  If so, then the
m_pos and m_length variables are wrong.  Currently, I will assume that
all const char * passed in will be us-ascii 8-bit chars
*/

#include "stdafx.h"
#include "jdefines.h"

#include <string.h>
#include "ptypes.h"
#include "icalsrdr.h"

//---------------------------------------------------------------------

ICalStringReader::ICalStringReader() {}

//---------------------------------------------------------------------

ICalStringReader::ICalStringReader(const char * string, 
                                   JulianUtility::MimeEncoding encoding)
: m_string(string), m_pos(0), m_mark(0)
{
    m_length = strlen(m_string);
    m_unistring = m_string;
    m_Encoding = encoding;
}

//---------------------------------------------------------------------

t_int8 ICalStringReader::read(ErrorCode & status)
{
    if (m_pos >= m_length)
    {
        status = 1;
        return -1;
    }
    else
    {
        status = ZERO_ERROR;
        if (m_Encoding == JulianUtility::MimeEncoding_7bit)
        {
            return m_string[m_pos++];
        }
        else
        {
            // for now only handles quoted-printable
            PR_ASSERT(m_Encoding == JulianUtility::MimeEncoding_QuotedPrintable);
            if (m_Encoding == JulianUtility::MimeEncoding_QuotedPrintable)
            {
                if ((m_string[m_pos] == '=') && (m_length >= m_pos + 3))
                {
                    // TODO: use libmime decoding algorithm instead of this one                    
                    if (ICalReader::isHex(m_string[m_pos+1]) && ICalReader::isHex(m_string[m_pos + 2]))
                    {
                        t_int8 c;
                        c = ICalReader::convertHex(m_string[m_pos + 1], m_string[m_pos + 2]);
                        m_pos += 3;
                        return c;
                    }
                    else
                    {
                        return m_string[m_pos++];
                    }
                }
                else
                {
                    return m_string[m_pos++];
                }
            }
            else
            {
                // handle like 7bit
                return m_string[m_pos++];
            }
        }
    }
}

//---------------------------------------------------------------------

void ICalStringReader::mark()
{
    m_mark = m_pos;
}

//---------------------------------------------------------------------

void ICalStringReader::reset()
{
    m_pos = m_mark;
}

//---------------------------------------------------------------------

UnicodeString & ICalStringReader::readLine(UnicodeString & aLine,
                                           ErrorCode & status)
{
    status = ZERO_ERROR;
    t_int8 c = 0;
    t_int32 oldpos = m_pos;

    aLine = "";
    c = read(status);
    while (!(FAILURE(status)))
    {    
        /* Break on '\n', '\r\n', and '\r' */
        if (c == '\n')
        {
            break;
        }
        else if (c == '\r')
        {
            mark();
            c = read(status);
            if (FAILURE(status))
                break;
            else if (c == '\n')
            {
                break;
            }
            else
            {
                reset();
                break;
            }
        }
#if 1
        aLine += c;
#endif
        c = read(status);
    }
#if 0
    if (m_pos > oldpos)
        m_unistring.extractBetween(oldpos, m_pos - 1, aLine);
    else
        aLine = "";
#endif
    

    //if (FALSE) TRACE("end of readline:---%s---\r\n", aLine.toCString(""));
    return aLine;
}

//---------------------------------------------------------------------

UnicodeString & ICalStringReader::readFullLine(UnicodeString & aLine,
                                               ErrorCode & status, t_int32 iTemp)
{
    status = ZERO_ERROR;
    t_int32 i;
    
    readLine(aLine, status);
    //if (FALSE) TRACE("rfl(1) %s\r\n", aLine.toCString(""));

    if (FAILURE(status))
    {
        //aLine = "";
        return aLine;
    }
    
    UnicodeString aSubLine;

    while (TRUE)
    {
        mark();
        i = read(status);
        if (i == ' ')
        {
            aLine += readLine(aSubLine, status);
            //if (FALSE) TRACE("rfl(2) %s\r\n", aLine.toCString(""));
        }
        else if (FAILURE(status))
        {
            return aLine;
        }
        else
        {
            reset();
            break;
        }
    }
    //if (FALSE) TRACE("end of rfl: ---%s---\r\n", aLine.toCString(""));
    return aLine;
}
//---------------------------------------------------------------------

                                                

