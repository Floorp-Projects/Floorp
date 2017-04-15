/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperWin.h"

#include "mozilla/Logging.h"
#include "nsTArray.h"
#include "WinUtils.h"

static LazyLogModule sScreenLog("WidgetScreen");

namespace mozilla {
namespace widget {

BOOL CALLBACK
CollectMonitors(HMONITOR aMon, HDC, LPRECT, LPARAM ioParam)
{
  auto screens = reinterpret_cast<nsTArray<RefPtr<Screen>>*>(ioParam);
  BOOL success = FALSE;
  MONITORINFO info;
  info.cbSize = sizeof(MONITORINFO);
  success = ::GetMonitorInfoW(aMon, &info);
  if (!success) {
    MOZ_LOG(sScreenLog, LogLevel::Error, ("GetMonitorInfoW failed"));
    return TRUE; // continue the enumeration
  }
  double scale = WinUtils::LogToPhysFactor(aMon);
  DesktopToLayoutDeviceScale contentsScaleFactor;
  if (WinUtils::IsPerMonitorDPIAware()) {
    contentsScaleFactor.scale = 1.0;
  } else {
    contentsScaleFactor.scale = scale;
  }
  CSSToLayoutDeviceScale defaultCssScaleFactor(scale);
  LayoutDeviceIntRect rect(info.rcMonitor.left, info.rcMonitor.top,
                           info.rcMonitor.right - info.rcMonitor.left,
                           info.rcMonitor.bottom - info.rcMonitor.top);
  LayoutDeviceIntRect availRect(info.rcWork.left, info.rcWork.top,
                                info.rcWork.right - info.rcWork.left,
                                info.rcWork.bottom - info.rcWork.top);
  //XXX not sure how to get this info for multiple monitors, this might be ok...
  HDC hDCScreen = ::GetDC(nullptr);
  NS_ASSERTION(hDCScreen,"GetDC Failure");
  uint32_t pixelDepth = ::GetDeviceCaps(hDCScreen, BITSPIXEL);
  ::ReleaseDC(nullptr, hDCScreen);
  if (pixelDepth == 32) {
    // If a device uses 32 bits per pixel, it's still only using 8 bits
    // per color component, which is what our callers want to know.
    // (Some devices report 32 and some devices report 24.)
    pixelDepth = 24;
  }
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("New screen [%d %d %d %d %d %f]",
                                        rect.x, rect.y, rect.width, rect.height,
                                        pixelDepth, defaultCssScaleFactor.scale));
  auto screen = new Screen(rect, availRect,
                           pixelDepth, pixelDepth,
                           contentsScaleFactor, defaultCssScaleFactor);
  if (info.dwFlags & MONITORINFOF_PRIMARY) {
    // The primary monitor must be the first element of the screen list.
    screens->InsertElementAt(0, Move(screen));
  } else {
    screens->AppendElement(Move(screen));
  }
  return TRUE;
}

void
ScreenHelperWin::RefreshScreens()
{
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refreshing screens"));

  AutoTArray<RefPtr<Screen>, 4> screens;
  BOOL result = ::EnumDisplayMonitors(nullptr, nullptr,
                                      (MONITORENUMPROC)CollectMonitors,
                                      (LPARAM)&screens);
  if (!result) {
    NS_WARNING("Unable to EnumDisplayMonitors");
  }
  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.Refresh(Move(screens));
}

} // namespace widget
} // namespace mozilla
