/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsToolkit.h"
#include "nsWindow.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"

HINSTANCE nsToolkit::mDllInstance = 0;

nsWindow     *MouseTrailer::mHoldMouse;
MouseTrailer *MouseTrailer::theMouseTrailer;

//
// Dll entry point. Keep the dll instance
//
BOOL APIENTRY DllMain(  HINSTANCE hModule, 
                        DWORD reason, 
                        LPVOID lpReserved )
{
    switch( reason ) {
        case DLL_PROCESS_ATTACH:
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

            VERIFY(::RegisterClass(&wc));

            break;

        case DLL_THREAD_ATTACH:
            break;
    
        case DLL_THREAD_DETACH:
            break;
    
        case DLL_PROCESS_DETACH:
            //VERIFY(::UnregisterClass("nsToolkitClass", nsToolkit::mDllInstance));
            ::UnregisterClass("nsToolkitClass", nsToolkit::mDllInstance);
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

    // do registration and creation in this thread
    info->toolkit->CreateInternalWindow(PR_GetCurrentThread());

    gThreadState = PR_TRUE;

    ::PR_Notify(info->monitor);
    ::PR_ExitMonitor(info->monitor);

    delete info;

    // Process messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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
    NS_INIT_REFCNT();
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
}


//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS(nsToolkit, NS_ITOOLKIT_IID)


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
    mDispatchWnd = ::CreateWindow("nsToolkitClass",
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
void nsToolkit::Init(PRThread *aThread)
{
    // Store the thread ID of the thread containing the message pump.  
    // If no thread is provided create one
    if (NULL != aThread) {
        CreateInternalWindow(aThread);
    }
    else {
        // create a thread where the message pump will run
        CreateUIThread();
    }
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

    return ::DefWindowProc(hWnd, msg, wParam, lParam);
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
    mTimerId = 0;
    mHoldMouse = NULL;
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

#define TIMER_DEBUG 1
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void CALLBACK MouseTrailer::TimerProc(HWND hWnd, UINT msg, UINT event, DWORD time)
{
    if (MouseTrailer::mHoldMouse && ::IsWindow(MouseTrailer::mHoldMouse->GetWindowHandle())) {
    

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
    } else {
      MouseTrailer::theMouseTrailer->DestroyTimer();
      MouseTrailer::mHoldMouse = NULL;
    }
    
}


