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

// txnobj.cpp
// John Sun
// 11:36 AM March 10, 1998

#include "stdafx.h"

#include "jdefines.h"
#include "txnobj.h"
#include "datetime.h"

#if CAPI_READY
#include <capi.h>
#endif /* #if CAPI_READY */

#include "vevent.h"
#include "jlog.h"
#include "icalsrdr.h"
#include "keyword.h"

//---------------------------------------------------------------------

TransactionObject::TransactionObject()
: m_Calendar(0), m_ICalComponentVctr(0), m_Modifiers(0)
{
}

//---------------------------------------------------------------------

TransactionObject::TransactionObject(NSCalendar & cal,
                                     JulianPtrArray & components,
                                     User & user,
                                     JulianPtrArray & recipients,
                                     UnicodeString & subject,
                                     JulianPtrArray & modifiers,
                                     JulianForm * jf,
                                     MWContext * context,
                                     UnicodeString & attendeeName,
                                     EFetchType fetchType)
: m_Modifiers(0)
{
    m_Calendar = &cal;

    // assumption is that components are processed.
    m_ICalComponentVctr = &components;
    m_User = &user;
    m_Recipients = &recipients;
    m_Subject = subject;

    m_Modifiers = &modifiers;

    m_AttendeeName = attendeeName;
    m_FetchType = fetchType;

    // for sending IMIP messages
    m_JulianForm = jf;
    m_Context = context;
}
//---------------------------------------------------------------------
TransactionObject::~TransactionObject()
{

}
//---------------------------------------------------------------------

void
TransactionObject::execute(JulianPtrArray * out, 
                           ETxnErrorCode & status)
{
    PR_ASSERT(m_User != 0 && m_Recipients != 0);
    status = TR_OK;
    
    //t_int32 status;
    
    // split up recipients
    /*
    JulianPtrArray * m_CAPIRecipients = new JulianPtrArray();
    JulianPtrArray * m_IRIPRecipients = new JulianPtrArray();
    JulianPtrArray * m_IMIPRecipients = new JulianPtrArray();
    JulianPtrArray * m_OtherRecipients = new JulianPtrArray();
    t_int32 i;
    User * u;
    for (i = 0; i < m_Recipients->GetSize(); i++)
    {
        u = (User *) m_Recipients->GetAt(i);
        if (u->IsValidCAPI())
        {
            m_CAPIRecipients->Add(u);
        }
        else if (u->IsValidIRIP())
        {
            m_IRIPRecipients->Add(u);
        }
        else if (u->IsVaildIMIP())
        {
            m_IMIPRecipients->Add(u);
        }
        else
        {
            // Add to other bin
            m_OtherRecipients->Add(u);
        }
    }
    */

    t_int32 size = 0;
    
    if (m_Recipients != 0)
    {
        size = m_Recipients->GetSize();
    }
    t_bool * recipientStatus = new t_bool[size];
    
    //if (status != TR_OK)
    //{

    // TRY ALL CAPI
    //executeCAPI(m_CAPIRecipients, status);
    //executeCAPI(m_Recipients, status);
    //executeCAPI(out, status);
    //handleCAPI();

    // TRY ALL IRIP
    //executeIRIP(m_IRIPRecipients, status);
    //executeIRIP(m_Recipients, status);

    // TRY IMIP
    //executeIMIP(m_IMIPRecipients, status);
    //executeIMIP(m_Recipients, status);
    executeIMIP(out, status);
    delete [] recipientStatus; recipientStatus = 0;
}
//---------------------------------------------------------------------

void
TransactionObject::executeIRIP(JulianPtrArray * out, 
                               ETxnErrorCode & status)
{
    PR_ASSERT(FALSE);
    // NOTE: remove later to avoid compiler warnings
    if (status && out != 0);
    /* NOT YET DEFINED */
}

//---------------------------------------------------------------------

void
TransactionObject::executeIMIP(JulianPtrArray * out, 
                               ETxnErrorCode & status)
{
    UnicodeString itipMessage;
    UnicodeString sContentTypeHeader;
    // prints FROM: User and TO: users
    UnicodeString sCharSet = "UTF-8";
    UnicodeString sMethod;
    UnicodeString sComponentType;

    t_int32 i;
    User * u;
    PR_ASSERT(m_User != 0 && m_Recipients != 0);
    if (m_User != 0 && m_Recipients != 0)
    {
        for (i = 0; i < m_Recipients->GetSize(); i++)
        {
            u = (User *) m_Recipients->GetAt(i);
        }
    } 
    // Create the body of the message
    itipMessage = MakeITIPMessage(itipMessage);

    // Create the content header    
    PR_ASSERT(m_Calendar != 0);
    if (m_Calendar != 0)
    {
        sMethod = NSCalendar::methodToString(m_Calendar->getMethod(), sMethod);
    }
    PR_ASSERT(m_ICalComponentVctr != 0 && m_ICalComponentVctr->GetSize() > 0);
    if (m_ICalComponentVctr != 0 && m_ICalComponentVctr->GetSize() > 0)
    {
        // Use first component in m_ICalComponentVctr to get type
        sComponentType = 
            ICalComponent::componentToString(((ICalComponent *)m_ICalComponentVctr->GetAt(0))->GetType());
    }
    sContentTypeHeader = "Content-Type: text/calendar; method=";
    sContentTypeHeader += sMethod;
    sContentTypeHeader += "; charset=";
    sContentTypeHeader += sCharSet;
    sContentTypeHeader += "; component=";
    sContentTypeHeader += sComponentType;

//#ifdef DEBUG_ITIP
    m_DebugITIPMessage = itipMessage;
//#endif /* DEBUG_ITIP */
    
    // TODO: send it via mail API here
    if (m_JulianForm != 0 && m_Context != 0 && m_Recipients != 0)
    {
        int iOut;
        User * userTo;
        char * to = NULL;
        char * from = m_User->getIMIPAddress().toCString("");
        char * subject = m_Subject.toCString("");
        char * body = itipMessage.toCString("");
        char * otherheaders = sContentTypeHeader.toCString("");

        // TODO: this needs to be fixed, I have to loop to send each message
        // rather than sending one message.
        for (i = 0; i < m_Recipients->GetSize(); i++)
        {
            userTo = (User *) m_Recipients->GetAt(i);
            to = userTo->getIMIPAddress().toCString("");
            iOut = (*m_JulianForm->getCallbacks()->SendMessageUnattended)(m_Context, to, subject,
                otherheaders, body);
        }
    }
}

//---------------------------------------------------------------------

UnicodeString &
TransactionObject::MakeITIPMessage(UnicodeString & out)
{
    // first print header, then print components
    UnicodeString s, sName, sMethod;
    t_bool isRecurring = FALSE;

    PR_ASSERT(m_Calendar != 0);
    if (m_Calendar != 0)
    {
        out = "BEGIN:VCALENDAR\r\n";
        out += m_Calendar->createCalendarHeader(s);
        sMethod = NSCalendar::methodToString(m_Calendar->getMethod(), sMethod);

        sName = m_AttendeeName;

        out += printComponents(m_ICalComponentVctr, sMethod, 
            sName, isRecurring, s);
        out += "END:VCALENDAR\r\n";
    }
    return out;
}
//---------------------------------------------------------------------

UnicodeString &
TransactionObject::printComponents(JulianPtrArray * components,
                                   UnicodeString & sMethod,
                                   UnicodeString & sName,
                                   t_bool isRecurring, 
                                   UnicodeString & out)
{
    out = "";
    PR_ASSERT(components != 0 && components->GetSize() > 0);
    if (components != 0 && components->GetSize() > 0)
    {
        t_int32 i;
        ICalComponent * ic;
        DateTime dtStamp; 

        for (i = 0; i < components->GetSize(); i++)
        {
            ic = (ICalComponent *) components->GetAt(i);
            out += ic->toICALString(sMethod, sName, isRecurring);
        }
    }
    return out;
}

//---------------------------------------------------------------------
#if CAPI_READY
#if 0
void 
TransactionObject::setCAPIInfo(char * userxstring,
                               char * password,
                               char * hostname,
                               char * node,
                               char * recipientxstring)
{
    m_userxstring = userxstring;
    m_password = password;
    m_hostname = hostname;
    m_node = node;
    m_recipientxstring = recipientxstring;
}
#endif
//---------------------------------------------------------------------

void
TransactionObject::executeCAPI(JulianPtrArray * outCalendars, 
                               ETxnErrorCode & status)
{
    pCAPISession pS = NULL;
    pCAPIHandle pH;
    char ** strings;
    char * eventFetchedByUID;
    t_int32 numstrings;
    t_int32 i;

    if (m_User != 0 && m_Recipients != 0 && m_Recipients->GetSize() > 0)
    {
        char sDebugOut[100];
        int handleCount = m_Recipients->GetSize();
        PR_ASSERT(outCalendars != 0);
        
        CAPIStatus cStat = CAPI_ERR_OK;
        User * aUser;
        char * xstring;

        // Execute specific CAPI call (fetch, store, delete)
        EFetchType outType;

        // TODO: horrible hack, restricting number of handles to 20
        pCAPIHandle handles[20]; // wish I could do handles[handleCount]
        if (handleCount > 20) 
            handleCount = 20;

        for (i = 0; i < handleCount; i++)
        {
            handles[i] = NULL;
        }

        // Capabilities       
        //if (FALSE) TRACE("CAPICapabilities:%s\r\n", CAPI_Capabilities((long) 0));
        //sprintf(sDebugOut, "CAPICapabilities:%s\r\n", CAPI_Capabilities((long) 0));
        
        // Logon

        // TODO: eventually use the CAPAddress
        char * userxstring = m_User->getXString().toCString("");        
        char * password = m_User->getPassword().toCString("");
        char * hostname = m_User->getHostname().toCString("");
        char * node = m_User->getNode().toCString("");
        TRACE("logon user: %s,\r\n password: %s,\r\n hostname: %s,\r\n node: %s,\r\n", 
            userxstring, password, hostname, node);
        
        
        cStat = CAPI_Logon(userxstring, password, hostname, node, 0, &pS);
        
        //if (FALSE) TRACE("CAPI_Logon:%lx\r\n", foo);
        //sprintf(sDebugOut, "CAPI_Logon:%lx\r\n", foo);

        if (cStat != CAPI_ERR_OK)
        {
            status = TR_CAPI_LOGIN_FAILED;
        }
        else
        {

            // Get Handle for each recipient
            for (i = 0; i < handleCount; i++)
            {
                aUser = (User *) m_Recipients->GetAt(i);
                xstring = aUser->getXString().toCString("");
                CAPI_GetHandle(pS, xstring, 0, &handles[i]);
            }
            //if (FALSE) TRACE("CAPI_GetHandle:%lx\r\n", foo);
            //sprintf(sDebugOut, "CAPI_GetHandle:%lx\r\n", foo);

            // HandleCAPI call
            // OLD WAY
            //foo = handleCAPI(pS, pH, m_Modifiers, &strings, 
            //    &eventFetchedByUID, numstrings, outType);
            //
           cStat = handleCAPI(pS, &handles[0], handleCount, 0, m_ICalComponentVctr, m_Calendar,
                m_Modifiers, outCalendars, outType);

            if (cStat != CAPI_ERR_OK)
            {
                if (status == TR_OK)
                {
                    status = TR_CAPI_HANDLE_CAPI_FAILED;
                }
            }

            // Logoff
            cStat = CAPI_Logoff(&pS, (long) 0);
            if (cStat != CAPI_ERR_OK)
            {
                if (status == TR_OK)
                {
                    status = TR_CAPI_LOGOFF_FAILED;
                }
            }
            //if (FALSE) TRACE("CAPI_GetLogoff:%lx\r\n", foo);
            //sprintf(sDebugOut, "CAPI_GetLogoff:%lx\r\n", status);

            // Destroy Handle
            //foo = CAPI_DestroyHandle(&pH);
            CAPI_DestroyHandles(&handles[0], handleCount, 0);
        }
    }
}
#endif /* #if CAPI_READY */
