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
 * getxnobj.cpp
 * John Sun
 * 4/1/98 5:16:29 PM
 */

#include "stdafx.h"
#include "getxnobj.h"
#include "nspr.h"

#if CAPI_READY
#include <capi.h>
#include "jparser.h"
#include "icalsrdr.h"
#include "nscal.h"
#include "capiredr.h"
//#define TEST_STRINGREADER_PARSE 1
#endif /* #if CAPI_READY */

//PRMonitor * GetTransactionObject::m_Monitor = 0;
//---------------------------------------------------------------------

GetTransactionObject::GetTransactionObject()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

GetTransactionObject::GetTransactionObject(NSCalendar & cal, 
                                           JulianPtrArray & components,
                                           User & user, 
                                           JulianPtrArray & recipients, 
                                           UnicodeString & subject,
                                           JulianPtrArray & modifiers,
                                           JulianForm * jf,
                                           MWContext * context,
                                           UnicodeString & attendeeName,
                                           EFetchType type)
: TransactionObject(cal, components, user, recipients, subject,
                    modifiers, 
                    jf, context,
                    attendeeName, type)                                           
{
#if CAPI_READY
    m_Monitor = 0;
#endif
}

//---------------------------------------------------------------------
#if CAPI_READY

/*
int writeToCAPIReader(void * pData, char * pBuf, 
                      int iSize, int * piTransferred)
{
    int i;
    for(i = 0; i < iSize; i++)
    {
        // TODO: put each char into new char * in capireader
        //putc( *(pBuf++), (void *) pData);
    }
    *piTransferred = i;

    return CAPI_CALLBACK_CONTINUE;
}
*/

//---------------------------------------------------------------------

int writeToBufferDebug(void * pData, char * pBuf,
                       int iSize, int * piTransferred)
{
    CAPI_CTX * pCtx = (CAPI_CTX*)pData;
    *piTransferred = (pCtx->iSize > iSize) ? iSize: pCtx->iSize;
    memcpy(pCtx->p, pBuf, *piTransferred);
    pCtx->iSize -= *piTransferred;
    pCtx->p += *piTransferred;
    return pCtx->iSize > 0 ? 0 : -1;
}

//---------------------------------------------------------------------

int getransactionobj_writeToCAPIReader(void * pData, char * pBuf, int iSize, int * piTransferred)
{
    //PR_EnterMonitor(GetTransactionObject::getMonitor());
#if TESTING_ITIPRIG
    TRACE("\t\t--writeToCAPIReader: entered method (transferring %d chars) \r\n", iSize);
#endif
    //ICalCAPIReader * pReader = (ICalCAPIReader *) pData;
    JulianParser * pJP = (JulianParser *) pData;
    ICalCAPIReader * pReader = pJP->getReader();
    //PR_EnterMonitor((PRMonitor *)(pReader->getMonitor()));
    if (!pJP->isParseStarted())
    {
        PRThread * pThread = pJP->getThread();

#if TESTING_ITIPRIG
        TRACE("\t\t--writeToCAPIReader: starting parseThread\r\n");
#endif
        pJP->setParseStarted();
        PR_Start(pThread, jparser_ParseCalendarsZero, pJP, NULL);
        //PR_Start(parseThread, jparser_ParseCalendars, (void *) (capiReader), (void *) (outCalendars));
    }

    // enter monitor
    PR_EnterMonitor((PRMonitor *)pReader->getMonitor());
#if TESTING_ITIPRIG
    TRACE("\t\t--writeToCAPIReader: in monitor\r\n");
#endif

    *piTransferred = iSize;
    // todo: this assumes pBuf is null-terminated
    pReader->AddChunk(new UnicodeString(pBuf));
    if (iSize < 500)
    {
#if TESTING_ITIPRIG
        TRACE("\t\t--writeToCAPIReader: added chunk -%s- \r\n", pBuf);
#endif

    }
    else
    {
#if TESTING_ITIPRIG
        TRACE("\t\t--writeToCAPIReader: added chunk bigger that 500 chars\r\n");
#endif
    }
    //PR_Wait((PRMonitor *)(pReader->getMonitor()), LL_MAXINT);
   
     //if finished, set it to TRUE
    if (iSize <= 0)
    {
        pReader->setFinished();
#if TESTING_ITIPRIG
        TRACE("\t\t--writeToCAPIReader: has no more chunks to add.\r\n");
#endif
    }

    //PR_ExitMonitor((PRMonitor *)(pReader->getMonitor()));
   
    // notify???
#if TESTING_ITIPRIG
    TRACE("\t\t--writeToCAPIReader: notifying parseThread that more chunks added\r\n");
#endif
    PR_Notify((PRMonitor *)pReader->getMonitor());

#if TESTING_ITIPRIG
    TRACE("\t\t--writeToCAPIReader: leaving monitor\r\n");
#endif
    PR_ExitMonitor((PRMonitor *)pReader->getMonitor());
#if TESTING_ITIPRIG
    TRACE("\t\t--writeToCAPIReader: yielding\r\n");
#endif
    PR_Yield();
    // exit monitor
    return iSize > 0 ? 0 : -1;
}

//---------------------------------------------------------------------
/*
int GetTransactionObject::writeToCAPIReader(void * pData, char * pBuf,
                                        int iSize, int * piTransferred)
{
    int retVal;
    retVal = writeToCAPIReaderHelper(pData, pBuf, iSize, piTransferred);
    /*
    PR_ASSERT(m_CAPIReader != 0);
    if (m_CAPIReader != 0)
    {
        m_CAPIReader->addNewChunk(pData, *piTransferred);
    }
    
    return retVal;
}
*/
//---------------------------------------------------------------------

CAPIStatus 
GetTransactionObject::handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags, 
        JulianPtrArray * inComponents, NSCalendar * inCal,
        JulianPtrArray * modifiers, 
        JulianPtrArray * outCalendars, TransactionObject::EFetchType & out)
{
    CAPIStatus capiStatus;
    pCAPIStream outStream = NULL;
    void * writeData = 0;

#if TEST_STRINGREADER_PARSE
    char debugBuf[20000];
    myCtx.p = debugBuf;
    myCtx.iSize = sizeof(debugBuf);
    
    capiStatus = CAPI_SetStreamCallbacks(&outStream, NULL, NULL, writeToBufferDebug, &myCtx, 0);
#else
    PRThread * parseThread;

    PR_ASSERT(outCalendars != 0);
    //

    PRThread * mainThread = PR_CurrentThread();

#if TESTING_ITIPRIG
    TRACE("--main thread running\n");
#endif

    m_Monitor = PR_NewMonitor();
    PR_ASSERT(m_Monitor != 0);
    ICalCAPIReader * capiReader = new ICalCAPIReader(m_Monitor);
    PR_ASSERT(capiReader != 0);
    //
    t_int32 STACK_SIZE = 4096;
    t_int32 threadPriority = 5;
    t_int32 stackSize = 0;
    parseThread = PR_CreateThread("parseThread", threadPriority, stackSize);
    JulianParser * jp = new JulianParser(capiReader, outCalendars, parseThread);
    
    //PR_Start(parseThread, jparser_ParseCalendars, (void *) (capiReader), (void *) (outCalendars));
     
    //JulianParser::ParseCalendars(capiReader, outCalendars);

    //capiStatus = CAPI_SetStreamCallbacks(&outStream, NULL, NULL, getransactionobj_writeToCAPIReader, capiReader, 0);
    capiStatus = CAPI_SetStreamCallbacks(&outStream, NULL, NULL, getransactionobj_writeToCAPIReader, jp, 0);

    
#endif

    if (capiStatus == CAPI_ERR_OK)
    {
        t_int32 modifierSize;
        switch (m_FetchType)
        {
        case EFetchType_UID:
            PR_ASSERT(modifiers != 0 && modifiers->GetSize() > 0 && 
                modifiers->GetSize() <= 3);
            PR_ASSERT(iHandleCount == 1);

            if (modifiers != 0 && modifiers->GetSize() > 0 &&
                modifiers->GetSize() <= 3 && iHandleCount == 1)
            {   

                char * modifier;
                char * uid;
                char * rid = NULL;
                t_int8 modifierInt = CAPI_THISINSTANCE;
                modifierSize = modifiers->GetSize();

                // Get the args (uid, rid, rangeModifier)
                uid = ((UnicodeString *) modifiers->GetAt(0))->toCString("");
                if (modifierSize > 1)
                {
                    rid = ((UnicodeString *) modifiers->GetAt(1))->toCString("");
                    if (modifierSize > 2)
                    {
                        modifier = ((UnicodeString *) modifiers->GetAt(2))->toCString("");
                        if (strcmp(modifier, "THISANDPRIOR") == 0)
                            modifierInt = CAPI_THISANDPRIOR;
                        else if (strcmp(modifier, "THISANDFUTURE") == 0)
                            modifierInt = CAPI_THISANDFUTURE;
                        else 
                            modifierInt = CAPI_THISINSTANCE;
                    }
                }

                // TODO: set the property list to NULL for now, pass in later
                TRACE("--main thread: fetching by UID:, %s, RID %s, MODIFIER %d\r\n", uid, rid, modifierInt);
                capiStatus = CAPI_FetchEventByID(pS, pH[0], 0, uid, rid, modifierInt,
                    NULL, 0, outStream);
            }
            break;
        case EFetchType_DateRange:
            PR_ASSERT(modifiers != 0 && modifiers->GetSize() == 2);
            PR_ASSERT(iHandleCount >= 1);            
            if (modifiers != 0 && modifiers->GetSize() == 2 &&
                iHandleCount >= 1)
            {
                char * start;
                char * end;
                start = ((UnicodeString *) modifiers->GetAt(0))->toCString("");
                end = ((UnicodeString *) modifiers->GetAt(1))->toCString("");

                // TODO: set the property list to NULL for now, pass in later
                TRACE("--main thread: fetching by DateRange:, START %s, END %s\r\n", start, end);
                capiStatus = CAPI_FetchEventsByRange(pS, &pH[0], iHandleCount, 0, 
                    start, end, NULL, 0, outStream);
            }
            break;
        case EFetchType_AlarmRange:
            PR_ASSERT(modifiers != 0 && modifiers->GetSize() == 2);
            PR_ASSERT(iHandleCount >= 1);            
            if (modifiers != 0 && modifiers->GetSize() == 2 &&
                iHandleCount >= 1)
            {
                char * start;
                char * end;
                start = ((UnicodeString *) modifiers->GetAt(0))->toCString("");
                end = ((UnicodeString *) modifiers->GetAt(1))->toCString("");

                // TODO: set the property list to NULL for now, pass in later
                TRACE("--main thread: fetching by AlarmRange:, START %s, END %s\r\n", start, end);
                capiStatus = CAPI_FetchEventsByAlarmRange(pS, &pH[0], iHandleCount, 0, 
                    start, end, NULL, 0, outStream);
            }
            break;
        }
        out = m_FetchType;
    }

    //----
    char * c = myCtx.p;

#if TEST_STRINGREADER_PARSE
    ICalReader * ir = (ICalReader *) new ICalStringReader(debugBuf);
    if (ir != 0)
    {
        JulianParser::ParseCalendars(ir, outCalendars);
        delete ir; ir = 0;
    }
#endif // TEST_STRINGREADER_PARSE

    return capiStatus;

}

#endif /* #if CAPI_READY */
