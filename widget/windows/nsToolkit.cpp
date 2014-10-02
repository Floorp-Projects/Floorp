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
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include <objbase.h>
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

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
    MOZ_COUNT_CTOR(nsToolkit);

#if defined(MOZ_STATIC_COMPONENT_LIBS)
    nsToolkit::Startup(GetModuleHandle(nullptr));
#endif

    if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
      mD3D9Timer = do_CreateInstance("@mozilla.org/timer;1");
      mD3D9Timer->InitWithFuncCallback(::StartAllowingD3D9,
                                       nullptr,
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
