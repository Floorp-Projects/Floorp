/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsToolkit.h"
#include "nsWindow.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#ifdef MOZ_AIMM
#include <initguid.h>
#include "aimm.h"
#endif

NS_IMPL_ISUPPORTS1(nsToolkit, nsIToolkit)

//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;

HINSTANCE nsToolkit::mDllInstance = 0;
PRBool    nsToolkit::mIsNT        = PR_FALSE;

#ifdef MOZ_AIMM
IActiveIMMApp* nsToolkit::gAIMMApp   = NULL;
PRInt32        nsToolkit::gAIMMCount = 0;
#endif

nsWindow     *MouseTrailer::mCaptureWindow  = NULL;
nsWindow     *MouseTrailer::mHoldMouse      = NULL;
MouseTrailer *MouseTrailer::theMouseTrailer = NULL;
PRBool        MouseTrailer::gIgnoreNextCycle(PR_FALSE);
PRBool        MouseTrailer::mIsInCaptureMode(PR_FALSE);

//
// Dll entry point. Keep the dll instance
//
BOOL APIENTRY DllMain(  HINSTANCE hModule, 
                        DWORD reason, 
                        LPVOID lpReserved )
{
    switch( reason ) {
        case DLL_PROCESS_ATTACH:
            //
            // Set flag of nsToolkit::mIsNT due to using Unicode API.
            //

            OSVERSIONINFO osversion;
            ::ZeroMemory(&osversion, sizeof(OSVERSIONINFO));
            osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            ::GetVersionEx(&osversion);
            nsToolkit::mIsNT = (osversion.dwPlatformId == VER_PLATFORM_WIN32_NT) ? PR_TRUE : PR_FALSE;

            nsToolkit::mDllInstance = hModule;

            //
            // register the internal window class
            //
            WNDCLASS wc;

            wc.style            = CS_GLOBALCLASS;
            wc.lpfnWndProc      = nsToolkit::WindowProc;
            wc.cbClsExtra       = 0;
            wc.cbWndExtra       = 0;
            wc.hInstance        = nsToolkit::mDllInstance;
            wc.hIcon            = NULL;
            wc.hCursor          = NULL;
            wc.hbrBackground    = NULL;
            wc.lpszMenuName     = NULL;
            wc.lpszClassName    = "nsToolkitClass";

            VERIFY(nsToolkit::nsRegisterClass(&wc));

            break;

        case DLL_THREAD_ATTACH:
            break;
    
        case DLL_THREAD_DETACH:
            break;
    
        case DLL_PROCESS_DETACH:
            //VERIFY(nsToolkit::nsUnregisterClass("nsToolkitClass", nsToolkit::mDllInstance));
            nsToolkit::nsUnregisterClass("nsToolkitClass", nsToolkit::mDllInstance);
            break;

    }

    return TRUE;
}


//
// main for the message pump thread
//
PRBool gThreadState = PR_FALSE;

struct ThreadInitInfo {
    PRMonitor *monitor;
    nsToolkit *toolkit;
};

void RunPump(void* arg)
{
    ThreadInitInfo *info = (ThreadInitInfo*)arg;
    ::PR_EnterMonitor(info->monitor);

#ifdef MOZ_AIMM
    // Start Active Input Method Manager on this thread
    if(nsToolkit::gAIMMApp)
        nsToolkit::gAIMMApp->Activate(TRUE);
#endif

    // do registration and creation in this thread
    info->toolkit->CreateInternalWindow(PR_GetCurrentThread());

    gThreadState = PR_TRUE;

    ::PR_Notify(info->monitor);
    ::PR_ExitMonitor(info->monitor);

    delete info;

    // Process messages
    MSG msg;
    while (nsToolkit::nsGetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        nsToolkit::nsDispatchMessage(&msg);
    }
}

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
    NS_INIT_REFCNT();
    mGuiThread  = NULL;
    mDispatchWnd = 0;

#ifdef MOZ_AIMM
    //
    // Initialize COM since create Active Input Method Manager object
    //
    if (!nsToolkit::gAIMMCount)
      ::CoInitialize(NULL);

    if(!nsToolkit::gAIMMApp)
      ::CoCreateInstance(CLSID_CActiveIMM, NULL, CLSCTX_INPROC_SERVER, IID_IActiveIMMApp, (void**) &nsToolkit::gAIMMApp);

    nsToolkit::gAIMMCount++;
#endif
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    NS_PRECONDITION(::IsWindow(mDispatchWnd), "Invalid window handle");

#ifdef MOZ_AIMM
    nsToolkit::gAIMMCount--;

    if (!nsToolkit::gAIMMCount) {
        if(nsToolkit::gAIMMApp) {
            nsToolkit::gAIMMApp->Deactivate();
            nsToolkit::gAIMMApp->Release();
            nsToolkit::gAIMMApp = NULL;
        }
        ::CoUninitialize();
    }
#endif

    // Destroy the Dispatch Window
    ::DestroyWindow(mDispatchWnd);
    mDispatchWnd = NULL;

    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
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
    mDispatchWnd = nsToolkit::nsCreateWindow("nsToolkitClass",
                                       "NetscapeDispatchWnd",
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

#ifdef MOZ_AIMM
        // Start Active Input Method Manager on this thread
        if(nsToolkit::gAIMMApp)
            nsToolkit::gAIMMApp->Activate(TRUE);
#endif

        CreateInternalWindow(aThread);
    } else {
        // create a thread where the message pump will run
        CreateUIThread();
    }
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
        case WM_CALLMETHOD:
        {
            MethodInfo *info = (MethodInfo *)lParam;
            return info->Invoke();
        }
    }

#ifdef MOZ_AIMM
    if(nsToolkit::gAIMMApp) {
        LRESULT lResult;
        if (nsToolkit::gAIMMApp->OnDefWindowProc(hWnd, msg, wParam, lParam, &lResult) == S_OK)
            return lResult;
    }
#endif
    return nsToolkit::nsDefWindowProc(hWnd, msg, wParam, lParam);
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


//-------------------------------------------------------------------------
// Wrapper API of CreateWindowEx() for Unicode Window class support
//-------------------------------------------------------------------------

HWND nsToolkit::nsCreateWindowEx(DWORD dwExStyle,
                                 LPCSTR lpClassName,
                                 LPCSTR lpWindowName,
                                 DWORD dwStyle,
                                 int x, int y,
                                 int nWidth, int nHeight,
                                 HWND hWndParent,
                                 HMENU hMenu,
                                 HINSTANCE hInstance,
                                 LPVOID lpParam)
{
    if (!nsToolkit::mIsNT)
    {
        // Windows 95/98/Me do not support Unicode API of CreateWindow.

        return ::CreateWindowExA(dwExStyle,
                                 lpClassName,
                                 lpWindowName,
                                 dwStyle,
                                 x, y,
                                 nWidth, nHeight,
                                 hWndParent,
                                 hMenu,
                                 hInstance,
                                 lpParam);
    }

    HWND hWnd;
    int len;
    int needLen;
    LPWSTR lpClassStr = nsnull;
    LPWSTR lpWindowStr = nsnull;

    if (lpClassName)
    {
        len = lstrlenA(lpClassName) + 1;
        needLen = ::MultiByteToWideChar(CP_ACP, 0, lpClassName, len, NULL, 0);
        if (needLen != 0)
        {
            lpClassStr = new WCHAR[needLen];
            ::MultiByteToWideChar(CP_ACP, 0, lpClassName, len, lpClassStr, needLen);
        }
    }

    if (lpWindowName)
    {
        len = lstrlenA(lpWindowName) + 1;
        needLen = ::MultiByteToWideChar(CP_ACP, 0, lpWindowName, len, NULL, 0);
        if (needLen != 0)
        {
            lpWindowStr = new WCHAR[needLen];
            ::MultiByteToWideChar(CP_ACP, 0, lpWindowName, len, lpWindowStr, needLen);
        }
    }

    hWnd = ::CreateWindowExW(dwExStyle,
                             lpClassStr,
                             lpWindowStr,
                             dwStyle,
                             x, y,
                             nWidth, nHeight,
                             hWndParent,
                             hMenu,
                             hInstance,
                             lpParam);

    if (lpClassStr)
        delete lpClassStr;

    if (lpWindowStr)
        delete lpWindowStr;

    return hWnd;
}


//-------------------------------------------------------------------------
// Wrapper API of RegisterClass() for Unicode Window class support
//-------------------------------------------------------------------------

ATOM nsToolkit::nsRegisterClass(CONST WNDCLASS *lpWndClass)
{
    if (!nsToolkit::mIsNT)
    {
        // Windows 95/98/Me do not support Unicode API of ResigterClass.

        return ::RegisterClassA(lpWndClass);
    }

     WNDCLASSW wc;
     int len;
     int needLen;
     ATOM ret;

     wc.style            = lpWndClass->style;
     wc.lpfnWndProc      = lpWndClass->lpfnWndProc;
     wc.cbClsExtra       = lpWndClass->cbClsExtra;
     wc.cbWndExtra       = lpWndClass->cbWndExtra;
     wc.hInstance        = lpWndClass->hInstance;
     wc.hIcon            = lpWndClass->hIcon;
     wc.hCursor          = lpWndClass->hCursor;
     wc.hbrBackground    = lpWndClass->hbrBackground;
     wc.lpszMenuName     = NULL;
     wc.lpszClassName    = NULL;

    if (lpWndClass->lpszMenuName)
    {
        len = lstrlenA(lpWndClass->lpszMenuName) + 1;
        needLen = ::MultiByteToWideChar(CP_ACP, 0, lpWndClass->lpszMenuName, len, NULL, 0);
        if (needLen != 0)
        {
            wc.lpszMenuName = new WCHAR[needLen];
            ::MultiByteToWideChar(CP_ACP, 0, lpWndClass->lpszMenuName, len, (LPWSTR) wc.lpszMenuName, needLen);
        }
    }

    if (lpWndClass->lpszClassName)
    {
        len = lstrlenA(lpWndClass->lpszClassName) + 1;
        needLen = ::MultiByteToWideChar(CP_ACP, 0, lpWndClass->lpszClassName, len, NULL, 0);
        if (needLen != 0)
        {
            wc.lpszClassName = new WCHAR[needLen];
            ::MultiByteToWideChar(CP_ACP, 0, lpWndClass->lpszClassName, len, (LPWSTR) wc.lpszClassName, needLen);
        }
    }

    ret = ::RegisterClassW(&wc);

    if (wc.lpszMenuName)
        delete [] (LPWSTR) wc.lpszMenuName;

    if (wc.lpszClassName)
        delete [] (LPWSTR) wc.lpszClassName;

    return ret;
}


//-------------------------------------------------------------------------
// Wrapper API of UnregisterClass() for Unicode Window class support
//-------------------------------------------------------------------------

BOOL nsToolkit::nsUnregisterClass(LPCSTR lpClassName, HINSTANCE hInstance)
{
    if (!nsToolkit::mIsNT)
    {
        // Windows 95/98/Me do not support Unicode API of UnResigterClass.

        return ::UnregisterClassA(lpClassName, hInstance);
    }

    LPWSTR lpClassUnicode;
    int len;
    int needLen;
    BOOL ret;

    if (lpClassName)
    {
        len = lstrlenA(lpClassName) + 1;
        needLen = ::MultiByteToWideChar(CP_ACP, 0, lpClassName, len, NULL, 0);
        if (needLen)
        {
            lpClassUnicode = new WCHAR[needLen];
            ::MultiByteToWideChar(CP_ACP, 0, lpClassName, len, lpClassUnicode, needLen);
        }
    }

    ret = ::UnregisterClassW(lpClassUnicode, hInstance);

    if (lpClassUnicode)
        delete [] lpClassUnicode;

    return ret;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer * MouseTrailer::GetMouseTrailer(DWORD aThreadID) {
  if (nsnull == MouseTrailer::theMouseTrailer) {
    MouseTrailer::theMouseTrailer = new MouseTrailer();
  }
  return MouseTrailer::theMouseTrailer;

}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsWindow * MouseTrailer::GetMouseTrailerWindow() {
  return MouseTrailer::mHoldMouse;
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::SetMouseTrailerWindow(nsWindow * aNSWin) {
  MouseTrailer::mHoldMouse = aNSWin;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::MouseTrailer()
{
  mTimerId         = 0;
  mHoldMouse       = NULL;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::~MouseTrailer()
{
    DestroyTimer();
    if (mHoldMouse) {
        NS_RELEASE(mHoldMouse);
    }
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
UINT MouseTrailer::CreateTimer()
{
  if (!mTimerId) {
    mTimerId = ::SetTimer(NULL, 0, 200, (TIMERPROC)MouseTrailer::TimerProc);
  } 

  return mTimerId;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::DestroyTimer()
{
  if (mTimerId) {
    ::KillTimer(NULL, mTimerId);
    mTimerId = 0;
  }

}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::SetCaptureWindow(nsWindow * aNSWin) 
{ 
  mCaptureWindow = aNSWin;
  if (nsnull != mCaptureWindow) {
    mIsInCaptureMode = PR_TRUE;
  }
}

#define TIMER_DEBUG 1
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void CALLBACK MouseTrailer::TimerProc(HWND hWnd, UINT msg, UINT event, DWORD time)
{
  // Check to see if we are in mouse capture mode,
  // Once capture ends we could still get back one more timer event 
  // Capture could end outside our window
  // Also, for some reason when the mouse is on the frame it thinks that
  // it is inside the window that is being captured.
  if (nsnull != MouseTrailer::mCaptureWindow) {
    if (MouseTrailer::mCaptureWindow != MouseTrailer::mHoldMouse) {
      return;
    }
  } else {
    if (mIsInCaptureMode) {
      // the mHoldMouse could be bad from rolling over the frame, so clear 
      // it if we were capturing and now this is the first timer call back 
      // since we canceled the capture
      MouseTrailer::mHoldMouse = nsnull;
      mIsInCaptureMode = PR_FALSE;
      return;
    }
  }

    if (MouseTrailer::mHoldMouse && ::IsWindow(MouseTrailer::mHoldMouse->GetWindowHandle())) {
      if (gIgnoreNextCycle) {
        gIgnoreNextCycle = PR_FALSE;
      }
      else {
        POINT mp;
        DWORD pos = ::GetMessagePos();
        mp.x = LOWORD(pos);
        mp.y = HIWORD(pos);

        if (::WindowFromPoint(mp) != mHoldMouse->GetWindowHandle()) {
            int64 time = PR_Now(); // time in milliseconds
            ::ScreenToClient(mHoldMouse->GetWindowHandle(), &mp);

            //notify someone that a mouse exit happened
            if (nsnull != MouseTrailer::mHoldMouse) {
              MouseTrailer::mHoldMouse->DispatchMouseEvent(NS_MOUSE_EXIT);
            }
  
            // we are out of this window and of any window, destroy timer
            MouseTrailer::theMouseTrailer->DestroyTimer();
            MouseTrailer::mHoldMouse = NULL;
        }
      }
    } else {
      MouseTrailer::theMouseTrailer->DestroyTimer();
      MouseTrailer::mHoldMouse = NULL;
    }
}


