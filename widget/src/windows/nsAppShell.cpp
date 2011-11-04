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
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Darin Fisher <darin@meer.net>
 *   Jim Mathies <jmathies@mozilla.com>
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

#include "mozilla/ipc/RPCChannel.h"
#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsThreadUtils.h"
#include "WinTaskbar.h"
#include "nsString.h"
#include "nsIMM32Handler.h"
#include "mozilla/widget/AudioSession.h"

// For skidmark code
#include <windows.h> 
#include <tlhelp32.h> 

const PRUnichar* kAppShellEventId = L"nsAppShell:EventID";
const PRUnichar* kTaskbarButtonEventId = L"TaskbarButtonCreated";

// The maximum time we allow before forcing a native event callback
#define NATIVE_EVENT_STARVATION_LIMIT mozilla::TimeDuration::FromSeconds(1)

static UINT sMsgId;

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
static UINT sTaskbarButtonCreatedMsg;

/* static */
UINT nsAppShell::GetTaskbarButtonCreatedMessage() {
	return sTaskbarButtonCreatedMsg;
}
#endif

namespace mozilla {
namespace crashreporter {
void LSPAnnotate();
} // namespace crashreporter
} // namespace mozilla

using mozilla::crashreporter::LSPAnnotate;

//-------------------------------------------------------------------------

static bool PeekUIMessage(MSG* aMsg)
{
  MSG keyMsg, imeMsg, mouseMsg, *pMsg = 0;
  bool haveKeyMsg, haveIMEMsg, haveMouseMsg;

  haveKeyMsg = ::PeekMessageW(&keyMsg, NULL, WM_KEYFIRST, WM_IME_KEYLAST, PM_NOREMOVE);
  haveIMEMsg = ::PeekMessageW(&imeMsg, NULL, NS_WM_IMEFIRST, NS_WM_IMELAST, PM_NOREMOVE);
  haveMouseMsg = ::PeekMessageW(&mouseMsg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_NOREMOVE);

  if (haveKeyMsg) {
    pMsg = &keyMsg;
  }
  if (haveIMEMsg && (!pMsg || imeMsg.time < pMsg->time)) {
    pMsg = &imeMsg;
  }

  if (pMsg && !nsIMM32Handler::CanOptimizeKeyAndIMEMessages(pMsg)) {
    return false;
  }

  if (haveMouseMsg && (!pMsg || mouseMsg.time < pMsg->time)) {
    pMsg = &mouseMsg;
  }

  if (!pMsg) {
    return false;
  }

  return ::PeekMessageW(aMsg, NULL, pMsg->message, pMsg->message, PM_REMOVE);
}

/*static*/ LRESULT CALLBACK
nsAppShell::EventWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == sMsgId) {
    nsAppShell *as = reinterpret_cast<nsAppShell *>(lParam);
    as->NativeEventCallback();
    NS_RELEASE(as);
    return TRUE;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

nsAppShell::~nsAppShell()
{
  if (mEventWnd) {
    // DestroyWindow doesn't do anything when called from a non UI thread.
    // Since mEventWnd was created on the UI thread, it must be destroyed on
    // the UI thread.
    SendMessage(mEventWnd, WM_CLOSE, 0, 0);
  }
}

nsresult
nsAppShell::Init()
{
#ifdef MOZ_CRASHREPORTER
  LSPAnnotate();
#endif

  mLastNativeEventScheduled = TimeStamp::Now();

  if (!sMsgId)
    sMsgId = RegisterWindowMessageW(kAppShellEventId);

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  sTaskbarButtonCreatedMsg = ::RegisterWindowMessageW(kTaskbarButtonEventId);
  NS_ASSERTION(sTaskbarButtonCreatedMsg, "Could not register taskbar button creation message");
#endif

  WNDCLASSW wc;
  HINSTANCE module = GetModuleHandle(NULL);

  const PRUnichar *const kWindowClass = L"nsAppShell:EventWindowClass";
  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = EventWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH) NULL;
    wc.lpszMenuName  = (LPCWSTR) NULL;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }

  mEventWnd = CreateWindowW(kWindowClass, L"nsAppShell:EventWindow",
                           0, 0, 0, 10, 10, NULL, NULL, module, NULL);
  NS_ENSURE_STATE(mEventWnd);

  return nsBaseAppShell::Init();
}

/**
 * This is some temporary code to keep track of where in memory dlls are
 * loaded. This is useful in case someone calls into a dll that has been
 * unloaded. This code lets us see which dll used to be loaded at the given
 * called address.
 */
#if defined(_MSC_VER) && defined(_M_IX86)

#define LOADEDMODULEINFO_STRSIZE 23
#define NUM_LOADEDMODULEINFO 250

struct LoadedModuleInfo {
  void* mStartAddr;
  void* mEndAddr;
  char mName[LOADEDMODULEINFO_STRSIZE + 1];
};

static LoadedModuleInfo* sLoadedModules = 0;

static void
CollectNewLoadedModules()
{
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
  MODULEENTRY32W module;

  // Take a snapshot of all modules in our process.
  hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  if (hModuleSnap == INVALID_HANDLE_VALUE)
    return;

  // Set the size of the structure before using it.
  module.dwSize = sizeof(MODULEENTRY32W);

  // Now walk the module list of the process,
  // and display information about each module
  bool done = !Module32FirstW(hModuleSnap, &module);
  while (!done) {
    NS_LossyConvertUTF16toASCII moduleName(module.szModule);
    bool found = false;
    PRUint32 i;
    for (i = 0; i < NUM_LOADEDMODULEINFO &&
                sLoadedModules[i].mStartAddr; ++i) {
      if (sLoadedModules[i].mStartAddr == module.modBaseAddr &&
          !strcmp(moduleName.get(),
                  sLoadedModules[i].mName)) {
        found = true;
        break;
      }
    }

    if (!found && i < NUM_LOADEDMODULEINFO) {
      sLoadedModules[i].mStartAddr = module.modBaseAddr;
      sLoadedModules[i].mEndAddr = module.modBaseAddr + module.modBaseSize;
      strncpy(sLoadedModules[i].mName, moduleName.get(),
              LOADEDMODULEINFO_STRSIZE);
      sLoadedModules[i].mName[LOADEDMODULEINFO_STRSIZE] = 0;
    }

    done = !Module32NextW(hModuleSnap, &module);
  }

  PRUint32 i;
  for (i = 0; i < NUM_LOADEDMODULEINFO &&
              sLoadedModules[i].mStartAddr; ++i) {}

  CloseHandle(hModuleSnap);
}

NS_IMETHODIMP
nsAppShell::Run(void)
{
  LoadedModuleInfo modules[NUM_LOADEDMODULEINFO];
  memset(modules, 0, sizeof(modules));
  sLoadedModules = modules;	

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  // Ignore failure; failing to start the application is not exactly an
  // appropriate response to failing to start an audio session.
  mozilla::widget::StartAudioSession();
#endif

  nsresult rv = nsBaseAppShell::Run();

#ifdef MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  mozilla::widget::StopAudioSession();
#endif

  // Don't forget to null this out!
  sLoadedModules = nsnull;

  return rv;
}

#endif

void
nsAppShell::DoProcessMoreGeckoEvents()
{
  // Called by nsBaseAppShell's NativeEventCallback() after it has finished
  // processing pending gecko events and there are still gecko events pending
  // for the thread. (This can happen if NS_ProcessPendingEvents reached it's
  // starvation timeout limit.) The default behavior in nsBaseAppShell is to
  // call ScheduleNativeEventCallback to post a follow up native event callback
  // message. This triggers an additional call to NativeEventCallback for more
  // gecko event processing.

  // There's a deadlock risk here with certain internal Windows modal loops. In
  // our dispatch code, we prioritize messages so that input is handled first.
  // However Windows modal dispatch loops often prioritize posted messages. If
  // we find ourselves in a tight gecko timer loop where NS_ProcessPendingEvents
  // takes longer than the timer duration, NS_HasPendingEvents(thread) will
  // always be true. ScheduleNativeEventCallback will be called on every
  // NativeEventCallback callback, and in a Windows modal dispatch loop, the
  // callback message will be processed first -> input gets starved, dead lock.
  
  // To avoid, don't post native callback messages from NativeEventCallback
  // when we're in a modal loop. This gets us back into the Windows modal
  // dispatch loop dispatching input messages. Once we drop out of the modal
  // loop, we use mNativeCallbackPending to fire off a final NativeEventCallback
  // if we need it, which insures NS_ProcessPendingEvents gets called and all
  // gecko events get processed.
  if (mEventloopNestingLevel < 2) {
    OnDispatchedEvent(nsnull);
    mNativeCallbackPending = false;
  } else {
    mNativeCallbackPending = true;
  }
}

void
nsAppShell::ScheduleNativeEventCallback()
{
  // Post a message to the hidden message window
  NS_ADDREF_THIS(); // will be released when the event is processed
  // Time stamp this event so we can detect cases where the event gets
  // dropping in sub classes / modal loops we do not control. 
  mLastNativeEventScheduled = TimeStamp::Now();
  ::PostMessage(mEventWnd, sMsgId, 0, reinterpret_cast<LPARAM>(this));
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
#if defined(_MSC_VER) && defined(_M_IX86)
  if (sXPCOMHasLoadedNewDLLs && sLoadedModules) {
    sXPCOMHasLoadedNewDLLs = false;
    CollectNewLoadedModules();
  }
#endif

  // Notify ipc we are spinning a (possibly nested) gecko event loop.
  mozilla::ipc::RPCChannel::NotifyGeckoEventDispatch();

  bool gotMessage = false;

  do {
    MSG msg;
    // Give priority to keyboard and mouse messages.
    if (PeekUIMessage(&msg) ||
        ::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
      gotMessage = true;
      if (msg.message == WM_QUIT) {
        ::PostQuitMessage(msg.wParam);
        Exit();
      } else {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
      }
    } else if (mayWait) {
      // Block and wait for any posted application message
      ::WaitMessage();
    }
  } while (!gotMessage && mayWait);

  // See DoProcessNextNativeEvent, mEventloopNestingLevel will be
  // one when a modal loop unwinds.
  if (mNativeCallbackPending && mEventloopNestingLevel == 1)
    DoProcessMoreGeckoEvents();

  // Check for starved native callbacks. If we haven't processed one
  // of these events in NATIVE_EVENT_STARVATION_LIMIT, fire one off.
  if ((TimeStamp::Now() - mLastNativeEventScheduled) >
      NATIVE_EVENT_STARVATION_LIMIT) {
    ScheduleNativeEventCallback();
  }
  
  return gotMessage;
}
