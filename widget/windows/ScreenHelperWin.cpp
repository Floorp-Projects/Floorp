/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperWin.h"

#include "mozilla/Logging.h"
#include "nsTArray.h"
#include "WinUtils.h"

static mozilla::LazyLogModule sScreenLog("WidgetScreen");

namespace mozilla {
namespace widget {

BOOL CALLBACK
CollectMonitors(HMONITOR aMon, HDC, LPRECT, LPARAM ioParam)
{
  auto screens = reinterpret_cast<nsTArray<RefPtr<Screen>>*>(ioParam);
  BOOL success = FALSE;
  MONITORINFOEX info;
  info.cbSize = sizeof(MONITORINFOEX);
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

  HDC hDC = CreateDC(nullptr, info.szDevice, nullptr, nullptr);
  if (!hDC) {
    MOZ_LOG(sScreenLog, LogLevel::Error, ("CollectMonitors CreateDC failed"));
    return TRUE;
  }
  uint32_t pixelDepth = ::GetDeviceCaps(hDC, BITSPIXEL);
  DeleteDC(hDC);
  if (pixelDepth == 32) {
    // If a device uses 32 bits per pixel, it's still only using 8 bits
    // per color component, which is what our callers want to know.
    // (Some devices report 32 and some devices report 24.)
    pixelDepth = 24;
  }

  float dpi = WinUtils::MonitorDPI(aMon);
  MOZ_LOG(sScreenLog, LogLevel::Debug,
           ("New screen [%d %d %d %d (%d %d %d %d) %d %f %f %f]",
            rect.X(), rect.Y(), rect.Width(), rect.Height(),
            availRect.X(), availRect.Y(), availRect.Width(), availRect.Height(),
            pixelDepth, contentsScaleFactor.scale, defaultCssScaleFactor.scale,
            dpi));
  auto screen = new Screen(rect, availRect,
                           pixelDepth, pixelDepth,
                           contentsScaleFactor, defaultCssScaleFactor,
                           dpi);
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
