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

extern "C" {
#include <mach-o/getsect.h>
}
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach/vm_map.h>
#include <unistd.h>
#include <dlfcn.h>

#import <Cocoa/Cocoa.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/IOMessage.h>

#include "nsCocoaUtils.h"
#include "nsObjCExceptions.h"

#include "nsWidgetAtoms.h"
#include "nsIRollupListener.h"
#include "nsIWidget.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"

#include "mozilla/Preferences.h"

using namespace mozilla;

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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult
nsToolkit::RegisterForSleepWakeNotifcations()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  IONotificationPortRef notifyPortRef;

  NS_ASSERTION(!mSleepWakeNotificationRLS, "Already registered for sleep/wake");

  gRootPort = ::IORegisterForSystemPower(0, &notifyPortRef, ToolkitSleepWakeCallback, &mPowerNotifier);
  if (gRootPort == MACH_PORT_NULL) {
    NS_ERROR("IORegisterForSystemPower failed");
    return NS_ERROR_FAILURE;
  }

  mSleepWakeNotificationRLS = ::IONotificationPortGetRunLoopSource(notifyPortRef);
  ::CFRunLoopAddSource(::CFRunLoopGetCurrent(),
                       mSleepWakeNotificationRLS,
                       kCFRunLoopDefaultMode);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void
nsToolkit::RemoveSleepWakeNotifcations()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mSleepWakeNotificationRLS) {
    ::IODeregisterForSystemPower(&mPowerNotifier);
    ::CFRunLoopRemoveSource(::CFRunLoopGetCurrent(),
                            mSleepWakeNotificationRLS,
                            kCFRunLoopDefaultMode);

    mSleepWakeNotificationRLS = nsnull;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

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
  gRollupListener->Rollup(nsnull, nsnull);
  return event;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NULL);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't do this for apps that (like Camino) use native context menus.
#ifdef MOZ_USE_NATIVE_POPUP_WINDOWS
  return;
#endif /* MOZ_USE_NATIVE_POPUP_WINDOWS */

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsToolkit::UnregisterAllProcessMouseEventHandlers()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mEventTapRLS) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mEventTapRLS,
                          kCFRunLoopDefaultMode);
    CFRelease(mEventTapRLS);
    mEventTapRLS = nsnull;
  }
  if (mEventTapPort) {
    // mEventTapPort must be invalidated as well as released.  Otherwise the
    // event tap doesn't get destroyed until the browser process ends (it
    // keeps showing up in the list returned by CGGetEventTapList()).
    CFMachPortInvalidate(mEventTapPort);
    CFRelease(mEventTapPort);
    mEventTapPort = nsnull;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
NS_IMETHODIMP NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

PRInt32 nsToolkit::OSXVersion()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  static PRInt32 gOSXVersion = 0x0;
  if (gOSXVersion == 0x0) {
    OSErr err = ::Gestalt(gestaltSystemVersion, (SInt32*)&gOSXVersion);
    if (err != noErr) {
      // This should probably be changed when our minimum version changes
      NS_ERROR("Couldn't determine OS X version, assuming 10.5");
      gOSXVersion = MAC_OS_X_VERSION_10_5_HEX;
    }
  }
  return gOSXVersion;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

bool nsToolkit::OnSnowLeopardOrLater()
{
  return (OSXVersion() >= MAC_OS_X_VERSION_10_6_HEX);
}

bool nsToolkit::OnLionOrLater()
{
  return (OSXVersion() >= MAC_OS_X_VERSION_10_7_HEX);
}

// An alternative to [NSObject poseAsClass:] that isn't deprecated on OS X
// Leopard and is available to 64-bit binaries on Leopard and above.  Based on
// ideas and code from http://www.cocoadev.com/index.pl?MethodSwizzling.
// Since the Method type becomes an opaque type as of Objective-C 2.0, we'll
// have to switch to using accessor methods like method_exchangeImplementations()
// when we build 64-bit binaries that use Objective-C 2.0 (on and for Leopard
// and above).  But these accessor methods aren't available in Objective-C 1
// (or on Tiger).  So we need to access Method's members directly for (Tiger-
// capable) binaries (32-bit or 64-bit) that use Objective-C 1 (as long as we
// keep supporting Tiger).
//
// Be aware that, if aClass doesn't have an orgMethod selector but one of its
// superclasses does, the method substitution will (in effect) take place in
// that superclass (rather than in aClass itself).  The substitution has
// effect on the class where it takes place and all of that class's
// subclasses.  In order for method swizzling to work properly, posedMethod
// needs to be unique in the class where the substitution takes place and all
// of its subclasses.
nsresult nsToolkit::SwizzleMethods(Class aClass, SEL orgMethod, SEL posedMethod,
                                   bool classMethods)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  Method original = nil;
  Method posed = nil;

  if (classMethods) {
    original = class_getClassMethod(aClass, orgMethod);
    posed = class_getClassMethod(aClass, posedMethod);
  } else {
    original = class_getInstanceMethod(aClass, orgMethod);
    posed = class_getInstanceMethod(aClass, posedMethod);
  }

  if (!original || !posed)
    return NS_ERROR_FAILURE;

#ifdef __LP64__
  method_exchangeImplementations(original, posed);
#else
  IMP aMethodImp = original->method_imp;
  original->method_imp = posed->method_imp;
  posed->method_imp = aMethodImp;
#endif

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#ifndef __LP64__

void ScanImportedFunctions(const struct mach_header* mh, intptr_t vmaddr_slide);

int gInWebInitForCarbonLevel = 0;

void Hooked_WebInitForCarbon();
OSStatus Hooked_InstallEventLoopIdleTimer(
   EventLoopRef inEventLoop,
   EventTimerInterval inDelay,
   EventTimerInterval inInterval,
   EventLoopIdleTimerUPP inTimerProc,
   void *inTimerData,
   EventLoopTimerRef *outTimer
);

void (*WebKit_WebInitForCarbon)() = NULL;
OSStatus (*HIToolbox_InstallEventLoopIdleTimer)(
   EventLoopRef inEventLoop,
   EventTimerInterval inDelay,
   EventTimerInterval inInterval,
   EventLoopIdleTimerUPP inTimerProc,
   void *inTimerData,
   EventLoopTimerRef *outTimer
) = NULL;

typedef struct _nsHookedFunctionSpec {
  const char *name;     // Includes leading underscore
  void *newAddress;
  void **oldAddressPtr;
} nsHookedFunctionSpec;

nsHookedFunctionSpec gHookedFunctions[] = {
  {"_WebInitForCarbon", (void *) Hooked_WebInitForCarbon,
    (void **) &WebKit_WebInitForCarbon},
  {"_InstallEventLoopIdleTimer", (void *) Hooked_InstallEventLoopIdleTimer,
    (void **) &HIToolbox_InstallEventLoopIdleTimer},
  {NULL, NULL, NULL}
};

// Plugins may exist that use the WebKit framework.  Those that are
// Carbon-based need to call WebKit's WebInitForCarbon() method.  There
// currently appears to be only one Carbon WebKit plugin --
// DivXBrowserPlugin (included with the DivX Web Player,
// http://www.divx.com/en/downloads/divx/mac).  See bug 509130.
//
// The source-code for WebInitForCarbon() is in the WebKit source tree's
// WebKit/mac/Carbon/CarbonUtils.mm file.  Among other things it installs
// an idle timer on the main event loop, whose target is the PoolCleaner()
// function (also in CarbonUtils.mm).  WebInitForCarbon() allocates an
// NSAutoreleasePool object which it stores in the global sPool variable.
// PoolCleaner() periodically releases/drains sPool and creates another
// NSAutoreleasePool object to take its place.  The intention is to ensure
// an autorelease pool is in place for whatever Objective-C code may be
// called by WebKit code, and that it periodically gets "cleaned".  But we're
// already doing this ourselves.  And PoolCleaner()'s periodic cleaning has a
// very bad effect on us -- it causes objects to be deleted prematurely, so
// that attempts to access them cause crashes.  This is probably because, when
// WebInitForCarbon() is called from a plugin, one or more autorelease pools
// are already in place.
//
// To get around this we hook/subclass WebInitForCarbon() and
// InstallEventLoopIdleTimer() and make the latter return without doing
// anything when called from the former.  This stops WebInitForCarbon()'s
// (useless and harmful) idle timer from ever being installed.
//
// PoolCleaner() only "works" if the autorelease pool count (returned by
// WKGetNSAutoreleasePoolCount(), stored in numPools) is the same as when
// sPool was last set.  But WKGetNSAutoreleasePoolCount() only works on OS X
// 10.5 and below.  So PoolCleaner() always fails 10.6 and above, and we
// needn't do anything there.
//
// WKGetNSAutoreleasePoolCount() is a thin wrapper around the following code:
//
//   unsigned count = NSPushAutoreleasePool(0);
//   NSPopAutoreleasePool(count);
//   return count;
//
// NSPushAutoreleasePool() and NSPopAutoreleasePool() are undocumented
// functions from the Foundation framework.  On OS X 10.5.X and below their
// declarations are (as best I can tell) as follows.  ('capacity' is
// presumably the initial capacity, in number of items, of the autorelease
// pool to be created.)
//
//   unsigned NSPushAutoreleasePool(unsigned capacity);
//   void NSPopAutoreleasePool(unsigned offset);
//
// But as of OS X 10.6 these functions appear to have changed as follows:
//
//   AutoreleasePool *NSPushAutoreleasePool(unsigned capacity);
//   void NSPopAutoreleasePool(AutoreleasePool *aPool);

void Hooked_WebInitForCarbon()
{
  ++gInWebInitForCarbonLevel;
  WebKit_WebInitForCarbon();
  --gInWebInitForCarbonLevel;
}

OSStatus Hooked_InstallEventLoopIdleTimer(
   EventLoopRef inEventLoop,
   EventTimerInterval inDelay,
   EventTimerInterval inInterval,
   EventLoopIdleTimerUPP inTimerProc,
   void *inTimerData,
   EventLoopTimerRef *outTimer
)
{
  OSStatus rv = noErr;
  if (gInWebInitForCarbonLevel <= 0) {
    rv = HIToolbox_InstallEventLoopIdleTimer(inEventLoop, inDelay, inInterval,
                                             inTimerProc, inTimerData, outTimer);
  }
  return rv;
}

// Try to hook (or "subclass") the dynamically bound functions specified in
// gHookedFunctions.  We don't hook these functions at their "original"
// addresses, so we can only "subclass" calls to them from modules other than
// the one in which they're defined.  Of course, this only works for globally
// accessible functions.
void HookImportedFunctions()
{
  // We currently only need to do anything on Tiger or Leopard.
  if (nsToolkit::OnSnowLeopardOrLater())
    return;

  // _dyld_register_func_for_add_image() makes the dynamic linker runtime call
  // ScanImportedFunctions() "once for each of the images that are currently
  // loaded into the program" (including the main image, i.e. firefox-bin).
  // When a new image is added (e.g. a plugin), ScanImportedFunctions() is
  // called again with data for that image.
  //
  // Calling HookImportedFunctions() from loadHandler's constructor (i.e. as
  // the current module is being loaded) minimizes the likelihood that the
  // imported functions in the already-loaded images will get called while
  // we're resetting their pointers.
  //
  // _dyld_register_func_for_add_image()'s behavior when a new image is added
  // allows us to reset its imported functions' pointers before they ever get
  // called.
  _dyld_register_func_for_add_image(ScanImportedFunctions);
}

struct segment_command *GetSegmentFromMachHeader(const struct mach_header* mh,
                                                 const char *segname,
                                                 uint32_t *numFollowingCommands)
{
  if (numFollowingCommands)
    *numFollowingCommands = 0;
  uint32_t numCommands = mh->ncmds;
  struct segment_command *aCommand = (struct segment_command *)
    ((uint32_t)mh + sizeof(struct mach_header));
  for (uint32_t i = 1; i <= numCommands; ++i) {
    if (aCommand->cmd != LC_SEGMENT)
      return NULL;
    if (strcmp(segname, aCommand->segname) == 0) {
      if (numFollowingCommands)
        *numFollowingCommands = numCommands-i;
      return aCommand;
    }
    aCommand = (struct segment_command *)
      ((uint32_t)aCommand + aCommand->cmdsize);
  }
  return NULL;
}

// Scan through parts of the "indirect symbol table" for imported functions
// (functions dynamically bound from another module) whose names match those
// we're trying to hook.  If we find one, change the corresponding pointer/
// instruction in a "jump table" or "lazy pointer array" to point at the
// function's replacement.  It appears we only need to look at "lazy bound"
// symbols -- non-"lazy" symbols seem to always be for (imported) data.  (A
// lazy bound symbol is one that's only resolved on first "use".)
//
// Most of what we do here is documented by Apple
// (http://developer.apple.com/Mac/library/documentation/DeveloperTools/Conceptual/MachORuntime/Reference/reference.html,
// http://developer.apple.com/mac/library/documentation/DeveloperTools/Reference/MachOReference/Reference/reference.html).
// When Apple doesn't explicitly document something (e.g. the format of the
// __LINKEDIT segment or the indirect symbol table), you can often get "hints"
// from the output of 'otool -l' or 'otool -I".  And sometimes mach-o header
// files contain additional information -- for example the format of the
// indirect symbol table is described in the comment above the definitions of
// INDIRECT_SYMBOL_LOCAL and INDIRECT_SYMBOL_ABS in mach-o/loader.h.
//
// The "__jump_table" section of the "__IMPORT" segment is an array of
// assembler JMP or CALL instructions.  It's only present in i386 binaries
// (ppc and x86_64 binaries use arrays of pointers).  Each instruction is
// 5 bytes long.  The format is a byte-length opcode (0xE9 for JMP, 0xE8 for
// CALL) followed by a four-byte relative address (relative to the start of
// the next instruction in the table).  All the CALL instructions point to the
// same code -- a 'dyld_stub_binding_helper()' that somehow locates the lazy-
// bound function and replaces the CALL instruction with a JMP instruction
// to the appropriate function.  If we replace the CALL instruction ourselves,
// dyld_stub_binding_helper() never gets called (and never needs to be).
void ScanImportedFunctions(const struct mach_header* mh, intptr_t vmaddr_slide)
{
  // While we're looking through all our images/modules, also scan for the
  // original addresses of the functions we plan to hook.  Though
  // NSLookupSymbolInImage() is deprecated (along with the entire NSModule
  // API), it's by far the best (and most efficient) way to do what we need
  // to do here (scan for the original addresses of symbols that aren't all
  // loaded at the same time).  It's still available to 64-bit apps on OS X
  // 10.6.X.
  for (uint32_t i = 0; gHookedFunctions[i].name; ++i) {
    // Since a symbol might be defined more than once, we record only its
    // "first" address.
    if (*gHookedFunctions[i].oldAddressPtr)
      continue;
    NSSymbol symbol =
      NSLookupSymbolInImage(mh, gHookedFunctions[i].name,
                            NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if (symbol)
      *gHookedFunctions[i].oldAddressPtr = NSAddressOfSymbol(symbol);
  }

  uint32_t numFollowingCommands = 0;
  struct segment_command *linkeditSegment =
    GetSegmentFromMachHeader(mh, "__LINKEDIT", &numFollowingCommands);
  if (!linkeditSegment)
    return;
  uint32_t fileoffIncrement = linkeditSegment->vmaddr - linkeditSegment->fileoff;

  struct symtab_command *symtab =
    (struct symtab_command *)((uint32_t)linkeditSegment + linkeditSegment->cmdsize);
  for (uint32_t i = 1;; ++i) {
    if (symtab->cmd == LC_SYMTAB)
      break;
    if (i == numFollowingCommands)
      return;
    symtab = (struct symtab_command *) ((uint32_t)symtab + symtab->cmdsize);
  }
  uint32_t symbolTableOffset = symtab->symoff + fileoffIncrement + vmaddr_slide;
  uint32_t stringTableOffset = symtab->stroff + fileoffIncrement + vmaddr_slide;

  struct dysymtab_command *dysymtab =
    (struct dysymtab_command *)((uint32_t)symtab + symtab->cmdsize);
  if (dysymtab->cmd != LC_DYSYMTAB)
    return;
  uint32_t indirectSymbolTableOffset =
    dysymtab->indirectsymoff + fileoffIncrement + vmaddr_slide;

  // Some i386 binaries on OS X 10.6.X use a __la_symbol_ptr section (in the
  // __DATA segment) instead of a __jump_table section (in the __IMPORT
  // segment).
  const struct section *lazySymbols = NULL;
#ifdef __i386__
  struct segment_command *importSegment =
    GetSegmentFromMachHeader(mh, "__IMPORT", nil);
  const struct section *jumpTable =
    getsectbynamefromheader(mh, "__IMPORT", "__jump_table");
  if (!jumpTable)
#endif
  {
    lazySymbols = getsectbynamefromheader(mh, "__DATA", "__la_symbol_ptr");
    if (!lazySymbols)
      return;
  }
  uint32_t numLazySymbols = 0;
  uint32_t lazyBytes = 0;
  unsigned char *lazy = NULL;
#ifdef __i386__
  uint32_t numJumpTableStubs = 0;
  uint32_t stubsBytes = 0;
  unsigned char *stubs = NULL;
  vm_prot_t importSegProt = VM_PROT_NONE;
  if (jumpTable) {
    // Bail if we don't have an __IMPORT segment (which shouldn't be possible,
    // but just in case).
    if (!importSegment)
      return;
    importSegProt = importSegment->initprot;
    // Bail if the size of each entry in the "jump table" isn't 5 bytes.
    if (jumpTable->reserved2 != 5)
      return;
    numJumpTableStubs = jumpTable->size/5;
    indirectSymbolTableOffset += jumpTable->reserved1*sizeof(uint32_t);
    stubs = (unsigned char *)
      (getsectdatafromheader(mh, "__IMPORT", "__jump_table", &stubsBytes) + vmaddr_slide);
    // Bail if (for some reason) these figures don't agree.
    if (stubsBytes != jumpTable->size)
      return;
  } else
#endif
  {
    numLazySymbols = lazySymbols->size/4;
    indirectSymbolTableOffset += lazySymbols->reserved1*sizeof(uint32_t);
    lazy = (unsigned char *)
      (getsectdatafromheader(mh, "__DATA", "__la_symbol_ptr", &lazyBytes) + vmaddr_slide);
  }

  uint32_t items = 0;
#ifdef __i386__
  if (jumpTable) {
    items = numJumpTableStubs;
    // If the __IMPORT segment is read-only, we'll need to make it writeable
    // before trying to change entries in its jump table.  Below we restore
    // its original level of protection.
    if (!(importSegProt & VM_PROT_WRITE)) {
      void *protAddr = (void *) (importSegment->vmaddr + vmaddr_slide);
      size_t protSize = importSegment->vmsize;
      vm_protect(mach_task_self(), (vm_address_t) protAddr, protSize, NO,
                 importSegProt | VM_PROT_WRITE);
    }
  } else
#endif
  {
    items = numLazySymbols;
  }
  uint32_t *indirectSymbolTableItem = (uint32_t *) indirectSymbolTableOffset;
  for (uint32_t i = 0; i < items; ++i, ++indirectSymbolTableItem) {
    // Skip indirect symbol table items that are 0x80000000 (for a local
    // symbol) and/or 0x40000000 (for an absolute symbol).  See
    // mach-o/loader.h.
    if (0xF0000000 & *indirectSymbolTableItem)
      continue;
    struct nlist *symbolTableItem = (struct nlist *)
      (symbolTableOffset + *indirectSymbolTableItem*sizeof(struct nlist));
    char *stringTableItem = (char *) (stringTableOffset + symbolTableItem->n_un.n_strx);

    for (uint32_t j = 0; gHookedFunctions[j].name; ++j) {
      if (strcmp(stringTableItem, gHookedFunctions[j].name) != 0)
        continue;
#ifdef __i386__
      if (jumpTable) {
        unsigned char *opcodeAddr = stubs + (i * 5);
        int32_t *displacementAddr = (int32_t *) (opcodeAddr + 1);
        int32_t eip = (int32_t) stubs + (i + 1) * 5;
        int32_t displacement = (int32_t) (gHookedFunctions[j].newAddress) - eip;
        displacementAddr[0] = displacement;
        opcodeAddr[0] = 0xE9;
      } else
#endif
      {
        int32_t *lazySymbolAddr = (int32_t *) (lazy + (i * 4));
        lazySymbolAddr[0] = (int32_t) (gHookedFunctions[j].newAddress);
      }
      break;
    }
  }

#ifdef __i386__
  // If we needed to make an __IMPORT segment writeable above, restore its
  // original protection level here.
  if (jumpTable && !(importSegProt & VM_PROT_WRITE)) {
    void *protAddr = (void *) (importSegment->vmaddr + vmaddr_slide);
    size_t protSize = importSegment->vmsize;
    vm_protect(mach_task_self(), (vm_address_t) protAddr, protSize,
               NO, importSegProt);
  }
#endif
}

class loadHandler
{
public:
  loadHandler();
  ~loadHandler() {}
};

loadHandler::loadHandler()
{
  // Calling HookImportedFunctions() from here (i.e. as the current module is
  // being loaded) minimizes the likelihood that the imported functions in
  // the already-loaded images will get called while we're resetting their
  // pointers.
  HookImportedFunctions();
}

loadHandler handler = loadHandler();

#endif // __LP64__
