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
 * getxnobj.h
 * John Sun
 * 4/1/98 5:10:26 PM
 */
#ifndef __GETTRANSACTIONOBJECT_H_
#define __GETTRANSACTIONOBJECT_H_

#include "jdefines.h"
#include "txnobj.h"
#include "nspr.h"

class GetTransactionObject: public TransactionObject
{
#if CAPI_READY
    /* for debugging only */
public:
    CAPI_CTX myCtx;
#endif

private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    GetTransactionObject();

public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    GetTransactionObject(NSCalendar & cal, JulianPtrArray & components,
        User & user, JulianPtrArray & recipients,
        UnicodeString & subject, JulianPtrArray & modifiers,
        JulianForm * jf, MWContext * context,
        UnicodeString & attendeeName, EFetchType type);

    virtual ~GetTransactionObject() {};
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/
#if CAPI_READY
    /*
    int writeToCAPIReaderHelper(void * pData, char * pBuf,
        int iSize, int * piTransferred);
    */
    /*
    int writeToCAPIReader(void * pData, char * pBuf,
        int iSize, int * piTransferred);
      */  
    PRMonitor * m_Monitor;
#endif
    /* virtual JulianPtrArray * executeCAPI(); */
#if CAPI_READY
    PRMonitor * getMonitor() { return m_Monitor; }
    virtual CAPIStatus handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags, 
        JulianPtrArray * inComponents, NSCalendar * inCal,
        JulianPtrArray * modifiers, 
        JulianPtrArray * outCalendars, TransactionObject::EFetchType & out);

    /*
    virtual CAPIStatus handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        JulianPtrArray * modifiers, char *** strings, char ** outevent, 
        t_int32 & numstrings, EFetchType & out);
    */

    /* TODO: more recent CAPI version*/
    /*virtual CAPIStatus executeCAPI(CAPISession * s, CAPIHandle ** ppH,
        t_int32 iHandleCount, long lFlags, JulianPtrArray * modifiers,
        JulianPtrArray * propList, CAPIStream * stream);*/

#endif /* #if CAPI_READY */
    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/ 
    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#ifdef XP_CPLUSPLUS
extern "C"{
#endif

    int getransactionobj_writeToCAPIReader(void *, char *, int, int *);

#ifdef XP_CPLUSPLUS
};
#endif

#endif /* __GETTRANSACTIONNOBJECT_H_ */




