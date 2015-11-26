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
