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

#ifndef nsXPITriggerInfo_h
#define nsXPITriggerInfo_h

#include "nsString.h"
#include "nsVector.h"
#include "nsCOMPtr.h"
#include "nsIXPINotifier.h"
#include "nsIFileSpec.h"
#include "jsapi.h"



class nsXPITriggerItem
{
  public:
    nsXPITriggerItem( const PRUnichar* name, const PRUnichar* URL, PRInt32 flags = 0);

    nsString    mName;
    nsString    mURL;
    nsString    mArguments;
    PRInt32     mFlags;

    nsCOMPtr<nsIFileSpec>  mFile;

    PRBool  IsFileURL() { return mURL.Equals("file://",7); }

  private:
    //-- prevent inadvertent copies and assignments
    nsXPITriggerItem& operator=(const nsXPITriggerItem& rhs);
    nsXPITriggerItem(const nsXPITriggerItem& rhs);
};



class nsXPITriggerInfo
{
  public:
    nsXPITriggerInfo();
    virtual ~nsXPITriggerInfo();

    void    Add( nsXPITriggerItem *aItem ) 
            { if ( aItem ) mItems.Add( (void*)aItem ); }

    nsXPITriggerItem*   Get( PRUint32 aIndex )
            { return NS_STATIC_CAST(nsXPITriggerItem*,mItems.Get(aIndex));}

    void                SaveCallback( JSContext *aCx, jsval aVal );
    PRUint32            Size() { return mItems.GetSize(); }
    
    nsVector mItems;
    JSContext *mCx;
    jsval     mGlobal;
    jsval     mCbval;

  private:
    //-- prevent inadvertent copies and assignments
    nsXPITriggerInfo& operator=(const nsXPITriggerInfo& rhs);
    nsXPITriggerInfo(const nsXPITriggerInfo& rhs);
};

#endif /* nsXPITriggerInfo_h */
