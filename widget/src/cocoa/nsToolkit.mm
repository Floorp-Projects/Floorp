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
 * The Original Code is mozilla.org code.
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

#include "nsToolkit.h"
#include "nsWidgetAtoms.h"

#include <Gestalt.h>
#include <Appearance.h>

#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"

// for some reason, this must come last. otherwise the appshell 
// component fails to instantiate correctly at runtime.
#undef DARWIN
#import <Cocoa/Cocoa.h>

static CFBundleRef getBundle(CFStringRef frameworkPath)
{
  CFBundleRef bundle = NULL;
 
  //	Make a CFURLRef from the CFString representation of the bundle's path.
  //	See the Core Foundation URL Services chapter for details.
  CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, frameworkPath, kCFURLPOSIXPathStyle, true);
  if (bundleURL != NULL) {
    bundle = CFBundleCreate(NULL, bundleURL);
    if (bundle != NULL)
      CFBundleLoadExecutable(bundle);
    CFRelease(bundleURL);
  }

  return bundle;
}

static void* getQDFunction(CFStringRef functionName)
{
  static CFBundleRef systemBundle = getBundle(CFSTR("/System/Library/Frameworks/ApplicationServices.framework"));
  if (systemBundle)
    return CFBundleGetFunctionPointerForName(systemBundle, functionName);
  return NULL;
}


//
// interface EventQueueHanlder
//
// An object that handles processing events for the PLEvent queue
// on each thread.
//
@interface EventQueueHandler : NSObject
{
  nsIEventQueue*  mMainThreadEventQueue;    // addreffed
  NSTimer*        mEventTimer;              // our timer [STRONG]
}

- (void)eventTimer:(NSTimer *)theTimer;

@end


static EventQueueHandler*  gEventQueueHandler = nsnull;


//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;


@implementation EventQueueHandler

//
// -init
//
// Do init stuff. Cache the EventQueue Service and kick off our repeater
// to process PLEvents. The toolkit owns the timer so that neither Mozilla
// nor embedding apps need to worry about polling to process these events.
//
- (id)init
{
  if ( (self = [super init]) )
  {
    nsCOMPtr<nsIEventQueueService> service = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
    if (!service) {
      [self release];
      return nil;
    }
    
    service->GetThreadEventQueue(NS_CURRENT_THREAD, &mMainThreadEventQueue);   // addref

    mEventTimer = [NSTimer scheduledTimerWithTimeInterval:0.005
                              target:self
                              selector:@selector(eventTimer:)
                              userInfo:nil
                              repeats:YES];
    if (!mMainThreadEventQueue || !mEventTimer) {
      [self release];
      return nil;
    }
  }
  
  return self;
}


//
// -dealloc
//
// When we're finally ready to go away, kill our timer and get rid of the
// EventQueue Service. Also set the global var to null so in case we're called
// into action again we'll start over and not try to re-use the deleted object.
//
- (void) dealloc
{
#if DEBUG
  printf("shutting down event queue\n");
#endif
  
  [mEventTimer release];
  NS_IF_RELEASE(mMainThreadEventQueue);
  
  gEventQueueHandler = nsnull;
  
  [super dealloc];
}


//
// -eventTimer
//
// Called periodically to process PLEvents from the queue on the current thread
//

#define MAX_PROCESS_EVENT_CALLS        20

//#define DEBUG_EVENT_TIMING

//#define TIMED_EVENT_PROCESSING
#define MAX_PLEVENT_TIME_MILLISECONDS  500


- (void)eventTimer:(NSTimer *)theTimer
{
#ifdef DEBUG_EVENT_TIMING
  AbsoluteTime startTime = ::UpTime();
#endif
  
  if (mMainThreadEventQueue)
  {
#ifdef TIMED_EVENT_PROCESSING
    // the new way; process events until some time has elapsed, or there are
    // no events left. UpTime() is a very low-overhead way to measure time.
    AbsoluteTime bailTime = ::AddDurationToAbsolute(MAX_PLEVENT_TIME_MILLISECONDS * durationMillisecond, ::UpTime());
    while (1)
    {
      PRBool pendingEvents = PR_FALSE;
      mMainThreadEventQueue->PendingEvents(&pendingEvents);
      if (!pendingEvents)
        break;
      mMainThreadEventQueue->ProcessPendingEvents();
      
      AbsoluteTime now = ::UpTime();
      if (UnsignedWideToUInt64(now) > UnsignedWideToUInt64(bailTime))
        break;
    }
#else
    // the old way; process events 20 times. Can suck CPU, and make the app
    // unresponsive
    for (PRInt32 i = 0; i < MAX_PROCESS_EVENT_CALLS; i ++)
    {
      PRBool pendingEvents = PR_FALSE;
      mMainThreadEventQueue->PendingEvents(&pendingEvents);
      if (!pendingEvents)
        break;
      mMainThreadEventQueue->ProcessPendingEvents();
    } 
#endif
  }

#ifdef DEBUG_EVENT_TIMING
  Nanoseconds duration = ::AbsoluteDeltaToNanoseconds(::UpTime(), startTime);
  UInt32 milliseconds = UnsignedWideToUInt64(duration) / 1000000;
    
  static UInt32 sMaxDuration = 0;
  
  if (milliseconds > sMaxDuration)
    sMaxDuration = milliseconds;
  
  printf("Event handling took %u ms (max %u)\n", milliseconds, sMaxDuration);
#endif
  
}


@end


#pragma mark -

NS_IMPL_THREADSAFE_ISUPPORTS1(nsToolkit, nsIToolkit);


// assume we begin as the fg app
bool nsToolkit::sInForeground = true;

static const char* gQuartzRenderingPref = "browser.quartz.enable";

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
: mInited(false)
{
  NS_INIT_ISUPPORTS();
  
  if (!gEventQueueHandler)
  {
    // autorelease this so that if Init is never called, it is not
    // leaked. Init retains it.
    gEventQueueHandler = [[[EventQueueHandler alloc] init] autorelease];
  }
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{ 
  nsWidgetAtoms::ReleaseAtoms();
  
  /* Decrement our refcount on gEventQueueHandler; a prelude toward
     stopping event handling. This is not something you want to do unless you've
     bloody well started event handling and incremented the refcount. That's
     done in the Init method, not the constructor, and that's what mInited is about.
  */
  if (mInited && gEventQueueHandler)
    [gEventQueueHandler release];
  
  // Remove the TLS reference to the toolkit...
  PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsToolkit::Init(PRThread */*aThread*/)
{
  if (!gEventQueueHandler)
    return NS_ERROR_FAILURE;
  
  [gEventQueueHandler retain];

  nsWidgetAtoms::AddRefAtoms();

  mInited = true;

  SetupQuartzRendering();
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if ( prefs )
    prefs->RegisterCallback(gQuartzRenderingPref, QuartzChangedCallback, nsnull);

  return NS_OK;
}


//
// QuartzChangedCallback
//
// The pref changed, reset the app to use quartz rendering as dictated by the pref
//
int nsToolkit::QuartzChangedCallback(const char* pref, void* data)
{
  SetupQuartzRendering();
  return NS_OK;
}


//
// SetupQuartzRendering
//
// Use apple's technote for 10.1.5 to turn on quartz rendering with CG metrics. This
// slows us down about 12% when turned on.
//
void nsToolkit::SetupQuartzRendering()
{
  // from Apple's technote, yet un-numbered.
#if UNIVERSAL_INTERFACES_VERSION <= 0x0400
  enum {
    kQDDontChangeFlags = 0xFFFFFFFF,         // don't change anything
    kQDUseDefaultTextRendering = 0,          // bit 0
    kQDUseTrueTypeScalerGlyphs = (1 << 0),   // bit 1
    kQDUseCGTextRendering = (1 << 1),        // bit 2
    kQDUseCGTextMetrics = (1 << 2)
  };
#endif
  const int kFlagsWeUse = kQDUseCGTextRendering | kQDUseCGTextMetrics;
  
  // turn on quartz rendering if we find the symbol in the app framework. Just turn
  // on the bits that we need, don't turn off what someone else might have wanted. If
  // the pref isn't found, assume we want it on. That way, we have to explicitly put
  // in a pref to disable it, rather than force everyone who wants it to carry around
  // an extra pref.
  typedef UInt32 (*qd_procptr)(UInt32);  
  static qd_procptr SwapQDTextFlags = (qd_procptr) getQDFunction(CFSTR("SwapQDTextFlags"));
  if ( SwapQDTextFlags ) {
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
    if (!prefs)
      return;
    PRBool enableQuartz = PR_TRUE;
    nsresult rv = prefs->GetBoolPref(gQuartzRenderingPref, &enableQuartz);
    UInt32 oldFlags = SwapQDTextFlags(kQDDontChangeFlags);
    if ( NS_FAILED(rv) || enableQuartz )
      SwapQDTextFlags(oldFlags | kFlagsWeUse);
    else 
      SwapQDTextFlags(oldFlags & !kFlagsWeUse);
  }
}

//-------------------------------------------------------------------------
//
// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  nsIToolkit* toolkit = nsnull;
  nsresult rv = NS_OK;
  PRStatus status;

  // Create the TLS index the first time through...
  if (0 == gToolkitTLSIndex) {
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
    if (PR_FAILURE == status) {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);

    //
    // Create a new toolkit for this thread...
    //
    if (!toolkit) {
      toolkit = new nsToolkit();

      if (!toolkit) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        NS_ADDREF(toolkit);
        toolkit->Init(PR_GetCurrentThread());
        //
        // The reference stored in the TLS is weak.  It is removed in the
        // nsToolkit destructor...
        //
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
      }
    } else {
      NS_ADDREF(toolkit);
    }
    *aResult = toolkit;
  }

  return rv;
}
