/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsNativeAppSupport.h"
#include "nsString.h"

#include <Gestalt.h>
#include <Dialogs.h>
#include <Resources.h>
#include <TextUtils.h>

#include "nsIObserver.h"

#define rSplashDialog 512

const OSType kNSCreator = 'MOSS';
const OSType kMozCreator = 'MOZZ';
const SInt16 kNSCanRunStrArrayID = 1000;
const SInt16 kAnotherVersionStrIndex = 1;

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

#if TARGET_CARBON
  ::ShowWindow(GetDialogWindow(mDialog));
  ::SetPortDialogPort(mDialog);
#else 
  ::ShowWindow(mDialog);
  ::SetPort(mDialog);
#endif

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

// Snagged from mozilla/xpinstall/wizrd/mac/src/SetupTypeWin.c
// VersGreaterThan4 - utility function to test if it's >4.x running
static Boolean VersGreaterThan4(FSSpec *fSpec)
{
  Boolean result = false;
  short fRefNum = 0;
  
  ::SetResLoad(false);
  fRefNum = ::FSpOpenResFile(fSpec, fsRdPerm);
  ::SetResLoad(true);
  if (fRefNum != -1)
  {
    Handle  h;
    h = ::Get1Resource('vers', 2);
    if (h && **(unsigned short**)h >= 0x0500)
      result = true;
    ::CloseResFile(fRefNum);
  }
    
  return result;
}

PRBool NS_CanRun() 
{
  long response = 0;
  OSErr err = ::Gestalt (gestaltSystemVersion, &response);
  // check for at least MacOS 8.6
  if ( err || response < 0x850)
  {
    ::StopAlert (5000, NULL);
    return PR_FALSE;
  }

  // Check for running instances of Mozilla or Netscape. The real issue
  // is having more than one app use the same profile directory. That would
  // be REAL BAD!!!!!!!!

  // The code below is a copy of nsLocalFile::FindRunningAppBySignature which is
  // a non-static method. Sounds silly that I have to create an nsILocalFile
  // just to call that method. At any rate, don't think we want to go through
  // all that rigmarole during startup anyway.

  ProcessInfoRec  info;
  FSSpec          tempFSSpec;
  ProcessSerialNumber psn, nextProcessPsn;

  nextProcessPsn.highLongOfPSN = 0;
  nextProcessPsn.lowLongOfPSN  = kNoProcess;

  // first, get our psn so that we can exclude ourselves when searching
  err = ::GetCurrentProcess(&psn);

  if (err != noErr)
    return PR_FALSE;

  // We loop while err == noErr, which should mean all our calls are OK
  // The ways of 'break'-ing out of the loop are:
  //   GetNextProcess() fails (this includes getting procNotFound, meaning we're at the end of the process list),
  //   GetProcessInformation() fails
  // The ways we should fall out of the while loop are:
  //   GetIndString() fails, err == resNotFound,
  //   we found a running mozilla process or running Netscape > 4.x process, err == fnfErr
  while (err == noErr)
  {
    err = ::GetNextProcess(&nextProcessPsn);
    if (err != noErr)
      break; // most likely, end of process list
    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = nil;
    info.processAppSpec = &tempFSSpec;
    err = ::GetProcessInformation(&nextProcessPsn, &info);
    if (err != noErr)
      break; // aww crap, GetProcessInfo() failed, we're outta here

    if (info.processSignature == kNSCreator || info.processSignature == kMozCreator)
    {
      // if the found process is us, obviously, it's okay if WE'RE running,
      if (!(info.processNumber.lowLongOfPSN == psn.lowLongOfPSN &&
            info.processNumber.highLongOfPSN == psn.highLongOfPSN))
      {
        // check to see if Netscape process is greater than Netscape 4.x or
        // if process is Mozilla
        if ((info.processSignature == kNSCreator && VersGreaterThan4(&tempFSSpec)) ||
             info.processSignature == kMozCreator)
        {
          // put up error dialog
          Str255 str;
          ::GetIndString(str, kNSCanRunStrArrayID, kAnotherVersionStrIndex);
          if (StrLength(str) == 0)
            err = resNotFound; // set err to something so that we return false
          else
          {
            SInt16 outItemHit;
            if (str)
              ::StandardAlert(kAlertStopAlert, str, nil, nil, &outItemHit);
            err = fnfErr; // set err to something so that we return false
          }
        }
      }
    }
  }

  if (err == noErr || err == procNotFound)
    return PR_TRUE;

  return PR_FALSE;
}
