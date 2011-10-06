/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsXPITriggerInfo_h
#define nsXPITriggerInfo_h

#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsISupportsUtils.h"
#include "nsILocalFile.h"
#include "nsIOutputStream.h"
#include "jsapi.h"
#include "prthread.h"
#include "nsIXPConnect.h"
#include "nsICryptoHash.h"
#include "nsIPrincipal.h"
#include "nsThreadUtils.h"

struct XPITriggerEvent : public nsRunnable {
    nsString    URL;
    PRInt32     status;
    JSContext*  cx;
    jsval       cbval;
    nsCOMPtr<nsISupports> ref;
    nsCOMPtr<nsIPrincipal> princ;

    virtual ~XPITriggerEvent();
    NS_IMETHOD Run();
};

class nsXPITriggerItem
{
  public:
    nsXPITriggerItem( const PRUnichar* name,
                      const PRUnichar* URL,
                      const PRUnichar* iconURL,
                      const char* hash = nsnull,
                      PRInt32 flags = 0);
    ~nsXPITriggerItem();

    nsString    mName;
    nsString    mURL;
    nsString    mIconURL;
    nsString    mArguments;
    nsString    mCertName;

    bool        mHashFound; // this flag indicates that we found _some_ hash info in the trigger
    nsCString   mHash;
    nsCOMPtr<nsICryptoHash> mHasher;
    PRInt32     mFlags;

    nsCOMPtr<nsILocalFile>      mFile;
    nsCOMPtr<nsIOutputStream>   mOutStream;
    nsCOMPtr<nsIPrincipal>      mPrincipal;

    void    SetPrincipal(nsIPrincipal* aPrincipal);

    bool    IsFileURL() { return StringBeginsWith(mURL, NS_LITERAL_STRING("file:/")); }

    const PRUnichar* GetSafeURLString();

  private:
    //-- prevent inadvertent copies and assignments
    nsXPITriggerItem& operator=(const nsXPITriggerItem& rhs);
    nsXPITriggerItem(const nsXPITriggerItem& rhs);

    nsString    mSafeURL;
};



class nsXPITriggerInfo
{
  public:
    nsXPITriggerInfo();
    ~nsXPITriggerInfo();

    void                Add( nsXPITriggerItem *aItem )
                        { if ( aItem ) mItems.AppendElement( (void*)aItem ); }

    nsXPITriggerItem*   Get( PRUint32 aIndex )
                        { return (nsXPITriggerItem*)mItems.ElementAt(aIndex);}

    void                SaveCallback( JSContext *aCx, jsval aVal );

    PRUint32            Size() { return mItems.Count(); }

    void                SendStatus(const PRUnichar* URL, PRInt32 status);

    void                SetPrincipal(nsIPrincipal* aPrinc) { mPrincipal = aPrinc; }


  private:
    nsVoidArray mItems;
    JSContext   *mCx;
    nsCOMPtr<nsISupports> mContextWrapper;
    jsval       mCbval;
    nsCOMPtr<nsIThread> mThread;

    nsCOMPtr<nsIPrincipal>      mPrincipal;

    //-- prevent inadvertent copies and assignments
    nsXPITriggerInfo& operator=(const nsXPITriggerInfo& rhs);
    nsXPITriggerInfo(const nsXPITriggerInfo& rhs);
};

#endif /* nsXPITriggerInfo_h */
