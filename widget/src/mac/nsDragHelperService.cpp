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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDragHelperService.h"

#include "nsGUIEvent.h"
#include "nsIDOMNode.h"
#include "nsIDragSessionMac.h"
#include "nsIServiceManager.h"

#define kDragServiceContractID "@mozilla.org/widget/dragservice;1"


NS_IMPL_ADDREF(nsDragHelperService)
NS_IMPL_RELEASE(nsDragHelperService)
NS_IMPL_QUERY_INTERFACE1(nsDragHelperService, nsIDragHelperService)


//
// nsDragHelperService constructor
//
nsDragHelperService::nsDragHelperService()
: mDragOverTimer(nsnull)
, mDragOverDragRef(nsnull)
{
}


//
// nsDragHelperService destructor
//
nsDragHelperService::~nsDragHelperService()
{
  if (mDragOverTimer)
    ::RemoveEventLoopTimer(mDragOverTimer);

  NS_ASSERTION(!mDragService.get(),
               "A drag was not correctly ended by shutdown");
}


//
// Enter
//
// Called when the mouse has entered the rectangle bounding the browser
// during a drag. Cache the drag service so we don't have to fetch it
// repeatedly, and handles setting up the drag reference and sending an
// enter event.
//
NS_IMETHODIMP
nsDragHelperService::Enter(DragReference inDragRef, nsIEventSink *inSink)
{
  // get our drag service for the duration of the drag.
  mDragService = do_GetService(kDragServiceContractID);
  NS_ASSERTION(mDragService,
               "Couldn't get a drag service, we're in biiig trouble");
  if (!mDragService || !inSink)
    return NS_ERROR_FAILURE;

  // tell the session about this drag
  mDragService->StartDragSession();
  nsCOMPtr<nsIDragSessionMac> macSession(do_QueryInterface(mDragService));
  if (macSession)
    macSession->SetDragReference(inDragRef);

  DoDragAction(NS_DRAGDROP_ENTER, inDragRef, inSink);

  // Start sending regular NS_DRAGDROP_OVER events.
  SetDragOverTimer(inSink, inDragRef);

  return NS_OK;
}


//
// Tracking
//
// Called while the mouse is moved inside the rectangle bounding the browser
// during a drag. The important thing done here is to clear the |canDrop|
// property of the drag session every time through. The event handlers
// will reset it if appropriate.
//
NS_IMETHODIMP
nsDragHelperService::Tracking(DragReference inDragRef, nsIEventSink *inSink,
                              PRBool* outDropAllowed)
{
  NS_ASSERTION(mDragService,
               "Couldn't get a drag service, we're in biiig trouble");
  if (!mDragService || !inSink) {
    *outDropAllowed = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  // clear out the |canDrop| property of the drag session. If it's meant to
  // be, it will be set again.
  nsCOMPtr<nsIDragSession> session;
  mDragService->GetCurrentSession(getter_AddRefs(session));
  NS_ASSERTION(session, "If we don't have a drag session, we're fucked");
  if (session)
    session->SetCanDrop(PR_FALSE);

  // mDragOverTimer is running, but an NS_DRAGDROP_OVER event is about to be
  // sent.  Make sure that the timer doesn't send another event too quickly.
  SetDragOverTimer(inSink, inDragRef);

  DoDragAction(NS_DRAGDROP_OVER, inDragRef, inSink);

  // check if gecko has since allowed the drop and return it
  if (session)
    session->GetCanDrop(outDropAllowed);

  return NS_OK;
}


//
// Leave
//
// Called when the mouse leaves the rectangle bounding the browser
// during a drag. Cleans up the drag service and releases it.
//
NS_IMETHODIMP
nsDragHelperService::Leave(DragReference inDragRef, nsIEventSink *inSink)
{
  // Stop sending NS_DRAGDROP_OVER events.
  SetDragOverTimer(nsnull, nsnull);

  NS_ASSERTION(mDragService,
               "Couldn't get a drag service, we're in biiig trouble");
  if (!mDragService || !inSink)
    return NS_ERROR_FAILURE;

  // clear out the dragRef in the drag session. We are guaranteed that
  // this will be called _after_ the drop has been processed (if there
  // is one), so we're not destroying valuable information if the drop
  // was in our window.
  nsCOMPtr<nsIDragSessionMac> macSession(do_QueryInterface(mDragService));
  if (macSession)
    macSession->SetDragReference(0);

  DoDragAction(NS_DRAGDROP_EXIT, inDragRef, inSink);

#ifndef MOZ_WIDGET_COCOA
  ::HideDragHilite(inDragRef);
#endif

  nsCOMPtr<nsIDragSession> currentDragSession;
  mDragService->GetCurrentSession(getter_AddRefs(currentDragSession));

  if (currentDragSession) {
    nsCOMPtr<nsIDOMNode> sourceNode;
    currentDragSession->GetSourceNode(getter_AddRefs(sourceNode));

    if (!sourceNode) {
      // We're leaving a window while doing a drag that was
      // initiated in a differnt app. End the drag session,
      // since we're done with it for now (until the user
      // drags back into mozilla).
      mDragService->EndDragSession(PR_FALSE);
    }
  }

  // we're _really_ done with it, so let go of the service.
  mDragService = nsnull;

  return NS_OK;
}


//
// Drop
//
// Called when a drop occurs within the rectangle bounding the browser
// during a drag. Cleans up the drag service and releases it.
//
NS_IMETHODIMP
nsDragHelperService::Drop(DragReference inDragRef, nsIEventSink *inSink,
                          PRBool* outAccepted)
{
  // Stop sending NS_DRAGDROP_OVER events.
  SetDragOverTimer(nsnull, nsnull);

  NS_ASSERTION(mDragService,
               "Couldn't get a drag service, we're in biiig trouble");
  if (!mDragService || !inSink) {
    *outAccepted = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  // We make the assuption that the dragOver handlers have correctly set
  // the |canDrop| property of the Drag Session. Before we dispatch the event
  // into Gecko, check that value and either dispatch it or set the result
  // code to "spring-back" and show the user the drag failed.
  OSErr result = noErr;
  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession) {
    // if the target has set that it can accept the drag, pass along
    // to gecko, otherwise set phasers for failure.
    PRBool canDrop = PR_FALSE;
    if (NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)))
      if (canDrop)
        DoDragAction(NS_DRAGDROP_DROP, inDragRef, inSink);
      else
        result = dragNotAcceptedErr;
  } // if a valid drag session

  // if there was any kind of error, the drag wasn't accepted
  *outAccepted = (result == noErr);

  return NS_OK;
}


// DoDragAction
//
// Common routine for NS_DRAGDROP_(ENTER|EXIT|OVER|DROP) events.  Obtains
// information about the drag in progress and dispatches the event.
//
// protected
PRBool
nsDragHelperService::DoDragAction(PRUint32      aMessage,
                                  DragReference aDragRef,
                                  nsIEventSink* aSink)
{
  // Get information about the drag.
  Point mouseLocGlobal;
  ::GetDragMouse(aDragRef, &mouseLocGlobal, nsnull);
  short modifiers;
  ::GetDragModifiers(aDragRef, &modifiers, nsnull, nsnull);

  // Set the drag action on the service so the frames know what is going on.
  SetDragActionBasedOnModifiers(modifiers);

  // Pass into Gecko for handling.
  PRBool handled = PR_FALSE;
  aSink->DragEvent(aMessage, mouseLocGlobal.h, mouseLocGlobal.v,
                   modifiers, &handled);

  return handled;
}


//
// SetDragActionsBasedOnModifiers
//
// Examines the MacOS modifier keys and sets the appropriate drag action on the
// drag session to copy/move/etc
//
void
nsDragHelperService::SetDragActionBasedOnModifiers(short inModifiers)
{
  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession) {
    PRUint32 action = nsIDragService::DRAGDROP_ACTION_MOVE;

    // force copy = option, alias = cmd-option, default is move
    if (inModifiers & optionKey) {
      if (inModifiers & cmdKey)
        action = nsIDragService::DRAGDROP_ACTION_LINK;
      else
        action = nsIDragService::DRAGDROP_ACTION_COPY;
    }

    dragSession->SetDragAction(action);
  }

} // SetDragActionBasedOnModifiers


// SetDragOverTimer
//
// Sets a timer to send NS_DRAGDROP_OVER events into Gecko at regular
// intervals.  If a timer is already running, the interval is reset.
// If either argument is null, any running timer is stopped.
//
// protected
void
nsDragHelperService::SetDragOverTimer(nsIEventSink* aSink,
                                      DragRef       aDragRef)
{
  // win32 seems to use 16Hz internally, gtk2 has a 10Hz timer
  const EventTimerInterval kDragOverInterval = 1.0/10.0;

  if (!aSink || !aDragRef) {
    // Cancel the timer.
    if (mDragOverTimer) {
      ::RemoveEventLoopTimer(mDragOverTimer);
      mDragOverTimer = nsnull;
      mDragOverSink = nsnull;
    }
    return;
  }

  if (mDragOverTimer) {
    // A timer is already running, just bump its next-fire time.
    ::SetEventLoopTimerNextFireTime(mDragOverTimer, kDragOverInterval);
    return;
  }

  mDragOverSink = aSink;
  mDragOverDragRef = aDragRef;

  static EventLoopTimerUPP sDragOverTimerUPP;
  if (!sDragOverTimerUPP)
    sDragOverTimerUPP = ::NewEventLoopTimerUPP(DragOverTimerHandler);
 
  ::InstallEventLoopTimer(::GetCurrentEventLoop(),
                          kDragOverInterval,
                          kDragOverInterval,
                          sDragOverTimerUPP,
                          this,
                          &mDragOverTimer);
}


// DragOverTimerHandler
//
// Called at regular intervals to send NS_DRAGDROP_OVER events.
//
// protected static
void
nsDragHelperService::DragOverTimerHandler(EventLoopTimerRef aTimer,
                                          void* aUserData)
{
  nsDragHelperService* self = NS_STATIC_CAST(nsDragHelperService*, aUserData);

  self->DoDragAction(NS_DRAGDROP_OVER,
                     self->mDragOverDragRef, self->mDragOverSink);
}
