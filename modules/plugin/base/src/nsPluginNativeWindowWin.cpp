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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "windows.h"
#include "windowsx.h"

#include "nsDebug.h"

#include "plevent.h"
#include "nsIEventQueueService.h"

#include "nsIPluginInstancePeer.h"
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
  nsPluginType_Other
} nsPluginType;

class nsPluginNativeWindowWin : public nsPluginNativeWindow {
public: 
  nsPluginNativeWindowWin();
  virtual ~nsPluginNativeWindowWin();

  virtual nsresult CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance);

private:
  nsresult SubclassAndAssociateWindow();
  nsresult UndoSubclassAndAssociateWindow();

public:
  // locals
  WNDPROC GetWindowProc();
  nsresult GetEventService(nsCOMPtr<nsIEventQueueService> &aEventService);
  PluginWindowEvent * GetPluginWindowEvent(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam);

private:
  WNDPROC mPluginWinProc;
  nsCOMPtr<nsIEventQueueService> mEventService;
  PluginWindowEvent mPluginWindowEvent;

public:
  nsPluginType mPluginType;
};

static PRBool ProcessFlashMessageDelayed(nsPluginNativeWindowWin * aWin, 
                                         HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  NS_ENSURE_TRUE(aWin, NS_ERROR_NULL_POINTER);

  if (msg != WM_USER+1)
    return PR_FALSE; // no need to delay

  // do stuff
  nsCOMPtr<nsIEventQueueService> eventService;
  if (NS_SUCCEEDED(aWin->GetEventService(eventService))) {
    nsCOMPtr<nsIEventQueue> eventQueue;  
    eventService->GetThreadEventQueue(PR_GetCurrentThread(), getter_AddRefs(eventQueue));
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
          else
            win->mPluginType = nsPluginType_Other;
        }
      }
    }
  }

  // Macromedia Flash plugin may flood the message queue with some special messages
  // (WM_USER+1) causing 100% CPU consumption and GUI freeze, see mozilla bug 132759;
  // we can prevent this from happening by delaying the processing such messages;
  if (win->mPluginType == nsPluginType_Flash) {
    if (ProcessFlashMessageDelayed(win, hWnd, msg, wParam, lParam))
      return TRUE;
  }

  LRESULT res = TRUE;

  nsCOMPtr<nsIPluginInstance> inst;
  win->GetPluginInstance(inst);

  NS_TRY_SAFE_CALL_RETURN(res, 
                          ::CallWindowProc((WNDPROC)win->GetWindowProc(), hWnd, msg, wParam, lParam),
                          nsnull, inst);
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

nsresult nsPluginNativeWindowWin::GetEventService(nsCOMPtr<nsIEventQueueService> &aEventService)
{
  if (!mEventService) {
    mEventService = do_GetService(kEventQueueServiceCID);
    if (!mEventService)
      return NS_ERROR_FAILURE;
  }
  aEventService = mEventService;
  return NS_OK;
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
};

nsresult nsPluginNativeWindowWin::CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance)
{
  // check the incoming instance, null indicates that window is going away and we are
  // not interested in subclassing business any more, undo and don't subclass
  if (!aPluginInstance)
    UndoSubclassAndAssociateWindow();

  nsPluginNativeWindow::CallSetWindow(aPluginInstance);

  if (aPluginInstance)
    SubclassAndAssociateWindow();

  return NS_OK;
}

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
