/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
#if CAPI_READY
    m_Session = 0;
#endif
}

//---------------------------------------------------------------------

User::~User()
{
}

//---------------------------------------------------------------------

User::User(UnicodeString realName, UnicodeString imip)
{
    m_RealName = realName;
    m_IMIPAddress = imip;
    m_CAPIAddress = "";
    m_IRIPAddress = "";
    m_XItemID = -1;     
#if CAPI_READY
    m_Session = 0;
#endif
}

//---------------------------------------------------------------------

User::User(UnicodeString realName, UnicodeString imip, 
        UnicodeString capi, UnicodeString irip, 
        t_int32 xItemID)
{
    m_RealName = realName;
    m_CAPIAddress = capi;
    m_IRIPAddress = irip;
    m_IMIPAddress = imip;
    m_XItemID = xItemID;
#if CAPI_READY
    m_Session = 0;
#endif
}

//---------------------------------------------------------------------

User::User(User & that)
{
    m_CAPIAddress = that.m_CAPIAddress;
    m_IMIPAddress = that.m_IMIPAddress;
    m_IRIPAddress = that.m_IRIPAddress;
    m_RealName = that.m_RealName;
    m_LoginName = that.m_LoginName ;  
    m_Password = that.m_Password;
    m_Hostname = that.m_Hostname;
    m_Node = that.m_Node;
#if CAPI_READY
    m_Session = 0;  // can't copy session's
#endif
}
//---------------------------------------------------------------------

User *
User::clone()
{
    return new User(*this);
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
    if (m_LoginName.size() > 0)
    {
        return MakeXString(m_LoginName, out);
    }
    else 
        return MakeXString(m_RealName, out);
}

//---------------------------------------------------------------------

UnicodeString
User::getLogonString()
{
    UnicodeString out;
    if (m_LoginName.size() > 0)
    {
        return MakeCAPILogonString(m_LoginName, m_Node, out);
    }
    else 
        return MakeCAPILogonString(m_RealName, m_Node, out);
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

UnicodeString & 
User::MakeCAPILogonString(UnicodeString & realName, 
                          UnicodeString & node, 
                          UnicodeString & out)
{
    MakeXString(realName, out);
    out.insert(0, ":/");
    out += "/ND=";
    out += node;
    out += "/";
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

void
User::cloneUserVector(JulianPtrArray * toClone,
                      JulianPtrArray * out)
{
    if (out != 0)
    {
        if (toClone != 0)
        {
            t_int32 i;
            User * comp;
            User * clone;
            for (i = 0; i < toClone->GetSize(); i++)
            {
                comp = (User *) toClone->GetAt(i);
                clone = comp->clone();
                out->Add(clone);
            }
        }
    }
}
//---------------------------------------------------------------------

