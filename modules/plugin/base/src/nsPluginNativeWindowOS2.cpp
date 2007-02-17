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
#define INCL_WIN
#include "os2.h"

#include "nsDebug.h"

#include "nsIPluginInstancePeer.h"
#include "nsPluginSafety.h"
#include "nsPluginNativeWindow.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsTWeakRef.h"

static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID); // needed for NS_TRY_SAFE_CALL

#define NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION "MozillaPluginWindowPropertyAssociation"

typedef nsTWeakRef<class nsPluginNativeWindowOS2> PluginWindowWeakRef;

extern "C" {
PVOID APIENTRY WinQueryProperty(HWND hwnd, PCSZ  pszNameOrAtom);

PVOID APIENTRY WinRemoveProperty(HWND hwnd, PCSZ  pszNameOrAtom);

BOOL  APIENTRY WinSetProperty(HWND hwnd, PCSZ  pszNameOrAtom,
                              PVOID pvData, ULONG ulFlags);
}

/**
 *  PLEvent handling code
 */
class PluginWindowEvent : public nsRunnable {
public:
  PluginWindowEvent();
  void Init(const PluginWindowWeakRef &ref, HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
  void Clear();
  HWND   GetWnd()    { return mWnd; };
  ULONG   GetMsg()    { return mMsg; };
  MPARAM GetWParam() { return mWParam; };
  MPARAM GetLParam() { return mLParam; };
  PRBool InUse()     { return (mWnd!=NULL || mMsg!=0); };
  
  NS_DECL_NSIRUNNABLE

protected:
  PluginWindowWeakRef mPluginWindowRef;
  HWND   mWnd;
  ULONG  mMsg;
  MPARAM mWParam;
  MPARAM mLParam;
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

void PluginWindowEvent::Init(const PluginWindowWeakRef &ref, HWND aWnd,
                             ULONG aMsg, MPARAM mp1, MPARAM mp2)
{
  NS_ASSERTION(aWnd!=NULL && aMsg!=0, "invalid plugin event value");
  NS_ASSERTION(mWnd==NULL && mMsg==0 && mWParam==0 && mLParam==0,"event already in use");
  mPluginWindowRef = ref;
  mWnd    = aWnd;
  mMsg    = aMsg;
  mWParam = mp1;
  mLParam = mp2;
}

/**
 *  nsPluginNativeWindow Windows specific class declaration
 */

typedef enum {
  nsPluginType_Unknown = 0,
  nsPluginType_Flash,
  nsPluginType_Java_vm,
  nsPluginType_Other
} nsPluginType;

class nsPluginNativeWindowOS2 : public nsPluginNativeWindow {
public: 
  nsPluginNativeWindowOS2();
  virtual ~nsPluginNativeWindowOS2();

  virtual nsresult CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance);

private:
  nsresult SubclassAndAssociateWindow();
  nsresult UndoSubclassAndAssociateWindow();

public:
  // locals
  PFNWP GetWindowProc();
  PluginWindowEvent * GetPluginWindowEvent(HWND aWnd, ULONG aMsg, MPARAM mp1, MPARAM mp2);

private:
  PFNWP mPluginWinProc;
  PluginWindowWeakRef mWeakRef;
  nsRefPtr<PluginWindowEvent> mCachedPluginWindowEvent;

public:
  nsPluginType mPluginType;
};

static PRBool ProcessFlashMessageDelayed(nsPluginNativeWindowOS2 * aWin, 
                                         HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  NS_ENSURE_TRUE(aWin, NS_ERROR_NULL_POINTER);

  if (msg != WM_USER+1)
    return PR_FALSE; // no need to delay

  // do stuff
  nsCOMPtr<nsIRunnable> pwe = aWin->GetPluginWindowEvent(hWnd, msg, mp1, mp2);
  if (pwe) {
    NS_DispatchToCurrentThread(pwe);
    return PR_TRUE;  
  }
  return PR_FALSE;
}

/**
 *   New plugin window procedure
 */
MRESULT EXPENTRY PluginWndProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  nsPluginNativeWindowOS2 * win = (nsPluginNativeWindowOS2 *)::WinQueryProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  if (!win)
    return (MRESULT)TRUE;

  // check plugin mime type and cache whether it is Flash or java-vm or not
  // flash and java-vm will need special treatment later
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
          else if (!strcmp(mimetype, "application/x-java-vm"))
            win->mPluginType = nsPluginType_Java_vm;
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
    if (ProcessFlashMessageDelayed(win, hWnd, msg, mp1, mp2))
      return (MRESULT)TRUE;
  }

  MRESULT res = (MRESULT)TRUE;

  nsCOMPtr<nsIPluginInstance> inst;
  win->GetPluginInstance(inst);

  if (win->mPluginType == nsPluginType_Java_vm) {
    NS_TRY_SAFE_CALL_RETURN(res, WinDefWindowProc(hWnd, msg, mp1, mp2), nsnull, inst);
  } else {
    NS_TRY_SAFE_CALL_RETURN(res, (win->GetWindowProc())(hWnd, msg, mp1, mp2), nsnull, inst);
  }

  return res;
}

/**
 *   nsPluginNativeWindowOS2 implementation
 */
nsPluginNativeWindowOS2::nsPluginNativeWindowOS2() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nsnull; 
  x = 0; 
  y = 0; 
  width = 0; 
  height = 0; 

  mPluginWinProc = NULL;
  mPluginType = nsPluginType_Unknown;
}

nsPluginNativeWindowOS2::~nsPluginNativeWindowOS2()
{
  // clear weak reference to self to prevent any pending events from
  // dereferencing this.
  mWeakRef.forget();
}

PFNWP nsPluginNativeWindowOS2::GetWindowProc()
{
  return mPluginWinProc;
}

NS_IMETHODIMP PluginWindowEvent::Run()
{
  nsPluginNativeWindowOS2 *win = mPluginWindowRef.get();
  if (!win)
    return NS_OK;

  HWND hWnd = GetWnd();
  if (!hWnd)
    return NS_OK;

  nsCOMPtr<nsIPluginInstance> inst;
  win->GetPluginInstance(inst);
  NS_TRY_SAFE_CALL_VOID((win->GetWindowProc()) 
                       (hWnd, 
                        GetMsg(), 
                        GetWParam(), 
                        GetLParam()),
                        nsnull, inst);
  Clear();
  return NS_OK;
}

PluginWindowEvent*
nsPluginNativeWindowOS2::GetPluginWindowEvent(HWND aWnd, ULONG aMsg, MPARAM aMp1, MPARAM aMp2)
{
  if (!mWeakRef) {
    mWeakRef = this;
    if (!mWeakRef)
      return nsnull;
  }

  PluginWindowEvent *event;
  if (!mCachedPluginWindowEvent || mCachedPluginWindowEvent->InUse()) {
    // We have the ability to alloc if needed in case in the future some plugin
    // should post multiple PostMessages. However, this could lead to many
    // alloc's per second which could become a performance issue. If/when this
    // is asserting then this needs to be studied. See bug 169247
    NS_ASSERTION(1, "possible plugin performance issue");
    event = new PluginWindowEvent();
    if (!event)
      return nsnull;
  }
  else {
    event = mCachedPluginWindowEvent;
  }
  NS_ADDREF(event);

  event->Init(mWeakRef, aWnd, aMsg, aMp1, aMp2);
  return event;
};

nsresult nsPluginNativeWindowOS2::CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance)
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

nsresult nsPluginNativeWindowOS2::SubclassAndAssociateWindow()
{
  if (type != nsPluginWindowType_Window)
    return NS_ERROR_FAILURE;

  HWND hWnd = (HWND)window;
  if (!hWnd)
    return NS_ERROR_FAILURE;

  // check if we need to re-subclass
  PFNWP currentWndProc = (PFNWP)::WinQueryWindowPtr(hWnd, QWP_PFNWP);
  if (PluginWndProc == currentWndProc)
    return NS_OK;

  mPluginWinProc = WinSubclassWindow(hWnd, PluginWndProc);
  if (!mPluginWinProc)
    return NS_ERROR_FAILURE;

  nsPluginNativeWindowOS2 * win = (nsPluginNativeWindowOS2 *)::WinQueryProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  NS_ASSERTION(!win || (win == this), "plugin window already has property and this is not us");
  
  if (!::WinSetProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION, (PVOID)this, 0))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult nsPluginNativeWindowOS2::UndoSubclassAndAssociateWindow()
{
  // release plugin instance
  SetPluginInstance(nsnull);

  // remove window property
  HWND hWnd = (HWND)window;
  if (WinIsWindow(/*HAB*/0, hWnd))
    ::WinRemoveProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);

  // restore the original win proc
  // but only do this if this were us last time
  if (mPluginWinProc) {
    PFNWP currentWndProc = (PFNWP)::WinQueryWindowPtr(hWnd, QWP_PFNWP);
    if (currentWndProc == PluginWndProc)
      WinSubclassWindow(hWnd, mPluginWinProc);
  }

  return NS_OK;
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);

  *aPluginNativeWindow = new nsPluginNativeWindowOS2();

  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowOS2 *p = (nsPluginNativeWindowOS2 *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}
