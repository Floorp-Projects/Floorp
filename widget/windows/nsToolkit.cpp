/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsToolkit.h"
#include "nsAppShell.h"
#include "nsWindow.h"
#include "nsWidgetsCID.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include <objbase.h>
#include <initguid.h>
#include "WinUtils.h"

#include "nsUXThemeData.h"

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

using namespace mozilla::widget;

nsToolkit* nsToolkit::gToolkit = nullptr;
HINSTANCE nsToolkit::mDllInstance = 0;
static const unsigned long kD3DUsageDelay = 5000;

static void
StartAllowingD3D9(nsITimer *aTimer, void *aClosure)
{
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    nsWindow::StartAllowingD3D9(true);
  }
}

MouseTrailer*       nsToolkit::gMouseTrailer;

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
    MOZ_COUNT_CTOR(nsToolkit);

#if defined(MOZ_STATIC_COMPONENT_LIBS)
    nsToolkit::Startup(GetModuleHandle(NULL));
#endif

    gMouseTrailer = &mMouseTrailer;

    if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
      mD3D9Timer = do_CreateInstance("@mozilla.org/timer;1");
      mD3D9Timer->InitWithFuncCallback(::StartAllowingD3D9,
                                       NULL,
                                       kD3DUsageDelay,
                                       nsITimer::TYPE_ONE_SHOT);
    }
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    MOZ_COUNT_DTOR(nsToolkit);
    gMouseTrailer = nullptr;
}

void
nsToolkit::Startup(HMODULE hModule)
{
    nsToolkit::mDllInstance = hModule;
    WinUtils::Initialize();
    nsUXThemeData::Initialize();
}

void
nsToolkit::Shutdown()
{
    delete gToolkit;
    gToolkit = nullptr;
}

void
nsToolkit::StartAllowingD3D9()
{
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    nsToolkit::GetToolkit()->mD3D9Timer->Cancel();
    nsWindow::StartAllowingD3D9(false);
  }
}

//-------------------------------------------------------------------------
//
// Return the nsToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
// static
nsToolkit* nsToolkit::GetToolkit()
{
  if (!gToolkit) {
    gToolkit = new nsToolkit();
  }

  return gToolkit;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::MouseTrailer() : mMouseTrailerWindow(nullptr), mCaptureWindow(nullptr),
  mIsInCaptureMode(false), mEnabled(true)
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
    TimerProc(nullptr, nullptr);
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
    mIsInCaptureMode = true;
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

  return mTimer->InitWithFuncCallback(TimerProc, nullptr, 200,
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
    mTimer = nullptr;
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
      mtrailer->mMouseTrailerWindow = nullptr;
      mtrailer->mIsInCaptureMode = false;
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
      // Notify someone that a mouse exit happened.
      PostMessage(mtrailer->mMouseTrailerWindow, WM_MOUSELEAVE, 0, 0);

      // we are out of this window, destroy timer
      mtrailer->DestroyTimer();
      mtrailer->mMouseTrailerWindow = nullptr;
    }
  } else {
    mtrailer->DestroyTimer();
    mtrailer->mMouseTrailerWindow = nullptr;
  }
}

