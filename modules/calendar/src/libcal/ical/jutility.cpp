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

#include "stdafx.h"

#include "jdefines.h"
#include <unistring.h>
#include "jutility.h"
#include "ptrarray.h"

//---------------------------------------------------------------------

JulianUtility::JulianUtility() {}

//---------------------------------------------------------------------

//t_int32 JulianUtility::atot_int32(const char * nPtr,
t_int32 JulianUtility::atot_int32(char * nPtr,
                                  t_bool & bParseError,
                                  t_int32 size)
{
    t_int32 i;
    char * ptr = nPtr;
    char c;
    for (i = 0; i < size; i++)
    {
        if (!isdigit(*ptr))
        {
            bParseError = TRUE; // or the ERROR to TRUE
            break;
        }
        ptr++;
    }
    c = nPtr[size];
    nPtr[size] = '\0';
    i = atol(nPtr);
    nPtr[size] = c;
    return i;
}
//---------------------------------------------------------------------
t_int32 JulianUtility::atot_int32(const char * nPtr,
                                  t_bool & bParseError,
                                  t_int32 size)
{
    t_int32 i;
    const char * ptr = nPtr;

    for (i = 0; i < size; i++)
    {
        if (!isdigit(*ptr))
        {
            bParseError |= TRUE; // or the ERROR to TRUE
            break;
        }
        ptr++;
    }
    return atol(nPtr);
}
//---------------------------------------------------------------------
t_bool JulianUtility::checkRange(t_int32 hashCode, JAtom range[],
                                 t_int32 rangeSize)
{
    t_int32 i;
    for (i = 0; i < rangeSize; i++)
    {
        if (range[i] == hashCode)
            return TRUE;
    }
    return FALSE;
}

//---------------------------------------------------------------------

void 
JulianUtility::stripDoubleQuotes(UnicodeString & u)
{
    if (u.size() > 0)
    {
        if (u[(TextOffset) 0] == '\"')
        {
            u.remove(0, 1);
        }
        if (u[(TextOffset) (u.size() - 1)] == '\"')
        {
            u.remove(u.size() - 1, 1);
        }
    }
}

//---------------------------------------------------------------------

UnicodeString &
JulianUtility::addDoubleQuotes(UnicodeString & us)
{
    if (us.size() != 0)
    {
        us += '\"';
        us.insert(0, "\"");
    }
    return us;
}

//---------------------------------------------------------------------
/*
char * JulianUtility::unistrToStr(const UnicodeString & us)
{
    return us.toCString("");
}
*/
//---------------------------------------------------------------------
/*
UnicodeString & JulianUtility::ToUpper(const UnicodeString & eightBitString)
{
    t_int32 i;
    //
    char c;
    for (i = 0; i < eightBitString.size(); i++)
    {
        c = (char) eightBitString[(TextOffset) i];
        if (islower(c))
        {
            c = (char) toupper(c);
            eightBitString.replace((TextOffset) i, 1, &c);
        }
    }
    //
    char * c = eightBitString.toCString("");
    for (i = 0; i < eightBitString.size(); i++)
    {
        if (islower(c[i]))
        {
            c[i] = (char) toupper(c[i]);
        }
    }
    eightBitString = c;
    return eightBitString;
}
*/
//---------------------------------------------------------------------

/*
void JulianUtility::TRACE_DateTimeVector(JulianPtrArray & vector)
{
      for (i = 0; i < vector.GetSize(); i++)         
      {
          DateTime * dt = (DateTime *) dateVector.GetAt(i); 
          if (TRUE) TRACE("dtVec[%d] = %s\r\n", i, dt->toISO8601().toCString(""));
      }
}
*/
//---------------------------------------------------------------------


