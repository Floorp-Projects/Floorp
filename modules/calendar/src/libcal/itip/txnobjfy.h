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
 * txnobjfy.h
 * John Sun
 * 4/13/98 10:34:44 AM
 */
#ifndef __TRANSACTIONOBJECTFACTORY_H_
#define __TRANSACTIONOBJECTFACTORY_H_

#include "jdefines.h"
#include "txnobj.h"

class TransactionObjectFactory
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    TransactionObjectFactory();
public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    static TransactionObject * Make(NSCalendar & cal, 
        JulianPtrArray & components, User & user, JulianPtrArray & recipients,
        UnicodeString & subject, JulianPtrArray & modifiers, 
        JulianForm * jf, MWContext * context,
        UnicodeString & attendeeName, 
        TransactionObject::EFetchType fetchType = TransactionObject::EFetchType_NONE);
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/ 
    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#endif /*__TRANSACTIONOBJECTFACTORY_H_ */

