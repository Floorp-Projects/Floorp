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
 *   Andrei Volkov <av@netscape.com>
 *   Brian Stell <bstell@netscape.com>
 *   Peter Lubczynski <peterl@netscape.com>
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

#include "windows.h"
#include "windowsx.h"

// XXXbz windowsx.h defines GetFirstChild, GetNextSibling,
// GetPrevSibling are macros, apparently... Eeevil.  We have functions
// called that on some classes, so undef them.
#undef GetFirstChild
#undef GetNextSibling
#undef GetPrevSibling

#include "nsDebug.h"

#include "plevent.h"
#include "nsIEventQueueService.h"
#include "nsGUIEvent.h"

#include "nsIPluginInstancePeer.h"
#include "nsIPluginInstanceInternal.h"
#include "nsPluginSafety.h"
#include "nsPluginNativeWindow.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID); // needed for NS_TRY_SAFE_CALL

#define NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION "MozillaPluginWindowPropertyAssociation"

/**
 *  PLEvent handling code
 */
class PluginWindowEvent : public PLEvent {
public:
  PluginWindowEvent();
  void Init(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void Clear();
  HWND   GetWnd()    { return mWnd; };
  UINT   GetMsg()    { return mMsg; };
  WPARAM GetWParam() { return mWParam; };
  LPARAM GetLParam() { return mLParam; };
  PRBool GetIsAlloced() { return mIsAlloced; };
  void   SetIsAlloced(PRBool aIsAlloced) { mIsAlloced = aIsAlloced; };
  PRBool InUse() { return (mWnd!=NULL || mMsg!=0); };

protected:
  HWND   mWnd;
  UINT   mMsg;
  WPARAM mWParam;
  LPARAM mLParam;
  PRBool mIsAlloced;
};

PluginWindowEvent::PluginWindowEvent()
{
  Clear();
}

void PluginWindowEvent::Clear()
{
  mWnd    = NULL;
  mMsg    = 0;
  mWParam = 0;
  mLParam = 0;
}

void PluginWindowEvent::Init(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
  NS_ASSERTION(aWnd!=NULL && aMsg!=0, "invalid plugin event value");
  NS_ASSERTION(mWnd==NULL && mMsg==0 && mWParam==0 && mLParam==0,"event already in use");
  mWnd    = aWnd;
  mMsg    = aMsg;
  mWParam = aWParam;
  mLParam = aLParam;
}

/**
 *  nsPluginNativeWindow Windows specific class declaration
 */

typedef enum {
  nsPluginType_Unknown = 0,
  nsPluginType_Flash,
  nsPluginType_Real,
  nsPluginType_Other
} nsPluginType;

class nsPluginNativeWindowWin : public nsPluginNativeWindow {
public: 
  nsPluginNativeWindowWin();
  virtual ~nsPluginNativeWindowWin();

  virtual nsresult CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance);

private:
#ifndef WINCE
  nsresult SubclassAndAssociateWindow();
  nsresult UndoSubclassAndAssociateWindow();
#endif

public:
  // locals
  WNDPROC GetWindowProc();
  nsIEventQueueService *GetEventService();
  PluginWindowEvent * GetPluginWindowEvent(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam);

private:
  WNDPROC mPluginWinProc;
  nsCOMPtr<nsIEventQueueService> mEventService;
  PluginWindowEvent mPluginWindowEvent;

public:
  nsPluginType mPluginType;
};

static PRBool sInMessageDispatch = PR_FALSE;
static UINT sLastMsg = 0;

static PRBool ProcessFlashMessageDelayed(nsPluginNativeWindowWin * aWin, 
                                         HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  NS_ENSURE_TRUE(aWin, NS_ERROR_NULL_POINTER);

  if (msg != WM_USER+1)
    return PR_FALSE; // no need to delay

  // do stuff
  nsIEventQueueService *eventService = aWin->GetEventService();
  if (eventService) {
    nsCOMPtr<nsIEventQueue> eventQueue;  
    eventService->GetThreadEventQueue(PR_GetCurrentThread(),
                                      getter_AddRefs(eventQueue));
    if (eventQueue) {
      PluginWindowEvent *pwe = aWin->GetPluginWindowEvent(hWnd, msg, wParam, lParam);
      if (pwe) {
        eventQueue->PostEvent(pwe);
        return PR_TRUE;  
      }
    }
  }
  return PR_FALSE;
}

PR_STATIC_CALLBACK(void*)
DelayedPopupsEnabledEvent_Handle(PLEvent *event)
{
  nsIPluginInstanceInternal *instInternal =
    (nsIPluginInstanceInternal *)event->owner;

  instInternal->PushPopupsEnabledState(PR_FALSE);

  return nsnull;
}

PR_STATIC_CALLBACK(void)
DelayedPopupsEnabledEvent_Destroy(PLEvent *event)
{
  nsIPluginInstanceInternal *instInternal =
    (nsIPluginInstanceInternal *)event->owner;

  NS_RELEASE(instInternal);

  delete event;
}

/**
 *   New plugin window procedure
 */
static LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  nsPluginNativeWindowWin * win = (nsPluginNativeWindowWin *)::GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  if (!win)
    return TRUE;

  // check plugin myme type and cache whether it is Flash or not
  // Flash will need special treatment later
  if (win->mPluginType == nsPluginType_Unknown) {
    nsCOMPtr<nsIPluginInstance> inst;
    win->GetPluginInstance(inst);
    if (inst) {
      nsCOMPtr<nsIPluginInstancePeer> pip;
      inst->GetPeer(getter_AddRefs(pip));
      if (pip) {
        nsMIMEType mimetype = nsnull;
        pip->GetMIMEType(&mimetype);
        if (mimetype) { 
          if (!strcmp(mimetype, "application/x-shockwave-flash"))
            win->mPluginType = nsPluginType_Flash;
          else if (!strcmp(mimetype, "audio/x-pn-realaudio-plugin"))
            win->mPluginType = nsPluginType_Real;
          else
            win->mPluginType = nsPluginType_Other;
        }
      }
    }
  }

  // Real may go into a state where it recursivly dispatches the same event
  // when subclassed. If this is Real, lets examine the event and drop it
  // on the floor if we get into this recursive situation. See bug 192914.
  if (win->mPluginType == nsPluginType_Real) {
    
    if (sInMessageDispatch && (msg == sLastMsg)) {
#ifdef DEBUG
      printf("Dropping event %d for Real on the floor\n", msg);
#endif
      return PR_TRUE;  // prevents event dispatch
    } else {
      sLastMsg = msg;  // no need to prevent dispatch
    }
  }

  PRBool enablePopups = PR_FALSE;

  // Activate/deactivate mouse capture on the plugin widget
  // here, before we pass the Windows event to the plugin
  // because its possible our widget won't get paired events
  // (see bug 131007) and we'll look frozen. Note that this
  // is also done in ChildWindow::DispatchMouseEvent.
  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget)
        widget->CaptureMouse(PR_TRUE);
      break;
    }
    case WM_LBUTTONUP:
      enablePopups = PR_TRUE;

      // fall through
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget)
        widget->CaptureMouse(PR_FALSE);
      break;
    }
    case WM_KEYDOWN:
      // Ignore repeating keydown messages...
      if ((lParam & 0x40000000) != 0) {
        break;
      }

      // fall through
    case WM_KEYUP:
      enablePopups = PR_TRUE;

      break;

#ifndef WINCE
    case WM_MOUSEACTIVATE: {
      // This seems to be the only way we're
      // notified when a child window that doesn't have this handler proc
      // (read as: windows created by plugins like Adobe Acrobat)
      // has been activated via clicking.
      // should be handled here because some plugins won't forward
      // messages to original WinProc.
      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget) {
        nsFocusEvent event(PR_TRUE, NS_PLUGIN_ACTIVATE, widget);
        nsEventStatus status;
        widget->DispatchEvent(&event, status);
      }
    }
    break;
#endif
  }

  // Macromedia Flash plugin may flood the message queue with some special messages
  // (WM_USER+1) causing 100% CPU consumption and GUI freeze, see mozilla bug 132759;
  // we can prevent this from happening by delaying the processing such messages;
  if (win->mPluginType == nsPluginType_Flash) {
    if (ProcessFlashMessageDelayed(win, hWnd, msg, wParam, lParam))
      return TRUE;
  }

  LRESULT res = TRUE;

  nsCOMPtr<nsIPluginInstanceInternal> instInternal;
  nsCOMPtr<nsIPluginInstance> inst;
  win->GetPluginInstance(inst);

  if (enablePopups) {
    nsCOMPtr<nsIPluginInstanceInternal> tmp = do_QueryInterface(inst);

    if (tmp && !nsVersionOK(tmp->GetPluginAPIVersion(),
                            NP_POPUP_API_VERSION)) {
      tmp.swap(instInternal);

      instInternal->PushPopupsEnabledState(PR_TRUE);
    }
  }

  sInMessageDispatch = PR_TRUE;

  NS_TRY_SAFE_CALL_RETURN(res, 
                          ::CallWindowProc((WNDPROC)win->GetWindowProc(), hWnd, msg, wParam, lParam),
                          nsnull, inst);

  sInMessageDispatch = PR_FALSE;

  if (instInternal) {
    // Popups are enabled (were enabled before the call to
    // CallWindowProc()). Some plugins (at least the flash player)
    // post messages from their key handlers etc that delay the actual
    // processing, so we need to delay the disabling of popups so that
    // popups remain enabled when the flash player ends up processing
    // the actual key handlers. We do this by posting an event that
    // does the disabling, this way our disabling will happen after
    // the handlers in the plugin are done.

    // Note that it's not fatal if any of this fails (which won't
    // happen unless we're out of memory anyways) since the plugin
    // code will pop any popup state pushed by this plugin on
    // destruction.

    nsIEventQueueService *eventService = win->GetEventService();
    if (eventService) {
      nsCOMPtr<nsIEventQueue> eventQueue;  
      eventService->GetThreadEventQueue(PR_GetCurrentThread(),
                                        getter_AddRefs(eventQueue));
      if (eventQueue) {
        PLEvent *event = new PLEvent;

        if (event) {
          nsIPluginInstanceInternal *eventInst = instInternal;

          // Make the event own the plugin instance.
          NS_ADDREF(eventInst);

          PL_InitEvent(event, eventInst, DelayedPopupsEnabledEvent_Handle,
                       DelayedPopupsEnabledEvent_Destroy);

          eventQueue->PostEvent(event);
        }
      }
    }
  }

  return res;
}

/**
 *   nsPluginNativeWindowWin implementation
 */
nsPluginNativeWindowWin::nsPluginNativeWindowWin() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nsnull; 
  x = 0; 
  y = 0; 
  width = 0; 
  height = 0; 

  mPluginWinProc = NULL;
  mPluginWindowEvent.SetIsAlloced(PR_FALSE);
  mPluginType = nsPluginType_Unknown;
}

nsPluginNativeWindowWin::~nsPluginNativeWindowWin()
{
  // clear any pending events to avoid dangling pointers
  nsCOMPtr<nsIEventQueueService> eventService(do_GetService(kEventQueueServiceCID));
  if (eventService) {
    nsCOMPtr<nsIEventQueue> eventQueue;  
    eventService->GetThreadEventQueue(PR_GetCurrentThread(), getter_AddRefs(eventQueue));
    if (eventQueue) {
      eventQueue->RevokeEvents(this);
    }
  }
}

WNDPROC nsPluginNativeWindowWin::GetWindowProc()
{
  return mPluginWinProc;
}

PR_STATIC_CALLBACK(void*)
PluginWindowEvent_Handle(PLEvent* self)
{
  if (!self)
    return nsnull;

  PluginWindowEvent *event = NS_STATIC_CAST(PluginWindowEvent*, self);
  
  HWND hWnd = event->GetWnd();
  if (!hWnd)
    return nsnull;

  nsPluginNativeWindowWin * win = (nsPluginNativeWindowWin *)::GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  if (win) {
    nsCOMPtr<nsIPluginInstance> inst;
    win->GetPluginInstance(inst);
    NS_TRY_SAFE_CALL_VOID(::CallWindowProc(win->GetWindowProc(), 
                          hWnd, 
                          event->GetMsg(), 
                          event->GetWParam(), 
                          event->GetLParam()),
                          nsnull, inst);
  }

  return nsnull;
}

PR_STATIC_CALLBACK(void)
PluginWindowEvent_Destroy(PLEvent* self)
{
  if (!self)
    return;

  PluginWindowEvent *event = NS_STATIC_CAST(PluginWindowEvent*, self);
  if (event->GetIsAlloced()) {
    delete event;
  }
  else
    event->Clear();
}

nsIEventQueueService *nsPluginNativeWindowWin::GetEventService()
{
  if (!mEventService) {
    mEventService = do_GetService(kEventQueueServiceCID);
  }

  return mEventService;
}

PluginWindowEvent*
nsPluginNativeWindowWin::GetPluginWindowEvent(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
  PluginWindowEvent *event;
  if (mPluginWindowEvent.InUse()) {
    // We have the ability to alloc if needed in case in the future some plugin
    // should post multiple PostMessages. However, this could lead to many
    // alloc's per second which could become a performance issue. If/when this
    // is asserting then this needs to be studied. See bug 169247
    NS_ASSERTION(1, "possible plugin performance issue");
    event = new PluginWindowEvent();
    if (!event)
      return nsnull;

    event->SetIsAlloced(PR_TRUE);
  }
  else {
    event = &mPluginWindowEvent;
  }

  event->Init(aWnd, aMsg, aWParam, aLParam);
  PL_InitEvent(event, (void *)this, &PluginWindowEvent_Handle, PluginWindowEvent_Destroy);
  return event;
}

nsresult nsPluginNativeWindowWin::CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance)
{
  // check the incoming instance, null indicates that window is going away and we are
  // not interested in subclassing business any more, undo and don't subclass

  // WINCE does not subclass windows.  See bug 300011 for the details.
#ifndef WINCE
  if (!aPluginInstance)
    UndoSubclassAndAssociateWindow();
#endif

  nsPluginNativeWindow::CallSetWindow(aPluginInstance);

#ifndef WINCE
  if (aPluginInstance)
    SubclassAndAssociateWindow();
#endif

  return NS_OK;
}

#ifndef WINCE

nsresult nsPluginNativeWindowWin::SubclassAndAssociateWindow()
{
  if (type != nsPluginWindowType_Window)
    return NS_ERROR_FAILURE;

  HWND hWnd = (HWND)window;
  if (!hWnd)
    return NS_ERROR_FAILURE;

  // check if we need to re-subclass
  WNDPROC currentWndProc = (WNDPROC)::GetWindowLong(hWnd, GWL_WNDPROC);
  if (PluginWndProc == currentWndProc)
    return NS_OK;

  mPluginWinProc = SubclassWindow(hWnd, (LONG)PluginWndProc);
  if (!mPluginWinProc)
    return NS_ERROR_FAILURE;

  nsPluginNativeWindowWin * win = (nsPluginNativeWindowWin *)::GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  NS_ASSERTION(!win || (win == this), "plugin window already has property and this is not us");
  
  if (!::SetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION, (HANDLE)this))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult nsPluginNativeWindowWin::UndoSubclassAndAssociateWindow()
{
  // release plugin instance
  SetPluginInstance(nsnull);

  // remove window property
  HWND hWnd = (HWND)window;
  if (IsWindow(hWnd))
    ::RemoveProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);

  // restore the original win proc
  // but only do this if this were us last time
  if (mPluginWinProc) {
    WNDPROC currentWndProc = (WNDPROC)::GetWindowLong(hWnd, GWL_WNDPROC);
    if (currentWndProc == PluginWndProc)
      SubclassWindow(hWnd, (LONG)mPluginWinProc);
  }

  return NS_OK;
}
#endif // WINCE

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);

  *aPluginNativeWindow = new nsPluginNativeWindowWin();

  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowWin *p = (nsPluginNativeWindowWin *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}
