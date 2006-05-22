/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 *  Mark Mentovai <mark@moxienet.com>
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

/*
 * Dispatcher for a variety of application-scope events.  The real event loop
 * is in nsAppShell.
 */

#include "nsMacMessagePump.h"

#include "nsIEventSink.h"
#include "nsIRollupListener.h"
#include "nsIWidget.h"
#include "nsPIWidgetMac.h"

#include "nsGfxUtils.h"
#include "nsGUIEvent.h"
#include "nsMacTSMMessagePump.h"
#include "nsToolkit.h"

#include <Carbon/Carbon.h>

#ifndef botRight
#define botRight(r) (((Point *) &(r))[1])
#endif

const short kMinWindowWidth = 125;
const short kMinWindowHeight = 150;

extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

nsMacMessagePump::nsMacMessagePump(nsToolkit *aToolkit)
: mToolkit(aToolkit)
, mMouseClickEventHandler(NULL)
, mWNETransitionEventHandler(NULL)
{
  NS_ASSERTION(mToolkit, "No toolkit");
  
  // This list encompasses Carbon events that can be converted into
  // EventRecords, and that still require dispatch through
  // nsMacMessagePump::DispatchEvent because they haven't yet been converted
  // completetly to Carbon events.
  const EventTypeSpec kWNETransitionEventList[] = {
    { kEventClassMouse,       kEventMouseDown },
    { kEventClassMouse,       kEventMouseUp },
    { kEventClassMouse,       kEventMouseMoved },
    { kEventClassMouse,       kEventMouseDragged },
    { kEventClassWindow,      kEventWindowUpdate },
    { kEventClassWindow,      kEventWindowActivated },
    { kEventClassWindow,      kEventWindowDeactivated },
    { kEventClassWindow,      kEventWindowCursorChange },
    { kEventClassApplication, kEventAppActivated },
    { kEventClassApplication, kEventAppDeactivated },
    { kEventClassAppleEvent,  kEventAppleEvent },
    { kEventClassControl,     kEventControlTrack },
  };

  static EventHandlerUPP sWNETransitionEventHandlerUPP;
  if (!sWNETransitionEventHandlerUPP)
    sWNETransitionEventHandlerUPP =
                               ::NewEventHandlerUPP(WNETransitionEventHandler);

  OSStatus err =
   ::InstallApplicationEventHandler(sWNETransitionEventHandlerUPP,
                                    GetEventTypeCount(kWNETransitionEventList),
                                    kWNETransitionEventList,
                                    NS_STATIC_CAST(void*, this),
                                    &mWNETransitionEventHandler);

  NS_ASSERTION(err == noErr, "Could not install WNETransitionEventHandler");

  // For middle clicks.  Install this handler second, because
  // WNETransitionEventHandler swallows all events, and MouseClickEventHandler
  // needs to be able to handle mouse-click events (punting non-middle-click
  // ones).
  const EventTypeSpec kMouseClickEventList[] = {
    { kEventClassMouse, kEventMouseDown },
    { kEventClassMouse, kEventMouseUp },
  };

  static EventHandlerUPP sMouseClickEventHandlerUPP;
  if (!sMouseClickEventHandlerUPP)
    sMouseClickEventHandlerUPP = ::NewEventHandlerUPP(MouseClickEventHandler);

  err =
   ::InstallApplicationEventHandler(sMouseClickEventHandlerUPP,
                                    GetEventTypeCount(kMouseClickEventList),
                                    kMouseClickEventList,
                                    NS_STATIC_CAST(void*, this),
                                    &mMouseClickEventHandler);
  NS_ASSERTION(err == noErr, "Could not install MouseClickEventHandler");

  //
  // create the TSM Message Pump
  //
  nsMacTSMMessagePump* tsmMessagePump = nsMacTSMMessagePump::GetSingleton();
  NS_ASSERTION(tsmMessagePump, "Unable to create TSM Message Pump");
}

nsMacMessagePump::~nsMacMessagePump()
{
  if (mMouseClickEventHandler)
    ::RemoveEventHandler(mMouseClickEventHandler);

  if (mWNETransitionEventHandler)
    ::RemoveEventHandler(mWNETransitionEventHandler);

  nsMacTSMMessagePump::Shutdown();
}

//=================================================================
/*  Dispatch a single event
 *  @param   anEvent - the event to dispatch
 *  @return  A boolean which states whether we handled the event
 */
PRBool nsMacMessagePump::DispatchEvent(EventRecord *anEvent)
{
  PRBool handled = PR_FALSE;

  switch(anEvent->what) {
    // diskEvt is gone in Carbon, and so is unhandled here.
    // keyUp, keyDown, and autoKey now have Carbon event handlers in
    //  nsMacWindow.

    case mouseDown:
      handled = DoMouseDown(*anEvent);
      break;

    case mouseUp:
      handled = DoMouseUp(*anEvent);
      break;

    case updateEvt:
      handled = DoUpdate(*anEvent);
      break;

    case activateEvt:
      handled = DoActivate(*anEvent);
      break;

    case osEvt: {
      unsigned char eventType = ((anEvent->message >> 24) & 0x00ff);
      switch (eventType)
      {
        case suspendResumeMessage:
          if (anEvent->message & resumeFlag)
            nsToolkit::AppInForeground();   // resume message
          else
            nsToolkit::AppInBackground();   // suspend message

          handled = DoMouseMove(*anEvent);
          break;

        case mouseMovedMessage:
          handled = DoMouseMove(*anEvent);
          break;
      }
      break;
    }
      
    case kHighLevelEvent:
      ::AEProcessAppleEvent(anEvent);
      handled = PR_TRUE;
      break;
  }

  return handled;
}

//-------------------------------------------------------------------------
//
// DoUpdate
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoUpdate(EventRecord &anEvent)
{

  WindowPtr whichWindow = reinterpret_cast<WindowPtr>(anEvent.message);
  
  StPortSetter portSetter(whichWindow);
  
  ::BeginUpdate(whichWindow);
  // The app can do its own updates here
  DispatchOSEventToRaptor(anEvent, whichWindow);
  ::EndUpdate(whichWindow);
  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// DoMouseDown
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoMouseDown(EventRecord &anEvent)
{
  WindowPtr     whichWindow;
  WindowPartCode        partCode;
  PRBool  handled = PR_FALSE;
  
  partCode = ::FindWindow(anEvent.where, &whichWindow);
  
  switch (partCode)
  {
      case inNoWindow:
        break;

      case inCollapseBox:   // we never seem to get this.
      case inSysWindow:
        if ( gRollupListener && gRollupWidget )
          gRollupListener->Rollup();
        break;

      case inMenuBar:
      {
        // If a xul popup is displayed, roll it up and don't allow the click
        // through to the menu code. This is how MacOS context menus work, so
        // I think this is a valid solution.
        if ( gRollupListener && gRollupWidget )
        {
          gRollupListener->Rollup();
        }
        else
        {
          ::MenuSelect(anEvent.where);
          handled = PR_TRUE;
        }
        
        break;
      }

      case inContent:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
        if ( IsWindowHilited(whichWindow) || (gRollupListener && gRollupWidget) )
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        else {
          nsCOMPtr<nsIWidget> topWidget;
          nsToolkit::GetTopWidget ( whichWindow, getter_AddRefs(topWidget) );
          nsCOMPtr<nsPIWidgetMac> macWindow ( do_QueryInterface(topWidget) );
          if ( macWindow ) {
            // a click occurred in a background window. Use WaitMouseMove() to determine if
            // it was a click or a drag. If it was a drag, send a drag gesture to the
            // background window. We don't need to rely on the ESM to track the gesture,
            // the OS has just told us.  If it was a click, bring it to the front like normal.
            Boolean initiateDragFromBGWindow = ::WaitMouseMoved(anEvent.where);
            if ( initiateDragFromBGWindow ) {
              nsCOMPtr<nsIEventSink> sink ( do_QueryInterface(topWidget) );
              if ( sink ) {
                // dispach a mousedown, an update event to paint any changes,
                // then the drag gesture event
                PRBool handled = PR_FALSE;
                sink->DispatchEvent ( &anEvent, &handled );
                
                EventRecord updateEvent = anEvent;
                updateEvent.what = updateEvt;
                updateEvent.message = NS_REINTERPRET_CAST(UInt32, whichWindow);
                sink->DispatchEvent ( &updateEvent, &handled );
                
                sink->DragEvent ( NS_DRAGDROP_GESTURE, anEvent.where.h, anEvent.where.v, 0L, &handled );                
              }
            }
            else {
              PRBool enabled;
              if (NS_SUCCEEDED(topWidget->IsEnabled(&enabled)) && !enabled)
                ::SysBeep(1);
              else
                macWindow->ComeToFront();
            }
            handled = PR_TRUE;
          }
        }
        break;
      }

      case inDrag:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);

        Point   oldTopLeft = {0, 0};
        ::LocalToGlobal(&oldTopLeft);
        
        // roll up popups BEFORE we start the drag
        if ( gRollupListener && gRollupWidget )
          gRollupListener->Rollup();

        Rect screenRect;
        ::GetRegionBounds(::GetGrayRgn(), &screenRect);
        ::DragWindow(whichWindow, anEvent.where, &screenRect);

        Point   newTopLeft = {0, 0};
        ::LocalToGlobal(&newTopLeft);

        // only activate if the command key is not down
        if (!(anEvent.modifiers & cmdKey))
        {
          nsCOMPtr<nsIWidget> topWidget;
          nsToolkit::GetTopWidget(whichWindow, getter_AddRefs(topWidget));
          
          nsCOMPtr<nsPIWidgetMac> macWindow ( do_QueryInterface(topWidget) );
          if ( macWindow )
            macWindow->ComeToFront();
        }
        
        // Dispatch the event because some windows may want to know that they have been moved.
        anEvent.where.h += newTopLeft.h - oldTopLeft.h;
        anEvent.where.v += newTopLeft.v - oldTopLeft.v;
        
        handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        break;
      }

      case inGrow:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);

        Rect sizeLimit;
        sizeLimit.top = kMinWindowHeight;
        sizeLimit.left = kMinWindowWidth;
        sizeLimit.bottom = 0x7FFF;
        sizeLimit.right = 0x7FFF;

        Rect newSize;
        ::ResizeWindow(whichWindow, anEvent.where, &sizeLimit, &newSize);

        Point newPt = botRight(newSize);
        ::LocalToGlobal(&newPt);
        newPt.h -= 8, newPt.v -= 8;
        anEvent.where = newPt;  // important!
        handled = DispatchOSEventToRaptor(anEvent, whichWindow);

        break;
      }

      case inGoAway:
      {
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
        if (::TrackGoAway(whichWindow, anEvent.where)) {
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        }
        break;
      }

      case inZoomIn:
      case inZoomOut:
        if (::TrackBox(whichWindow, anEvent.where, partCode))
        {
          if (partCode == inZoomOut)
          {
            nsCOMPtr<nsIWidget> topWidget;
            nsToolkit::GetTopWidget ( whichWindow, getter_AddRefs(topWidget) );
            nsCOMPtr<nsPIWidgetMac> macWindow ( do_QueryInterface(topWidget) );
            if ( macWindow )
              macWindow->CalculateAndSetZoomedSize();
          }
          // !!!  Do not call ZoomWindow before calling DispatchOSEventToRaptor
          //    otherwise nsMacEventHandler::HandleMouseDownEvent won't get
          //    the right partcode for the click location
          
          handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        }
        break;

      case inToolbarButton:           // Mac OS X only
        nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
        handled = DispatchOSEventToRaptor(anEvent, whichWindow);
        break;

  }

  return handled;
}

//-------------------------------------------------------------------------
//
// DoMouseUp
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoMouseUp(EventRecord &anEvent)
{
    WindowPtr     whichWindow;
    PRInt16       partCode;

  partCode = ::FindWindow(anEvent.where, &whichWindow);
  if (whichWindow == nil)
  {
    // We need to report the event even when it happens over no window:
    // when the user clicks a widget, keeps the mouse button pressed and
    // releases it outside the window, the event needs to be reported to
    // the widget so that it can deactivate itself.
    whichWindow = ::FrontWindow();
  }
  
  PRBool handled = DispatchOSEventToRaptor(anEvent, whichWindow);
  // consume mouse ups in the title bar, since nsMacWindow doesn't do that for us
  if (partCode == inDrag)
    handled = PR_TRUE;
  return handled;
}

//-------------------------------------------------------------------------
//
// DoMouseMove
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoMouseMove(EventRecord &anEvent)
{
  // same thing as DoMouseUp
  WindowPtr     whichWindow;
  PRInt16       partCode;
  PRBool        handled = PR_FALSE;
  
  partCode = ::FindWindow(anEvent.where, &whichWindow);
  if (whichWindow == nil)
    whichWindow = ::FrontWindow();

  /* Disable mouse moved events for windowshaded windows -- this prevents tooltips
     from popping up in empty space.
  */
  if (whichWindow == nil || !::IsWindowCollapsed(whichWindow))
    handled = DispatchOSEventToRaptor(anEvent, whichWindow);
  return handled;
}

//-------------------------------------------------------------------------
//
// DoActivate
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DoActivate(EventRecord &anEvent)
{
  WindowPtr whichWindow = (WindowPtr)anEvent.message;
  nsGraphicsUtils::SafeSetPortWindowPort(whichWindow);
  if (anEvent.modifiers & activeFlag)
  {
    ::HiliteWindow(whichWindow,TRUE);
  }
  else
  {
    PRBool ignoreDeactivate = PR_FALSE;
    nsCOMPtr<nsIWidget> windowWidget;
    nsToolkit::GetTopWidget ( whichWindow, getter_AddRefs(windowWidget));
    if (windowWidget)
    {
      nsCOMPtr<nsPIWidgetMac> window ( do_QueryInterface(windowWidget) );
      if (window)
      {
        window->GetIgnoreDeactivate(&ignoreDeactivate);
        window->SetIgnoreDeactivate(PR_FALSE);
      }
    }
    if (!ignoreDeactivate)
      ::HiliteWindow(whichWindow,FALSE);
  }

  return DispatchOSEventToRaptor(anEvent, whichWindow);
}

//-------------------------------------------------------------------------
//
// DispatchOSEventToRaptor
//
//-------------------------------------------------------------------------
PRBool  nsMacMessagePump::DispatchOSEventToRaptor(
                          EventRecord   &anEvent,
                          WindowPtr     aWindow)
{
  PRBool handled = PR_FALSE;
  nsCOMPtr<nsIEventSink> sink;
  nsToolkit::GetWindowEventSink ( aWindow, getter_AddRefs(sink) );
  if ( sink )
    sink->DispatchEvent ( &anEvent, &handled );
  return handled;
}

pascal OSStatus
nsMacMessagePump::MouseClickEventHandler(EventHandlerCallRef aHandlerCallRef,
                                         EventRef            aEvent,
                                         void*               aUserData)
{
  EventMouseButton button;
  OSErr err = ::GetEventParameter(aEvent, kEventParamMouseButton,
                                  typeMouseButton, NULL,
                                  sizeof(EventMouseButton), NULL, &button);

  // Only handle middle click events here.  Let the rest fall through.
  if (err != noErr || button != kEventMouseButtonTertiary)
    return eventNotHandledErr;

  EventRecord eventRecord;
  if (!::ConvertEventRefToEventRecord(aEvent, &eventRecord)) {
    // This will return FALSE on a middle click event; that's to let us know
    // it's giving us a nullEvent, which is expected since Classic events
    // don't support the middle button normally.
    //
    // We know better, so let's restore the actual event kind.
    UInt32 kind = ::GetEventKind(aEvent);
    eventRecord.what = (kind == kEventMouseDown) ? mouseDown : mouseUp;
  }

  // Classic mouse events don't record the button specifier. The message
  // parameter is unused in mouse click events, so let's stuff it there.
  // We'll pick it up in nsMacEventHandler::HandleMouseDownEvent().
  eventRecord.message = NS_STATIC_CAST(UInt32, button);

  // Process the modified event internally
  nsMacMessagePump* self = NS_STATIC_CAST(nsMacMessagePump*, aUserData);
  PRBool handled = self->DispatchEvent(&eventRecord);

  if (handled)
    return noErr;

  return eventNotHandledErr;
}

// WNETransitionEventHandler
//
// Transitional WaitNextEvent handler.  Accepts Carbon events from
// kWNETransitionEventList, converts them into EventRecords, and
// dispatches them through the path they would have gone if they
// had been received as EventRecords from WaitNextEvent.
//
// protected static
pascal OSStatus
nsMacMessagePump::WNETransitionEventHandler(EventHandlerCallRef aHandlerCallRef,
                                            EventRef            aEvent,
                                            void*               aUserData)
{
  nsMacMessagePump* self = NS_STATIC_CAST(nsMacMessagePump*, aUserData);

  EventRecord eventRecord;
  ::ConvertEventRefToEventRecord(aEvent, &eventRecord);

  PRBool handled = self->DispatchEvent(&eventRecord);

  if (!handled)
    return eventNotHandledErr;

  return noErr;
}
