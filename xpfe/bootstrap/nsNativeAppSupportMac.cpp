/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsNativeAppSupport.h"
#include "nsString.h"

#include <Gestalt.h>
#include <Dialogs.h>
#include <Resources.h>

#include "nsIObserver.h"

#define rSplashDialog 512

class nsSplashScreenMac : public nsISplashScreen,
                          public nsIObserver
{
public:

    // dialog itemse
    enum {
      eSplashPictureItem = 1,
      eSplashStatusTextItem    
    };
    
            nsSplashScreenMac();    
    virtual ~nsSplashScreenMac();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Show();
    NS_IMETHOD Hide();

    NS_DECL_NSIOBSERVER

protected:

    DialogPtr mDialog;

}; // class nsSplashScreenMac


nsSplashScreenMac::nsSplashScreenMac()
: mDialog(nsnull)
{
  NS_INIT_REFCNT();
}


nsSplashScreenMac::~nsSplashScreenMac()
{
  Hide();
}


NS_IMPL_ISUPPORTS2(nsSplashScreenMac, nsISplashScreen, nsIObserver);

NS_IMETHODIMP
nsSplashScreenMac::Show()
{
	mDialog = ::GetNewDialog(rSplashDialog, nil, (WindowPtr)-1L);
	if (!mDialog) return NS_ERROR_FAILURE;
	
	::ShowWindow(mDialog);
	::SetPort(mDialog);
	
	::DrawDialog(mDialog);    // we don't handle events for this dialog, so we
	                          // need to draw explicitly. Yuck.
  return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenMac::Hide()
{
	if (mDialog)
	{
		::DisposeDialog( mDialog );
    mDialog = nsnull;
	}
  return NS_OK;
}


NS_IMETHODIMP
nsSplashScreenMac::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  // update a string in the dialog
  
  nsCAutoString statusString;
  statusString.AssignWithConversion(someData);

  Handle    item = nsnull;
  Rect      itemRect;
  short     itemType;
  ::GetDialogItem(mDialog, eSplashStatusTextItem, &itemType, &item, &itemRect);
  if (!item) return NS_OK;
  
  // convert string to Pascal string
  Str255    statusPStr;
  PRInt32   maxLen = statusString.Length();
  if (maxLen > 254)
    maxLen = 254;
  strncpy((char *)&statusPStr[1], (const char *)statusString, maxLen);
  statusPStr[0] = maxLen;
  
  ::SetDialogItemText(item, statusPStr);
  ::DrawDialog(mDialog);
  return NS_OK;
}


#pragma mark -

nsresult NS_CreateSplashScreen(nsISplashScreen**aResult)
{
  if ( aResult ) {	
      *aResult = new nsSplashScreenMac;
      if ( *aResult ) {
          NS_ADDREF( *aResult );
          return NS_OK;
      } else {
          return NS_ERROR_OUT_OF_MEMORY;
      }
  } else {
      return NS_ERROR_NULL_POINTER;
  }
}

PRBool NS_CanRun() 
{
	long response = 0;
	OSErr err = ::Gestalt (gestaltSystemVersion, &response);
	if ( err || response < 0x850)
	{
		::StopAlert (5000, NULL);
		return PR_FALSE;
	}
	return PR_TRUE;
}
