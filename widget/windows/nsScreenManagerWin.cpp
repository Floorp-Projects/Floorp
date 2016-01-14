/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenManagerWin.h"
#include "mozilla/gfx/2D.h"
#include "nsScreenWin.h"
#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "WinUtils.h"

using namespace mozilla;

BOOL CALLBACK CountMonitors(HMONITOR, HDC, LPRECT, LPARAM ioCount);

nsScreenManagerWin::nsScreenManagerWin()
  : mNumberOfScreens(0)
{
  // nothing to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerWin::~nsScreenManagerWin()
{
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenManagerWin, nsIScreenManager)


//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
// NOTE: For this "single-monitor" impl, we just always return the cached primary
//        screen. This should change when a multi-monitor impl is done.
//
nsIScreen* 
nsScreenManagerWin::CreateNewScreenObject(HMONITOR inScreen)
{
  nsIScreen* retScreen = nullptr;
  
  // look through our screen list, hoping to find it. If it's not there,
  // add it and return the new one.
  for (unsigned i = 0; i < mScreenList.Length(); ++i) {
    ScreenListItem& curr = mScreenList[i];
    if (inScreen == curr.mMon) {
      NS_IF_ADDREF(retScreen = curr.mScreen.get());
      return retScreen;
    }
  } // for each screen.
 
  retScreen = new nsScreenWin(inScreen);
  mScreenList.AppendElement(ScreenListItem(inScreen, retScreen));

  NS_IF_ADDREF(retScreen);
  return retScreen;
}

NS_IMETHODIMP
nsScreenManagerWin::ScreenForId(uint32_t aId, nsIScreen **outScreen)
{
  *outScreen = nullptr;

  for (unsigned i = 0; i < mScreenList.Length(); ++i) {
    ScreenListItem& curr = mScreenList[i];
    uint32_t id;
    nsresult rv = curr.mScreen->GetId(&id);
    if (NS_SUCCEEDED(rv) && id == aId) {
      NS_IF_ADDREF(*outScreen = curr.mScreen.get());
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

//
// ScreenForRect 
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in logical screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerWin::ScreenForRect(int32_t inLeft, int32_t inTop,
                                  int32_t inWidth, int32_t inHeight,
                                  nsIScreen **outScreen)
{
  if (!(inWidth || inHeight)) {
    NS_WARNING("trying to find screen for sizeless window, using primary monitor");
    *outScreen = CreateNewScreenObject(nullptr);    // addrefs
    return NS_OK;
  }

  gfx::Rect logicalBounds(inLeft, inTop, inWidth, inHeight);
  HMONITOR genScreen = widget::WinUtils::MonitorFromRect(logicalBounds);
 
  *outScreen = CreateNewScreenObject(genScreen);    // addrefs
 
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerWin::GetPrimaryScreen(nsIScreen** aPrimaryScreen) 
{
  *aPrimaryScreen = CreateNewScreenObject(nullptr);    // addrefs  
  return NS_OK;
  
} // GetPrimaryScreen


//
// CountMonitors
//
// Will be called once for every monitor in the system. Just 
// increments the parameter, which holds a ptr to a PRUin32 holding the
// count up to this point.
//
BOOL CALLBACK
CountMonitors(HMONITOR, HDC, LPRECT, LPARAM ioParam)
{
  uint32_t* countPtr = reinterpret_cast<uint32_t*>(ioParam);
  ++(*countPtr);

  return TRUE; // continue the enumeration

} // CountMonitors


//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerWin::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
  if (mNumberOfScreens)
    *aNumberOfScreens = mNumberOfScreens;
  else {
    uint32_t count = 0;
    BOOL result = ::EnumDisplayMonitors(nullptr, nullptr, (MONITORENUMPROC)CountMonitors, (LPARAM)&count);
    if (!result)
      return NS_ERROR_FAILURE;
    *aNumberOfScreens = mNumberOfScreens = count;
  }

  return NS_OK;
  
} // GetNumberOfScreens

NS_IMETHODIMP
nsScreenManagerWin::GetSystemDefaultScale(float *aDefaultScale)
{
  HMONITOR primary = widget::WinUtils::GetPrimaryMonitor();
  *aDefaultScale = float(widget::WinUtils::LogToPhysFactor(primary));
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerWin::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
  HMONITOR mon = MonitorFromWindow((HWND) aWidget, MONITOR_DEFAULTTOPRIMARY);
  *outScreen = CreateNewScreenObject(mon);
  return NS_OK;
}
