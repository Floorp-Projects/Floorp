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

// user.cpp
// John Sun
// 3/10/98 2:58:33 PM

#include "stdafx.h"
#include "jdefines.h"
#include "unistrto.h"
#include "user.h"

//---------------------------------------------------------------------

User::User()
{
}

//---------------------------------------------------------------------

User::User(UnicodeString realName, UnicodeString imip, 
        UnicodeString capi, t_int32 xItemID,
        UnicodeString irip)
{
    m_RealName = realName;
    m_CAPIAddress = capi;
    m_IRIPAddress = irip;
    m_IMIPAddress = imip;
    m_XItemID = xItemID;
}


//---------------------------------------------------------------------

UnicodeString
User::toString()
{
    char sNum[20];
    sprintf(sNum, "%d", m_XItemID);
    UnicodeString out;
    out += "realname: "; 
    out += m_RealName;
    out += "\r\nCAPI Addr.: ";
    out += m_CAPIAddress;
    out += " XItemID: ";
    out += sNum;
    out += "\r\nIRIP Addr.: ";
    out += m_IRIPAddress;
    out += "\r\nIMIP Addr.: ";
    out += m_IMIPAddress;
    return out;
}

//---------------------------------------------------------------------

UnicodeString
User::getXString()
{
    UnicodeString out;
    return MakeXString(m_RealName, out);
}

//---------------------------------------------------------------------

UnicodeString &
User::MakeXString(UnicodeString & realName, UnicodeString & out)
{
    out = "";
    UnicodeStringTokenizer * st;
    UnicodeString u = " ";
    st = new UnicodeStringTokenizer(realName, u);
    if (st != 0)
    {
        UnicodeString sirName, givenName;
        if (st->hasMoreTokens())
        {
            ErrorCode status = ZERO_ERROR;
            givenName = st->nextToken(u, status);
            while (st->hasMoreTokens())
            {
                // last name may have spaces
                sirName += st->nextToken(u, status);
            }

        }
        out += "S=";
        out += sirName;
        out += "/G=";
        out += givenName;
        delete st; st = 0;
    }
    return out;
}

//---------------------------------------------------------------------

void
User::deleteUserVector(JulianPtrArray * users)
{
    t_int32 i;
    if (users != 0)
    {
        for (i = users->GetSize() - 1; i >= 0; i--)
        {
            delete ((User *) users->GetAt(i));
        }
    }
}

//---------------------------------------------------------------------
