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
 * sttxnobj.cpp
 * John Sun
 * 4/13/98 10:38:50 AM
 */

#include "stdafx.h"
#include "sttxnobj.h"
#include "txnobj.h"
#if CAPI_READY
#include <capi.h>
#endif /* CAPI_READY */

//---------------------------------------------------------------------

StoreTransactionObject::StoreTransactionObject()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

StoreTransactionObject::StoreTransactionObject(NSCalendar & cal, 
                                               JulianPtrArray & components,
                                               User & user, 
                                               JulianPtrArray & recipients, 
                                               UnicodeString & subject,
                                               JulianPtrArray & modifiers,
                                               JulianForm * jf,
                                               MWContext * context,
                                               UnicodeString & attendeeName)
: TransactionObject(cal, components, user, recipients, subject,
                    modifiers, 
                    jf, context,
                    attendeeName)                                           
{
}

//---------------------------------------------------------------------
#if CAPI_READY

int stringRead(void * pData, char * pBuf, int iSize, int* piTransferred)
{
    CAPI_CTX* pCtx = (CAPI_CTX*)pData;  
    *piTransferred = (pCtx->iSize > iSize) ? iSize : pCtx->iSize; 
    memcpy(pBuf,pCtx->p,*piTransferred); 
    pCtx->iSize -= *piTransferred; 
    pCtx->p += *piTransferred; 
    return pCtx->iSize > 0 ? 0 : -1; 
}

//---------------------------------------------------------------------

CAPIStatus 
StoreTransactionObject::handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags, 
        JulianPtrArray * inComponents, NSCalendar * inCal,
        JulianPtrArray * modifiers, 
        JulianPtrArray * outCalendars, TransactionObject::EFetchType & out)
{
    CAPIStatus status;
    pCAPIStream outStream = NULL;
    char ** uids = NULL;
    long outSize;
    t_int32 i;
    char * pBuf;  

    PR_ASSERT(inComponents != 0 && inCal != 0);
      
#if 0

    // For now
    // first make the MIME-message
    //char pBuf[3000];

    // works
    //strcpy(pBuf, "Mime-Version: 1.0\nContent-Type:text/calendar; method=PUBLISH; charset=US-ASCII\nContent-Transfer-Encoding: 7bit\n\nBEGIN:VCALENDAR\r\nMETHOD:PUBLISH\r\nPRODID:-//ACME/DesktopCalendar//EN\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nORGANIZER:mailto:a@example.com\r\nDTSTART:19980508T200000Z\r\nDTEND:19980508T210000Z\r\nDTSTAMP:19970611T190000Z\r\nSUMMARY:ST. PAUL SAINTS -VS- DULUTH-SUPERIOR DUKES\r\nUID:atestUID19970505T112233Z-303434@jsun.netscape.com\r\nCOMMENT:A comment with no params\r\nEND:VEVENT\r\nEND:VCALENDAR\n");
    
    // doesn't work
    //strcpy(pBuf, "Mime-Version: 1.0\nContent-Type:text/calendar; method=PUBLISH; charset=US-ASCII\nContent-Transfer-Encoding: 7bit\n\nBEGIN:VCALENDAR\r\nMETHOD:PUBLISH\r\nPRODID:-//ACME/DesktopCalendar//EN\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nORGANIZER:mailto:a@example.com\r\nDTSTART:19980508T200000Z\r\nDTEND:19980508T210000Z\r\nDTSTAMP:19980611T190000Z\r\nATTENDEE;ROLE=REQ-PARTICIPANT;RSVP=FALSE;EXPECT=FYI;CUTYPE=INDIVIDUAL:MAILTO\r\n :a@acme.com\r\nATTENDEE;ROLE=REQ-PARTICIPANT;RSVP=FALSE;EXPECT=FYI;CUTYPE=INDIVIDUAL:MAILTO\r\n :hello\r\nCLASS:PUBLIC\r\nCOMMENT:A comment with no params\r\nCOMMENT;LANGUAGE=us_en:comment with language only\r\nCOMMENT;VALUE=BINARY:a comment with a bad param\r\nDESCRIPTION:A VEvent with a DTStart having a TZID of America-New_York\r\nDURATION:P0Y0M0DT0H0M0S\r\nRELATED-TO:relatedto1@jdoe.com\r\nRELATED-TO:relatedto2@jdoe.com\r\nREQUEST-STATUS:2.02;Success, invalid property ignored.. ATTENDEE\r\nSEQUENCE:0\r\nSUMMARY:A VEvent with TZID DTStart\r\nUID:001jsun@netscape.com\r\nURL:http://www.ur12.com\r\nEND:VEVENT\r\nEND:VCALENDAR\n");

    // works
    //strcpy(pBuf, "Mime-Version: 1.0\nContent-Type:text/calendar; method=PUBLISH; charset=US-ASCII\nContent-Transfer-Encoding: 7bit\n\nBEGIN:VCALENDAR\r\nMETHOD:PUBLISH\r\nPRODID:-//ACME/DesktopCalendar//EN\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nATTENDEE:mailto:jsun@netscape.com\r\nORGANIZER:mailto:a@example.com\r\nDTSTART:19980508T200000Z\r\nDTEND:19980508T210000Z\r\nDTSTAMP:19980611T190000Z\r\nSUMMARY:A VEvent with TZID DTStart\r\nUID:001jsun@netscape.com\r\nCLASS:PUBLIC\r\nCOMMENT:A comment with no params\r\nSEQUENCE:0\r\nURL:http://www.ur12.com\r\nEND:VEVENT\r\nEND:VCALENDAR\n");
    
    // works
    //UnicodeString u = "Mime-Version: 1.0\nContent-Type:text/calendar; method=PUBLISH; charset=US-ASCII\nContent-Transfer-Encoding: 7bit\n\nBEGIN:VCALENDAR\r\nMETHOD:PUBLISH\r\nPRODID:-//ACME/DesktopCalendar//EN\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nATTENDEE:mailto:jsun@netscape.com\r\nORGANIZER:mailto:a@example.com\r\nDTSTART:19980508T200000Z\r\nDTEND:19980508T210000Z\r\nDTSTAMP:19980611T190000Z\r\nSUMMARY:A VEvent with TZID DTStart\r\nUID:001jsun@netscape.com\r\nCLASS:PUBLIC\r\nCOMMENT:A comment with no params\r\nSEQUENCE:0\r\nURL:http://www.ur12.com\r\nEND:VEVENT\r\nEND:VCALENDAR\n";
   
    UnicodeString u = "Mime-Version: 1.0\nContent-Type:text/calendar; method=PUBLISH; charset=US-ASCII\nContent-Transfer-Encoding: 7bit\n\nBEGIN:VCALENDAR\r\nMETHOD:PUBLISH\r\nPRODID:-//ACME/DesktopCalendar//EN\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nORGANIZER:MAILTO:organizer\r\nCLASS:PRIVATE\r\nCOMMENT:A VEvent\r\nDESCRIPTION:A VEvent with a DTStart having a TZID of America-New_York\r\nDURATION:P0Y0M0DT0H22M33S\r\nDTSTART:19980508T210000Z\r\nDTEND:19980508T212233Z\r\nDTSTAMP:19980303T200000Z\r\nLOCATION:A location\r\nSEQUENCE:0\r\nSUMMARY:A VEvent\r\nUID:002jsun@netscape.com\r\nEND:VEVENT\r\nEND:VCALENDAR\n";
    pBuf = u.toCString("");
    TRACE("u = %s", pBuf);
#else
    UnicodeString method, version;
    method = "PUBLISH";
    version = "2.0";
    if (inCal != 0)
    {
        method = NSCalendar::methodToString(inCal->getMethod(), method);
        if (inCal->getVersion().size() > 0)
            version = inCal->getVersion();
    }

    UnicodeString u2 = "Mime-Version: 1.0\nContent-Type:text/calendar; method=";
    u2 += method;
    u2 += "; charset=US-ASCII\nContent-Transfer-Encoding: 7bit\n\n";
    u2 += "BEGIN:VCALENDAR\r\nMETHOD:";
    u2 += method;
    u2 += "\r\n";
    u2 += "VERSION:";
    u2 += version; 
    u2 += "\r\n";
    u2 += "PRODID:-//ACME/DesktopCalendar//EN\r\n";
    
    // TODO: NOTE: doens't seem to handle DTSTART;TZID=A:19971122T112233 
    PR_ASSERT(inComponents != 0);
    if (inComponents != 0) 
    {
        for (i = 0; i < inComponents->GetSize(); i++)
        {
            ICalComponent * ic = (ICalComponent *) inComponents->GetAt(i);
            u2 += ic->toICALString();
        }
    }

    u2 += "END:VCALENDAR\n";
    pBuf = u2.toCString("");
    //if (FALSE) TRACE("u2 = %s", pBuf);
#endif

    m_Ctx.p = pBuf;
    m_Ctx.iSize = strlen(pBuf);
    
    status = CAPI_SetStreamCallbacks(&outStream, stringRead, &m_Ctx, NULL, NULL, 0);
       
    // TODO: if server is down, this may take forever!!!
    status = CAPI_StoreEvent(pS, pH, iHandleCount, 0, outStream);
    if (TRUE) TRACE("CAPI_StoreEvent:%lx\r\n", status);

    //TODO: the store event now has a CS&T UID.  What to do with it?
    status = CAPI_GetLastStoredUIDs(pS, &uids, &outSize, 0);
    
    if (TRUE) TRACE("Saved %d UIDs.\r\n", outSize);
    for (i = 0; i < outSize; i++)
    {
        if (TRUE) TRACE("%s\r\n", *(uids + i));
    }

    return status;
}

//---------------------------------------------------------------------

#endif /* #if CAPI_READY */
