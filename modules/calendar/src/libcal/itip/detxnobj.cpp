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
 * detxnobj.cpp
 * John Sun
 * 4/13/98 11:07:45 AM
 */
#include "stdafx.h"
#include "detxnobj.h"
#include "txnobj.h"

#if CAPI_READY
#include <capi.h>
#endif /* CAPI_READY */

//---------------------------------------------------------------------

DeleteTransactionObject::DeleteTransactionObject()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

DeleteTransactionObject::DeleteTransactionObject(NSCalendar & cal, 
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

CAPIStatus 
DeleteTransactionObject::handleCAPI(pCAPISession & pS, pCAPIHandle * pH, 
        t_int32 iHandleCount, t_int32 lFlags, 
        JulianPtrArray * inComponents, NSCalendar * inCal,
        JulianPtrArray * modifiers, 
        JulianPtrArray * outCalendars, TransactionObject::EFetchType & out)
{
    CAPIStatus status;
    t_int32 modifierSize;
    
    // TODO: finish, don't use for now
    //PR_ASSERT(FALSE);
    
    //
    PR_ASSERT(modifiers != 0 && modifiers->GetSize() > 0 &&
        modifiers->GetSize() <= 3);
    PR_ASSERT(iHandleCount >= 1);
    if (modifiers != 0 && modifiers->GetSize() > 0 &&
        modifiers->GetSize() <= 3 && iHandleCount >= 1)    
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
        status = CAPI_DeleteEvent(pS, pH, iHandleCount, 0, uid, rid, modifierInt);
    }
    //
    return status;
}

//---------------------------------------------------------------------

#endif /* #if CAPI_READY */
