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

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOMessage.h>

#include "nsCocoaUtils.h"

#include "nsWidgetAtoms.h"
#include "nsIRollupListener.h"
#include "nsIWidget.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

// defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

static io_connect_t gRootPort = MACH_PORT_NULL;

// Static thread local storage index of the Toolkit 
// object associated with a given thread...
static PRUintn gToolkitTLSIndex = 0;


nsToolkit::nsToolkit()
: mInited(false)
, mSleepWakeNotificationRLS(nsnull)
, mEventMonitorHandler(nsnull)
, mEventTapPort(nsnull)
, mEventTapRLS(nsnull)
{
}


nsToolkit::~nsToolkit()
{
  RemoveSleepWakeNotifcations();
  UnregisterAllProcessMouseEventHandlers();
  // Remove the TLS reference to the toolkit...
  PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsToolkit, nsIToolkit);


NS_IMETHODIMP
nsToolkit::Init(PRThread * aThread)
{
  nsWidgetAtoms::RegisterAtoms();
  
  mInited = true;
  
  RegisterForSleepWakeNotifcations();
  RegisterForAllProcessMouseEvents();

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


// We shouldn't do anything here.  See RegisterForAllProcessMouseEvents() for
// the reason why.
static OSStatus EventMonitorHandler(EventHandlerCallRef aCaller, EventRef aEvent, void* aRefcon)
{
  return eventNotHandledErr;
}

// Converts aPoint from the CoreGraphics "global display coordinate" system
// (which includes all displays/screens and has a top-left origin) to its
// (presumed) Cocoa counterpart (assumed to be the same as the "screen
// coordinates" system), which has a bottom-left origin.
static NSPoint ConvertCGGlobalToCocoaScreen(CGPoint aPoint)
{
  NSPoint cocoaPoint;
  cocoaPoint.x = aPoint.x;
  cocoaPoint.y = nsCocoaUtils::FlippedScreenY(aPoint.y);
  return cocoaPoint;
}


// Since our event tap is "listen only", events arrive here a little after
// they've already been processed.
static CGEventRef EventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
  if ((type == kCGEventTapDisabledByUserInput) ||
      (type == kCGEventTapDisabledByTimeout))
    return event;
  if (!gRollupWidget || !gRollupListener || [NSApp isActive])
    return event;
  // Don't bother with rightMouseDown events here -- because of the delay,
  // we'll end up closing browser context menus that we just opened.  Since
  // these events usually raise a context menu, we'll handle them by hooking
  // the @"com.apple.HIToolbox.beginMenuTrackingNotification" distributed
  // notification (in nsAppShell.mm's AppShellDelegate).
  if (type == kCGEventRightMouseDown)
    return event;
  NSWindow *ctxMenuWindow = (NSWindow*) gRollupWidget->GetNativeData(NS_NATIVE_WINDOW);
  if (!ctxMenuWindow)
    return event;
  NSPoint screenLocation = ConvertCGGlobalToCocoaScreen(CGEventGetLocation(event));
  // Don't roll up the rollup widget if our mouseDown happens over it (doing
  // so would break the corresponding context menu).
  if (NSPointInRect(screenLocation, [ctxMenuWindow frame]))
    return event;
  gRollupListener->Rollup(nsnull);
  return event;
}


// Cocoa Firefox's use of custom context menus requires that we explicitly
// handle mouse events from other processes that the OS handles
// "automatically" for native context menus -- mouseMoved events so that
// right-click context menus work properly when our browser doesn't have the
// focus (bmo bug 368077), and mouseDown events so that our browser can
// dismiss a context menu when a mouseDown happens in another process (bmo
// bug 339945).
void
nsToolkit::RegisterForAllProcessMouseEvents()
{
  // Don't do this for apps that (like Camino) use native context menus.
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRBool useNativeContextMenus;
    nsresult rv = prefs->GetBoolPref("ui.use_native_popup_windows",
                                     &useNativeContextMenus);
    if (NS_SUCCEEDED(rv) && useNativeContextMenus)
      return;
  }
  if (!mEventMonitorHandler) {
    // Installing a handler for particular Carbon events causes the OS to post
    // equivalent Cocoa events to the browser's event stream (the one that
    // passes through [NSApp sendEvent:]).  For this reason installing a
    // handler for kEventMouseMoved fixes bmo bug 368077, even though our
    // handler does nothing on mouse-moved events.  (Actually it's more
    // accurate to say that the OS (working in a different process) sends
    // events to the window server, from which the OS (acting in the browser's
    // process on its behalf) grabs them and turns them into both Carbon
    // events (which get fed to our handler) and Cocoa events (which get fed
    // to [NSApp sendEvent:]).)
    EventTypeSpec kEvents[] = {{kEventClassMouse, kEventMouseMoved}};
    InstallEventHandler(GetEventMonitorTarget(), EventMonitorHandler,
                        GetEventTypeCount(kEvents), kEvents, 0,
                        &mEventMonitorHandler);
  }
  if (!mEventTapRLS) {
    // Using an event tap for mouseDown events (instead of installing a
    // handler for them on the EventMonitor target) works around an Apple
    // bug that causes OS menus (like the Clock menu) not to work properly
    // on OS X 10.4.X and below (bmo bug 381448).
    // We install our event tap "listen only" to get around yet another Apple
    // bug -- when we install it as an event filter on any kind of mouseDown
    // event, that kind of event stops working in the main menu, and usually
    // mouse event processing stops working in all apps in the current login
    // session (so the entire OS appears to be hung)!  The downside of
    // installing listen-only is that events arrive at our handler slightly
    // after they've already been processed.
    mEventTapPort = CGEventTapCreate(kCGSessionEventTap,
                                     kCGHeadInsertEventTap,
                                     kCGEventTapOptionListenOnly,
                                     CGEventMaskBit(kCGEventLeftMouseDown)
                                       | CGEventMaskBit(kCGEventRightMouseDown)
                                       | CGEventMaskBit(kCGEventOtherMouseDown),
                                     EventTapCallback,
                                     nsnull);
    if (!mEventTapPort)
      return;
    mEventTapRLS = CFMachPortCreateRunLoopSource(nsnull, mEventTapPort, 0);
    if (!mEventTapRLS) {
      CFRelease(mEventTapPort);
      mEventTapPort = nsnull;
      return;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), mEventTapRLS, kCFRunLoopDefaultMode);
  }
}


void
nsToolkit::UnregisterAllProcessMouseEventHandlers()
{
  if (mEventMonitorHandler) {
    RemoveEventHandler(mEventMonitorHandler);
    mEventMonitorHandler = nsnull;
  }
  if (mEventTapRLS) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mEventTapRLS,
                          kCFRunLoopDefaultMode);
    CFRelease(mEventTapRLS);
    mEventTapRLS = nsnull;
  }
  if (mEventTapPort) {
    CFRelease(mEventTapPort);
    mEventTapPort = nsnull;
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
      //This should probably be changed when our minimum version changes
      NS_ERROR("Couldn't determine OS X version, assuming 10.4");
      gOSXVersion = MAC_OS_X_VERSION_10_4_HEX;
    }
  }
  return gOSXVersion;
}


PRBool nsToolkit::OnLeopardOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_5_HEX) ? PR_TRUE : PR_FALSE;
}
