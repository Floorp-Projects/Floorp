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

#include "nsString.h"

#include <Gestalt.h>
#include <Dialogs.h>
#include <Resources.h>
#include <TextUtils.h>
#include <ControlDefinitions.h>

#include "nsCOMPtr.h"
#include "nsNativeAppSupportBase.h"

#include "nsIAppShellService.h"
#include "nsIAppStartup.h"
#include "nsIBaseWindow.h"
#include "nsICmdLineService.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsIServiceManager.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"

#include "nsXPFEComponentsCID.h"

static Boolean VersGreaterThan4(const FSSpec *fSpec);

const OSType kNSCreator = 'MOSS';
const OSType kMozCreator = 'MOZZ';
const SInt16 kNSCanRunStrArrayID = 1000;
const SInt16 kAnotherVersionStrIndex = 1;

nsresult
GetNativeWindowPointerFromDOMWindow(nsIDOMWindowInternal *window, WindowRef *nativeWindow);

const SInt16 kNSOSVersErrsStrArrayID = 1001;

enum {
        eOSXVersTooOldErrIndex = 1,
        eOSXVersTooOldExplanationIndex,
        eContinueButtonTextIndex,
        eQuitButtonTextIndex,
        eCarbonLibVersTooOldIndex,
        eCarbonLibVersTooOldExplanationIndex
     };

class nsNativeAppSupportMac : public nsNativeAppSupportBase
{
public:
  nsNativeAppSupportMac() :
    mCanShowUI(PR_FALSE) { }

  NS_IMETHOD Start(PRBool* aRetVal);
  NS_IMETHOD ReOpen();
  NS_IMETHOD Enable();

private:
  PRBool mCanShowUI;

};

NS_IMETHODIMP
nsNativeAppSupportMac::Enable()
{
  mCanShowUI = PR_TRUE;
  return NS_OK;
}

/* boolean start (); */
NS_IMETHODIMP nsNativeAppSupportMac::Start(PRBool *_retval)
{
  Str255 str1;
  Str255 str2;
  SInt16 outItemHit;
  long   response = 0;
  OSErr  err = ::Gestalt (gestaltSystemVersion, &response);
  // check for at least MacOS 8.5
  if ( err || response < 0x850)
  {
    ::StopAlert (5000, NULL);
    *_retval = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  
#if TARGET_CARBON
  // If we're running under Mac OS X check for at least Mac OS X 10.1
  // If that fails display a StandardAlert giving the user the option
  // to continue running the app or quitting
  if (response >= 0x00001000 && response < 0x00001010)
  {
    // put up error dialog
    Str255 continueButtonLabel;
    Str255 quitButtonLabel;
    ::GetIndString(str1, kNSOSVersErrsStrArrayID, eOSXVersTooOldErrIndex);
    ::GetIndString(str2, kNSOSVersErrsStrArrayID, eOSXVersTooOldExplanationIndex);
    ::GetIndString(continueButtonLabel, kNSOSVersErrsStrArrayID, eContinueButtonTextIndex);
    ::GetIndString(quitButtonLabel, kNSOSVersErrsStrArrayID, eQuitButtonTextIndex);
    if (StrLength(str1) && StrLength(str1) && StrLength(continueButtonLabel) && StrLength(quitButtonLabel))
    {
      AlertStdAlertParamRec pRec;
      
      pRec.movable      = nil;
      pRec.filterProc 	= nil;
      pRec.defaultText  = continueButtonLabel;
      pRec.cancelText   = quitButtonLabel;
      pRec.otherText    = nil;
      pRec.helpButton   = nil;
      pRec.defaultButton = kAlertStdAlertOKButton;
      pRec.cancelButton  = kAlertStdAlertCancelButton;
      pRec.position      = 0;
      
      ::StandardAlert(kAlertNoteAlert, str1, str2, &pRec, &outItemHit);
      if (outItemHit == kAlertStdAlertCancelButton)
        return PR_FALSE;
    }
    else
      return PR_FALSE;
  }
  
  // We also check for CarbonLib version >= 1.4 if OS vers < 10.0
  // which is always cause for the app to quit
  if (response < 0x00001000)
  {
    err = ::Gestalt (gestaltCarbonVersion, &response);
    if (err || response < 0x00000140)
    {
      // put up error dialog
      ::GetIndString(str1, kNSOSVersErrsStrArrayID, eCarbonLibVersTooOldIndex);
      ::GetIndString(str2, kNSOSVersErrsStrArrayID, eCarbonLibVersTooOldExplanationIndex);
      if (StrLength(str1) && StrLength(str1))
      {
        ::StandardAlert(kAlertStopAlert, str1, str2, nil, &outItemHit);
      }
      return PR_FALSE;
    }
  }
#endif

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportMac::ReOpen()
{
  if (!mCanShowUI)
    return NS_ERROR_FAILURE;

  PRBool haveUncollapsed = PR_FALSE;
  PRBool haveOpenWindows = PR_FALSE;
  PRBool done = PR_FALSE;
  
  nsCOMPtr<nsIWindowMediator> 
    wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm)
  {
    return NS_ERROR_FAILURE;
  } 
  else
  {
    nsCOMPtr<nsISimpleEnumerator> windowList;
    wm->GetXULWindowEnumerator(nsnull, getter_AddRefs(windowList));
    PRBool more;
    windowList->HasMoreElements(&more);
    while (more)
    {
      nsCOMPtr<nsISupports> nextWindow = nsnull;
      windowList->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(nextWindow));
		  if (!baseWindow)
		  {
        windowList->HasMoreElements(&more);
        continue;
      }
      else
      {
        haveOpenWindows = PR_TRUE;
      }

      nsCOMPtr<nsIWidget> widget = nsnull;
      baseWindow->GetMainWidget(getter_AddRefs(widget));
      if (!widget)
      {
        windowList->HasMoreElements(&more);
        continue;
      }
      WindowRef windowRef = (WindowRef)widget->GetNativeData(NS_NATIVE_DISPLAY);
      if (!::IsWindowCollapsed(windowRef))
      {
        haveUncollapsed = PR_TRUE;
        break;  //have un-minimized windows, nothing to do
      } 
      windowList->HasMoreElements(&more);
    } // end while
        
    if (!haveUncollapsed)
    {
      //uncollapse the most recenty used window
      nsCOMPtr<nsIDOMWindowInternal> mru = nsnull;
      wm->GetMostRecentWindow(nsnull, getter_AddRefs(mru));
            
      if (mru) 
      {        
        WindowRef mruRef = nil;
        GetNativeWindowPointerFromDOMWindow(mru, &mruRef);
        if (mruRef)
        {
          ::CollapseWindow(mruRef, FALSE);
          ::SelectWindow(mruRef);
          done = PR_TRUE;
        }
      }
      
    } // end if have uncollapsed 
    
    if (!haveOpenWindows && !done)
    {    
    
      NS_WARNING("trying to open new window");
      //use the bootstrap helpers to make the right kind(s) of window open        
      nsresult rv = PR_FALSE;
      nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
      if (appStartup)
      {
        PRBool openedAWindow = PR_FALSE;
        appStartup->CreateStartupState(nsIAppShellService::SIZE_TO_CONTENT,
                                       nsIAppShellService::SIZE_TO_CONTENT,
                                       &openedAWindow);
      }
    }
    
  } // got window mediator
  return NS_OK;
}

nsresult
GetNativeWindowPointerFromDOMWindow(nsIDOMWindowInternal *a_window, WindowRef *a_nativeWindow)
{
    *a_nativeWindow = nil;
    if (!a_window) return NS_ERROR_INVALID_ARG;
    
    nsCOMPtr<nsIWebNavigation> mruWebNav(do_GetInterface(a_window));
    if (mruWebNav)
    {
      nsCOMPtr<nsIDocShellTreeItem> mruTreeItem(do_QueryInterface(mruWebNav));
      nsCOMPtr<nsIDocShellTreeOwner> mruTreeOwner = nsnull;
      mruTreeItem->GetTreeOwner(getter_AddRefs(mruTreeOwner));
      if(mruTreeOwner)
      {
        nsCOMPtr<nsIBaseWindow> mruBaseWindow(do_QueryInterface(mruTreeOwner));
        if (mruBaseWindow)
        {
          nsCOMPtr<nsIWidget> mruWidget = nsnull;
          mruBaseWindow->GetMainWidget(getter_AddRefs(mruWidget));
          if (mruWidget)
          {
            *a_nativeWindow = (WindowRef)mruWidget->GetNativeData(NS_NATIVE_DISPLAY);
          }
        }
      }
    }
    return NS_OK;
}

#pragma mark -

// Create and return an instance of class nsNativeAppSupportMac.
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport**aResult)
{
  *aResult = new nsNativeAppSupportMac;
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF( *aResult );
  return NS_OK;
}
