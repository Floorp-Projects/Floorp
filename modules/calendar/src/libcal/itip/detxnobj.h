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
 * detxnobj.h
 * John Sun
 * 4/13/98 11:05:55 AM
 */
#ifndef __DELETETRANSACTIONOBJECT_H_
#define __DELETETRANSACTIONOBJECT_H_

#include "jdefines.h"
#include "txnobj.h"

class DeleteTransactionObject : public TransactionObject
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    DeleteTransactionObject();
public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    DeleteTransactionObject(NSCalendar & cal, JulianPtrArray & components,
        User & user, JulianPtrArray & recipients,
        UnicodeString & subject, JulianPtrArray & modifiers,
        JulianForm * jf, MWContext * context,
        UnicodeString & attendeeName);

    virtual ~DeleteTransactionObject() {}
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
#if CAPI_READY
    /* older CAPI, TODO: remove later */
    virtual CAPIStatus handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags, 
        JulianPtrArray * inComponents, NSCalendar * inCal,
        JulianPtrArray * modifiers, 
        JulianPtrArray * outCalendars, TransactionObject::EFetchType & out);
    /*
    virtual CAPIStatus handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags,
        JulianPtrArray * modifiers, JulianPtrArray * outCalendars, 
        TransactionObject::EFetchType & out);
    */

    /* TODO: more recent CAPI version */
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

#endif /* __DELETETRANSACTIONOBJECT_H_ */

