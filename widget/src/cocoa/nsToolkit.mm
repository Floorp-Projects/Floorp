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

#include "nsRepeater.h"

// for some reason, this must come last. otherwise the appshell 
// component fails to instantiate correctly at runtime.
#undef DARWIN
#import <Cocoa/Cocoa.h>


//
// interface EventQueueHanlder
//
// An object that handles processing events for the PLEvent queue
// on each thread.
//
@interface EventQueueHandler : NSObject
{
  nsIEventQueueService* mEventQueueService;
  NSTimer* mEventTimer;                                   // our timer [STRONG]
}
- (void)eventTimer:(NSTimer *)theTimer;
- (PRBool)eventsArePending;
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
  // lame, but we can't use an nsCOMPtr as a member variable
  nsCOMPtr<nsIEventQueueService> service = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
  mEventQueueService = service.get();
  NS_IF_ADDREF(mEventQueueService);  

  mEventTimer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(eventTimer:) userInfo:nil
                           repeats:YES];
  NS_ASSERTION(mEventTimer, "UH OH! couldn't create periodic event processing timer");
  
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
printf("shutting down event queue\n");
  if ( mEventTimer )
    [mEventTimer release];
  NS_IF_RELEASE(mEventQueueService);
  
  gEventQueueHandler = nsnull;
  
  [super dealloc];
}


//
// -eventTimer
//
// Called periodically to process PLEvents from the queue on the current thread
//
- (void)eventTimer:(NSTimer *)theTimer
{
  if ( mEventQueueService ) {
    nsCOMPtr<nsIEventQueue> queue;
    mEventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (queue) {
      nsresult rv = queue->ProcessPendingEvents();
      NS_ASSERTION(NS_SUCCEEDED(rv), "Error processing PLEvents");
    }
  }
  
  EventRecord anEvent;
	Repeater::DoIdlers(anEvent);
  Repeater::DoRepeaters(anEvent);
}


//
// -eventsArePending
//
// See if events are pending on the event queue on the current thread.
//
- (PRBool)eventsArePending
{
  PRBool pendingEvents = PR_FALSE;
  
  if ( mEventQueueService ) {
    nsCOMPtr<nsIEventQueue> queue;
    mEventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (queue)
      queue->PendingEvents(&pendingEvents);
  }

  return pendingEvents;
}


@end


#pragma mark -

NS_IMPL_THREADSAFE_ISUPPORTS1(nsToolkit, nsIToolkit);


// assume we begin as the fg app
bool nsToolkit::sInForeground = true;


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit() : mInited(false)
{
  NS_INIT_REFCNT();
  
  if ( !gEventQueueHandler )
    gEventQueueHandler = [[[EventQueueHandler alloc] init] autorelease];
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
  if (gEventQueueHandler)
    [gEventQueueHandler retain];

  nsWidgetAtoms::AddRefAtoms();

  mInited = true;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
PRBool nsToolkit::ToolkitBusy()
{
  return (gEventQueueHandler) ? [gEventQueueHandler eventsArePending] : PR_FALSE;
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
bool nsToolkit::HasAppearanceManager()
{

#define APPEARANCE_MIN_VERSION  0x0110    // we require version 1.1
  
  static bool inited = false;
  static bool hasAppearanceManager = false;

  if (inited)
    return hasAppearanceManager;
  inited = true;

  SInt32 result;
  if (::Gestalt(gestaltAppearanceAttr, &result) != noErr)
    return false;   // no Appearance Mgr

  if (::Gestalt(gestaltAppearanceVersion, &result) != noErr)
    return false;   // still version 1.0

  hasAppearanceManager = (result >= APPEARANCE_MIN_VERSION);

  return hasAppearanceManager;
}


void 
nsToolkit :: AppInForeground ( )
{
  sInForeground = true;
}


void 
nsToolkit :: AppInBackground ( )
{
  sInForeground = false;
} 


bool
nsToolkit :: IsAppInForeground ( )
{
  return sInForeground;
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
