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
 * sttxnobj.h
 * John Sun
 * 4/13/98 10:37:07 AM
 */
#ifndef __STORETRANSACTIONNOBJECT_H_
#define __STORETRANSACTIONNOBJECT_H_

#include "jdefines.h"
#include "txnobj.h"

class StoreTransactionObject : public TransactionObject
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
#if CAPI_READY
    CAPI_CTX m_Ctx;
#endif
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    StoreTransactionObject();
public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    StoreTransactionObject(NSCalendar & cal, JulianPtrArray & components,
        User & user, JulianPtrArray & recipients,
        UnicodeString & subject, JulianPtrArray & modifiers,
        JulianForm * jf, MWContext * context,
        UnicodeString & attendeeName);

    virtual ~StoreTransactionObject() {}
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
#if CAPI_READY
    virtual CAPIStatus handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags, 
        JulianPtrArray * inComponents, NSCalendar * inCal,
        JulianPtrArray * modifiers, 
        JulianPtrArray * outCalendars, TransactionObject::EFetchType & out);
    /*
    virtual CAPIStatus handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        JulianPtrArray * modifiers, char *** strings, char ** outevent, 
        t_int32 & numstrings, TransactionObject::EFetchType & out);
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

#endif /* __STORETRANSACTIONNOBJECT_H_ */

