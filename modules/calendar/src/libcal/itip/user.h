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
 * user.h
 * John Sun
 * 3/10/98 2:28:01 PM
 */

#include <unistring.h>
#include "ptrarray.h"

#ifndef __USER_H_
#define __USER_H_

class User
{
private:
    /* For now store CAPI, IMIP, IRIP addresses as char **/
    UnicodeString m_CAPIAddress;
    t_int32 m_XItemID;

    UnicodeString m_IMIPAddress; /* usually e-mail address*/
    UnicodeString m_IRIPAddress; 

    UnicodeString m_RealName;

    /* temporary holding CS&T info, remove when CAP is ready */
    UnicodeString m_Password;
    UnicodeString m_Hostname;
    UnicodeString m_Node;

public:
    User();
    User (UnicodeString realName, UnicodeString imip, 
        UnicodeString capi = "", t_int32 xItemID = -1,
        UnicodeString irip = "");
    ~User() {}

    t_bool IsValidCAPI() const { return (m_CAPIAddress.size() > 0) && (m_XItemID != -1); }
    t_bool IsValidIRIP() const { return (m_IRIPAddress.size() > 0); }
    t_bool IsValidIMIP() const { return (m_IMIPAddress.size() > 0); }

    UnicodeString getIMIPAddress() const { return m_IMIPAddress; }
    UnicodeString getIRIPAddress() const { return m_IRIPAddress; }
    UnicodeString getCAPIAddress() const { return m_CAPIAddress; }
    t_int32 getXItemID() const { return m_XItemID; }


    /* TODO: temporarily hold CS&T info, remove when CAP is ready */
    void setCAPIInfo(UnicodeString password, UnicodeString hostname, UnicodeString node)
    {
        m_Password = password;
        m_Hostname = hostname;
        m_Node = node;
    }

    UnicodeString getPassword() const { return m_Password; }
    UnicodeString getHostname() const { return m_Hostname; }
    UnicodeString getNode() const { return m_Node; }

    /* end CS&T info */

    UnicodeString getRealName() const { return m_RealName; }

    /**
     * Prints summary of this User's private data info.
     *
     * @return          UnicodeString 
     */
    UnicodeString toString();
    

    /**
     * Return the CAPI x-string of this User.  Uses the MakeXString method. 
     *
     * @return          UnicodeString 
     */
    UnicodeString getXString();

    /**
     * Creates the CAPI X-string from the real-name argument in
     * the form "S=lastName*'/G=firstName*".  The lastName is the defined
     * to be all words after the first space.  
     * Thus if given the name John Van Der Wal, John would be the first
     * name and Van Der Wal would be the last name.  The generated
     * x-string would be "S=Van Der Wal/G=John".  It is recommended that
     * realname argument not have more than two words in a name.
     * An empty real name string returns "S=*'/G=*".
     * @param           out         generated x-string output
     *
     * @return          generated x-string output (out)
     */
    static UnicodeString & MakeXString(UnicodeString & realName, UnicodeString & out);

    /* static methods */

    /**
     * Cleanup method.  Delete each User element in the vector users.
     * @param           users       vector of users to delete from
     */
    static void deleteUserVector(JulianPtrArray * users);
};
#endif /* __USER_H_ */



