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
 *   Masayuki Nakano <masayuki@d-toybox.com>
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
#include "nsAppShell.h"
#include "nsWindow.h"
#include "nsWidgetsCID.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsWidgetAtoms.h"
#include <objbase.h>
#include <initguid.h>

#ifndef WINCE
#include "nsUXThemeData.h"
#endif

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

NS_IMPL_ISUPPORTS1(nsToolkit, nsIToolkit)

//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;


HINSTANCE nsToolkit::mDllInstance = 0;
PRBool    nsToolkit::mIsWinXP     = PR_FALSE;
static PRBool dummy = nsToolkit::InitVersionInfo();

#if !defined(MOZ_STATIC_COMPONENT_LIBS) && !defined(MOZ_ENABLE_LIBXUL)
//
// Dll entry point. Keep the dll instance
//

#if defined(__GNUC__)
// If DllMain gets name mangled, it won't be seen.
extern "C" {
#endif

// Windows CE is created when nsToolkit
// starts up, not when the dll is loaded.
#ifndef WINCE
BOOL APIENTRY DllMain(  HINSTANCE hModule, 
                        DWORD reason, 
                        LPVOID lpReserved )
{
    switch( reason ) {
        case DLL_PROCESS_ATTACH:
            nsToolkit::Startup(hModule);
            break;

        case DLL_THREAD_ATTACH:
            break;
    
        case DLL_THREAD_DETACH:
            break;
    
        case DLL_PROCESS_DETACH:
            nsToolkit::Shutdown();
            break;

    }

    return TRUE;
}
#endif //#ifndef WINCE

#if defined(__GNUC__)
} // extern "C"
#endif

#endif

//
// main for the message pump thread
//
PRBool gThreadState = PR_FALSE;

struct ThreadInitInfo {
    PRMonitor *monitor;
    nsToolkit *toolkit;
};

MouseTrailer*       nsToolkit::gMouseTrailer;

void RunPump(void* arg)
{
    ThreadInitInfo *info = (ThreadInitInfo*)arg;
    ::PR_EnterMonitor(info->monitor);

    // do registration and creation in this thread
    info->toolkit->CreateInternalWindow(PR_GetCurrentThread());

    gThreadState = PR_TRUE;

    ::PR_Notify(info->monitor);
    ::PR_ExitMonitor(info->monitor);

    delete info;

    // Process messages
    MSG msg;
    while (::GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
    mGuiThread  = NULL;
    mDispatchWnd = 0;

#if defined(MOZ_STATIC_COMPONENT_LIBS) || defined (WINCE)
    nsToolkit::Startup(GetModuleHandle(NULL));
#endif

    gMouseTrailer = new MouseTrailer();
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    NS_PRECONDITION(::IsWindow(mDispatchWnd), "Invalid window handle");

    // Destroy the Dispatch Window
    ::DestroyWindow(mDispatchWnd);
    mDispatchWnd = NULL;

    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);

    if (gMouseTrailer) {
      gMouseTrailer->DestroyTimer();
      delete gMouseTrailer;
      gMouseTrailer = nsnull;
    }

#if defined (MOZ_STATIC_COMPONENT_LIBS) || defined(WINCE)
    nsToolkit::Shutdown();
#endif
}

void
nsToolkit::Startup(HMODULE hModule)
{
    nsToolkit::mDllInstance = hModule;

    //
    // register the internal window class
    //
    WNDCLASSW wc;
    wc.style            = CS_GLOBALCLASS;
    wc.lpfnWndProc      = nsToolkit::WindowProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = nsToolkit::mDllInstance;
    wc.hIcon            = NULL;
    wc.hCursor          = NULL;
    wc.hbrBackground    = NULL;
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = L"nsToolkitClass";
    VERIFY(::RegisterClassW(&wc) || 
           GetLastError() == ERROR_CLASS_ALREADY_EXISTS);

    // Vista API.  Mozilla is DPI Aware.
    typedef BOOL (*SetProcessDPIAwareFunc)(VOID);

    SetProcessDPIAwareFunc setDPIAware = (SetProcessDPIAwareFunc)
      GetProcAddress(LoadLibraryW(L"user32.dll"), "SetProcessDPIAware");

    if (setDPIAware)
      setDPIAware();

#ifndef WINCE
    nsUXThemeData::Initialize();
#endif
}


void
nsToolkit::Shutdown()
{
    // Crashes on certain XP machines/profiles - see bug 448104 for details
    //nsUXThemeData::Teardown();
    //VERIFY(::UnregisterClass("nsToolkitClass", nsToolkit::mDllInstance));
    ::UnregisterClassW(L"nsToolkitClass", nsToolkit::mDllInstance);
}


//-------------------------------------------------------------------------
//
// Register the window class for the internal window and create the window
//
//-------------------------------------------------------------------------
void nsToolkit::CreateInternalWindow(PRThread *aThread)
{
    
    NS_PRECONDITION(aThread, "null thread");
    mGuiThread  = aThread;

    //
    // create the internal window
    //

    mDispatchWnd = ::CreateWindowW(L"nsToolkitClass",
                                   L"NetscapeDispatchWnd",
                                  WS_DISABLED,
                                  -50, -50,
                                  10, 10,
                                  NULL,
                                  NULL,
                                  nsToolkit::mDllInstance,
                                  NULL);

    VERIFY(mDispatchWnd);
}


//-------------------------------------------------------------------------
//
// Create a new thread and run the message pump in there
//
//-------------------------------------------------------------------------
void nsToolkit::CreateUIThread()
{
    PRMonitor *monitor = ::PR_NewMonitor();

    ::PR_EnterMonitor(monitor);

    ThreadInitInfo *ti = new ThreadInitInfo();
    ti->monitor = monitor;
    ti->toolkit = this;

    // create a gui thread
    mGuiThread = ::PR_CreateThread(PR_SYSTEM_THREAD,
                                    RunPump,
                                    (void*)ti,
                                    PR_PRIORITY_NORMAL,
                                    PR_LOCAL_THREAD,
                                    PR_UNJOINABLE_THREAD,
                                    0);

    // wait for the gui thread to start
    while(gThreadState == PR_FALSE) {
        ::PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);
    }

    // at this point the thread is running
    ::PR_ExitMonitor(monitor);
    ::PR_DestroyMonitor(monitor);
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
    // Store the thread ID of the thread containing the message pump.  
    // If no thread is provided create one
    if (NULL != aThread) {
        CreateInternalWindow(aThread);
    } else {
        // create a thread where the message pump will run
        CreateUIThread();
    }

    nsWidgetAtoms::RegisterAtoms();

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsToolkit WindowProc. Used to call methods on the "main GUI thread"...
//
//-------------------------------------------------------------------------
LRESULT CALLBACK nsToolkit::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, 
                                       LPARAM lParam)
{
    switch (msg) {
#ifndef WINCE
        case WM_SYSCOLORCHANGE:
        {
          // WM_SYSCOLORCHANGE messages are only dispatched to top
          // level windows but NS_SYSCOLORCHANGE messages must be dispatched
          // to all windows including child windows. We dispatch these messages 
          // from the nsToolkit because if we are running embedded we may not 
          // have a top-level nsIWidget window.
          
          // On WIN32 all windows are automatically invalidated after the 
          // WM_SYSCOLORCHANGE is dispatched so the window is drawn using
          // the current system colors.
          nsWindow::GlobalMsgWindowProc(hWnd, msg, wParam, lParam);
        }
#endif
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
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


PRBool nsToolkit::InitVersionInfo()
{
  static PRBool isInitialized = PR_FALSE;

  if (!isInitialized)
  {
    isInitialized = PR_TRUE;

#ifndef WINCE
    OSVERSIONINFO osversion;
    osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    ::GetVersionEx(&osversion);

    if (osversion.dwMajorVersion == 5)  { 
      nsToolkit::mIsWinXP = (osversion.dwMinorVersion == 1);
    }
#endif
  }

  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::MouseTrailer() : mMouseTrailerWindow(nsnull), mCaptureWindow(nsnull),
  mIsInCaptureMode(PR_FALSE), mEnabled(PR_TRUE)
{
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::~MouseTrailer()
{
  DestroyTimer();
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::SetMouseTrailerWindow(HWND aWnd) 
{
  if (mMouseTrailerWindow != aWnd && mTimer) {
    // Make sure TimerProc is fired at least once for the old window
    TimerProc(nsnull, nsnull);
  }
  mMouseTrailerWindow = aWnd;
  CreateTimer();
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::SetCaptureWindow(HWND aWnd) 
{ 
  mCaptureWindow = aWnd;
  if (mCaptureWindow) {
    mIsInCaptureMode = PR_TRUE;
  }
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsresult MouseTrailer::CreateTimer()
{
  if (mTimer || !mEnabled) {
    return NS_OK;
  } 

  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return mTimer->InitWithFuncCallback(TimerProc, nsnull, 200,
                                      nsITimer::TYPE_REPEATING_SLACK);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::DestroyTimer()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::TimerProc(nsITimer* aTimer, void* aClosure)
{
  MouseTrailer *mtrailer = nsToolkit::gMouseTrailer;
  NS_ASSERTION(mtrailer, "MouseTrailer still firing after deletion!");

  // Check to see if we are in mouse capture mode,
  // Once capture ends we could still get back one more timer event.
  // Capture could end outside our window.
  // Also, for some reason when the mouse is on the frame it thinks that
  // it is inside the window that is being captured.
  if (mtrailer->mCaptureWindow) {
    if (mtrailer->mCaptureWindow != mtrailer->mMouseTrailerWindow) {
      return;
    }
  } else {
    if (mtrailer->mIsInCaptureMode) {
      // mMouseTrailerWindow could be bad from rolling over the frame, so clear 
      // it if we were capturing and now this is the first timer callback 
      // since we canceled the capture
      mtrailer->mMouseTrailerWindow = nsnull;
      mtrailer->mIsInCaptureMode = PR_FALSE;
      return;
    }
  }

  if (mtrailer->mMouseTrailerWindow && ::IsWindow(mtrailer->mMouseTrailerWindow)) {
    POINT mp;
    DWORD pos = ::GetMessagePos();
    mp.x = GET_X_LPARAM(pos);
    mp.y = GET_Y_LPARAM(pos);
    HWND mouseWnd = ::WindowFromPoint(mp);
    if (mtrailer->mMouseTrailerWindow != mouseWnd) {
#ifndef WINCE
      // Notify someone that a mouse exit happened.
      PostMessage(mtrailer->mMouseTrailerWindow, WM_MOUSELEAVE, NULL, NULL);
#endif

      // we are out of this window, destroy timer
      mtrailer->DestroyTimer();
      mtrailer->mMouseTrailerWindow = nsnull;
    }
  } else {
    mtrailer->DestroyTimer();
    mtrailer->mMouseTrailerWindow = nsnull;
  }
}

