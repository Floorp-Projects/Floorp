/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "nscore.h"
#include "nsXPITriggerInfo.h"
#include "nsDebug.h"

//
// nsXPITriggerItem
//

nsXPITriggerItem::nsXPITriggerItem( const PRUnichar* aName, 
                                    const PRUnichar* aURL, 
                                    PRInt32 aFlags )
  : mName(aName), mFlags(aFlags)
{
    nsString URL(aURL);

    PRInt32 pos = URL.Find('?');
    if ( pos == -1 ) {
        // no arguments
        mURL = URL;
        mArguments = "";
    }
    else
    {
        URL.Left(mURL,pos);
        URL.Right(mArguments, URL.Length()-pos+1);
    }
}


//
// nsXPITriggerInfo
//

nsXPITriggerInfo::nsXPITriggerInfo() 
  : mCx(0), mGlobal(JSVAL_NULL), mCbval(JSVAL_NULL)
{}

nsXPITriggerInfo::~nsXPITriggerInfo()
{
    nsXPITriggerItem* item;

    for(PRUint32 i=0; i < Size(); i++)
    {
        item = Get(i);
        if (item) 
            delete item;
    }
    mItems.RemoveAll();

    if ( mCx && !JSVAL_IS_NULL(mGlobal) )
        JS_RemoveRoot( mCx, &mGlobal );

    if ( mCx && !JSVAL_IS_NULL(mCbval) )
        JS_RemoveRoot( mCx, &mCbval );
}

void nsXPITriggerInfo::SaveCallback( JSContext *aCx, jsval aVal )
{
    NS_ASSERTION( mCx == 0, "callback set twice, memory leak" );
    mCx = aCx;
    mGlobal = OBJECT_TO_JSVAL(JS_GetGlobalObject( mCx ));
    mCbval = aVal;

    if ( !JSVAL_IS_NULL(mGlobal) )
        JS_AddRoot( mCx, &mGlobal );
    if ( !JSVAL_IS_NULL(mCbval) )
        JS_AddRoot( mCx, &mCbval );
}

 
