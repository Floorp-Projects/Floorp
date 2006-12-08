/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsToolkit.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

#include "nsWidgetAtoms.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include <Gestalt.h>


static io_connect_t gRootPort = MACH_PORT_NULL;

static const char kQuartzRenderingPref[] = "browser.quartz.enable";
static const char kAllFontSizesPref[] = "browser.quartz.enable.all_font_sizes";

// Static thread local storage index of the Toolkit 
// object associated with a given thread...
static PRUintn gToolkitTLSIndex = 0;


nsToolkit::nsToolkit()
: mInited(false)
, mSleepWakeNotificationRLS(nsnull)
{
}


nsToolkit::~nsToolkit()
{
  RemoveSleepWakeNotifcations();
  // Remove the TLS reference to the toolkit...
  PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
}


NS_IMPL_THREADSAFE_ISUPPORTS2(nsToolkit, nsIToolkit, nsIObserver);


NS_IMETHODIMP
nsToolkit::Init(PRThread * aThread)
{
  nsWidgetAtoms::RegisterAtoms();
  
  mInited = true;
  
  RegisterForSleepWakeNotifcations();
  SetupQuartzRendering();
  
  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->AddObserver(kQuartzRenderingPref, this, PR_FALSE);  
    prefs->AddObserver(kAllFontSizesPref, this, PR_FALSE);
  }
  return NS_OK;
}


// The pref changed, reset the app to use quartz rendering as dictated by the pref
NS_IMETHODIMP
nsToolkit::Observe(nsISupports*     aSubject,
                       const char*      aTopic,
                       const PRUnichar* aData)
{
  SetupQuartzRendering();
  return NS_OK;
}


nsToolkit* NS_CreateToolkitInstance()
{
  return new nsToolkit();
}


void
nsToolkit::PostSleepWakeNotification(const char* aNotification)
{
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
  if (observerService)
    observerService->NotifyObservers(nsnull, aNotification, nsnull);
}


// http://developer.apple.com/documentation/DeviceDrivers/Conceptual/IOKitFundamentals/PowerMgmt/chapter_10_section_3.html
static void ToolkitSleepWakeCallback(void *refCon, io_service_t service, natural_t messageType, void * messageArgument)
{
  switch (messageType)
  {
    case kIOMessageSystemWillSleep:
      // System is going to sleep now.
      nsToolkit::PostSleepWakeNotification("sleep_notification");
      ::IOAllowPowerChange(gRootPort, (long)messageArgument);
      break;
      
    case kIOMessageCanSystemSleep:
      // In this case, the computer has been idle for several minutes
      // and will sleep soon so you must either allow or cancel
      // this notification. Important: if you don’t respond, there will
      // be a 30-second timeout before the computer sleeps.
      // In Mozilla's case, we always allow sleep.
      ::IOAllowPowerChange(gRootPort,(long)messageArgument);
      break;
      
    case kIOMessageSystemHasPoweredOn:
      // Handle wakeup.
      nsToolkit::PostSleepWakeNotification("wake_notification");
      break;
  }
}


nsresult
nsToolkit::RegisterForSleepWakeNotifcations()
{
  IONotificationPortRef notifyPortRef;

  NS_ASSERTION(!mSleepWakeNotificationRLS, "Already registered for sleep/wake");

  gRootPort = ::IORegisterForSystemPower(0, &notifyPortRef, ToolkitSleepWakeCallback, &mPowerNotifier);
  if (gRootPort == MACH_PORT_NULL) {
    NS_ASSERTION(0, "IORegisterForSystemPower failed");
    return NS_ERROR_FAILURE;
  }

  mSleepWakeNotificationRLS = ::IONotificationPortGetRunLoopSource(notifyPortRef);
  ::CFRunLoopAddSource(::CFRunLoopGetCurrent(),
                       mSleepWakeNotificationRLS,
                       kCFRunLoopDefaultMode);

  return NS_OK;
}


void
nsToolkit::RemoveSleepWakeNotifcations()
{
  if (mSleepWakeNotificationRLS) {
    ::IODeregisterForSystemPower(&mPowerNotifier);
    ::CFRunLoopRemoveSource(::CFRunLoopGetCurrent(),
                            mSleepWakeNotificationRLS,
                            kCFRunLoopDefaultMode);

    mSleepWakeNotificationRLS = nsnull;
  }
}


// SetupQuartzRendering
//
// Use apple's technote for 10.1.5 to turn on quartz rendering with CG metrics. This
// slows us down about 12% when turned on.
void
nsToolkit::SetupQuartzRendering()
{
  // from Apple's technote at http://developer.apple.com/qa/qa2001/qa1193.html
  enum {
    kQDDontChangeFlags = 0xFFFFFFFF,         // don't change anything
    kQDUseDefaultTextRendering = 0,          // bit 0
    kQDUseTrueTypeScalerGlyphs = (1 << 0),   // bit 1
    kQDUseCGTextRendering = (1 << 1),        // bit 2
    kQDUseCGTextMetrics = (1 << 2)
  };
  
  const int kFlagsWeUse = kQDUseCGTextRendering | kQDUseCGTextMetrics;
  
  // turn on quartz rendering if we find the symbol in the app framework. Just turn
  // on the bits that we need, don't turn off what someone else might have wanted. If
  // the pref isn't found, assume we want it on. That way, we have to explicitly put
  // in a pref to disable it, rather than force everyone who wants it to carry around
  // an extra pref.
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs)
    return;
  
  PRBool enableQuartz = PR_TRUE;
  nsresult rv = prefs->GetBoolPref(kQuartzRenderingPref, &enableQuartz);
  UInt32 oldFlags = QDSwapTextFlags(kQDDontChangeFlags);
  if (NS_FAILED(rv) || enableQuartz) {
    QDSwapTextFlags(oldFlags | kFlagsWeUse);
    
    // the system defaults to not anti-aliasing small fonts, but some people
    // think it looks better that way. If the pref is set, turn them on
    PRBool antiAliasAllFontSizes = PR_FALSE;
    rv = prefs->GetBoolPref(kAllFontSizesPref, &antiAliasAllFontSizes);
    if (NS_SUCCEEDED(rv) && antiAliasAllFontSizes)
      SetOutlinePreferred(true);
  }
  else {
    QDSwapTextFlags(oldFlags & !kFlagsWeUse);
  }
}


// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  
  // Create the TLS index the first time through...
  if (gToolkitTLSIndex == 0) {
    PRStatus status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
    if (PR_FAILURE == status)
      return NS_ERROR_FAILURE;
  }
  
  // Create a new toolkit for this thread...
  nsToolkit* toolkit = (nsToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);
  if (!toolkit) {
    toolkit = NS_CreateToolkitInstance();
    if (!toolkit)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(toolkit);
    toolkit->Init(PR_GetCurrentThread());
    //
    // The reference stored in the TLS is weak.  It is removed in the
    // nsToolkit destructor...
    //
    PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
  }
  else {
    NS_ADDREF(toolkit);
  }
  *aResult = toolkit;
  return NS_OK;
}


long nsToolkit::OSXVersion()
{
  static long gOSXVersion = 0x0;
  if (gOSXVersion == 0x0) {
    OSErr err = ::Gestalt(gestaltSystemVersion, &gOSXVersion);
    if (err != noErr) {
      NS_ERROR("Couldn't determine OS X version, assuming 10.3");
      gOSXVersion = MAC_OS_X_VERSION_10_3_HEX;
    }
  }
  return gOSXVersion;
}
