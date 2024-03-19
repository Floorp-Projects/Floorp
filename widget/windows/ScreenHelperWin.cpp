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

static void GetDisplayInfo(const char16ptr_t aName,
                           hal::ScreenOrientation& aOrientation,
                           uint16_t& aAngle, bool& aIsPseudoDisplay,
                           uint32_t& aRefreshRate) {
  DISPLAY_DEVICEW displayDevice = {.cb = sizeof(DISPLAY_DEVICEW)};

  // XXX Is the pseudodisplay status really useful?
  aIsPseudoDisplay =
      EnumDisplayDevicesW(aName, 0, &displayDevice, 0) &&
      (displayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER);

  DEVMODEW mode = {.dmSize = sizeof(DEVMODEW)};
  if (!EnumDisplaySettingsW(aName, ENUM_CURRENT_SETTINGS, &mode)) {
    return;
  }
  MOZ_ASSERT(mode.dmFields & DM_DISPLAYORIENTATION);

  aRefreshRate = mode.dmDisplayFrequency;

  // conver to default/natural size
  if (mode.dmDisplayOrientation == DMDO_90 ||
      mode.dmDisplayOrientation == DMDO_270) {
    DWORD temp = mode.dmPelsHeight;
    mode.dmPelsHeight = mode.dmPelsWidth;
    mode.dmPelsWidth = temp;
  }

  bool defaultIsLandscape = mode.dmPelsWidth >= mode.dmPelsHeight;
  switch (mode.dmDisplayOrientation) {
    case DMDO_DEFAULT:
      aOrientation = defaultIsLandscape
                         ? hal::ScreenOrientation::LandscapePrimary
                         : hal::ScreenOrientation::PortraitPrimary;
      aAngle = 0;
      break;
    case DMDO_90:
      aOrientation = defaultIsLandscape
                         ? hal::ScreenOrientation::PortraitPrimary
                         : hal::ScreenOrientation::LandscapeSecondary;
      aAngle = 270;
      break;
    case DMDO_180:
      aOrientation = defaultIsLandscape
                         ? hal::ScreenOrientation::LandscapeSecondary
                         : hal::ScreenOrientation::PortraitSecondary;
      aAngle = 180;
      break;
    case DMDO_270:
      aOrientation = defaultIsLandscape
                         ? hal::ScreenOrientation::PortraitSecondary
                         : hal::ScreenOrientation::LandscapePrimary;
      aAngle = 90;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected angle");
      break;
  }
}

struct CollectMonitorsParam {
  nsTArray<RefPtr<Screen>> screens;
  nsTArray<DXGI_OUTPUT_DESC1> outputs;
};

BOOL CALLBACK CollectMonitors(HMONITOR aMon, HDC, LPRECT, LPARAM ioParam) {
  CollectMonitorsParam* cmParam =
      reinterpret_cast<CollectMonitorsParam*>(ioParam);
  BOOL success = FALSE;
  MONITORINFOEX info;
  info.cbSize = sizeof(MONITORINFOEX);
  success = ::GetMonitorInfoW(aMon, &info);
  if (!success) {
    MOZ_LOG(sScreenLog, LogLevel::Error, ("GetMonitorInfoW failed"));
    return TRUE;  // continue the enumeration
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

  auto orientation = hal::ScreenOrientation::None;
  uint16_t angle = 0;
  bool isPseudoDisplay = false;
  uint32_t refreshRate = 0;
  GetDisplayInfo(info.szDevice, orientation, angle, isPseudoDisplay,
                 refreshRate);

  // Is this an HDR screen? Determine this by enumerating the DeviceManager
  // outputs (adapters) and correlating the monitor associated with the
  // the output with aMon, the monitor we are considering here.
  bool isHDR = false;
  for (auto& output : cmParam->outputs) {
    if (output.Monitor == aMon) {
      // Set isHDR to true if the output has a BT2020 colorspace with EOTF2084
      // gamma curve, this indicates the system is sending an HDR format to
      // this monitor.  The colorspace returned by DXGI is very vague - we only
      // see DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 for HDR and
      // DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 for SDR modes, even if the
      // monitor is using something like YCbCr444 according to Settings
      // (System -> Display Settings -> Advanced Display).  To get more specific
      // info we would need to query the DISPLAYCONFIG values in WinGDI.
      //
      // Note that we don't check bit depth here, since as of Windows 11 22H2,
      // HDR is supported with 8bpc for lower bandwidth, where DWM converts to
      // dithered RGB8 rather than RGB10, which doesn't really matter here.
      //
      // Since RefreshScreens(), the caller of this function, is triggered
      // by WM_DISPLAYCHANGE, this will pick up changes to the monitors in
      // all the important cases (resolution/color changes by the user).
      //
      // Further reading:
      // https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
      // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level
      isHDR = (output.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
      break;
    }
  }

  MOZ_LOG(sScreenLog, LogLevel::Debug,
          ("New screen [%s (%s) %d %u %f %f %f %d %d %d]",
           ToString(rect).c_str(), ToString(availRect).c_str(), pixelDepth,
           refreshRate, contentsScaleFactor.scale, defaultCssScaleFactor.scale,
           dpi, isPseudoDisplay, uint32_t(orientation), angle));
  auto screen = MakeRefPtr<Screen>(
      rect, availRect, pixelDepth, pixelDepth, refreshRate, contentsScaleFactor,
      defaultCssScaleFactor, dpi, Screen::IsPseudoDisplay(isPseudoDisplay),
      Screen::IsHDR(isHDR), orientation, angle);
  if (info.dwFlags & MONITORINFOF_PRIMARY) {
    // The primary monitor must be the first element of the screen list.
    cmParam->screens.InsertElementAt(0, std::move(screen));
  } else {
    cmParam->screens.AppendElement(std::move(screen));
  }
  return TRUE;
}

void ScreenHelperWin::RefreshScreens() {
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refreshing screens"));

  CollectMonitorsParam cmParam;
  if (DeviceManagerDx* dx = DeviceManagerDx::Get()) {
    // Get the adapters to pass as an arg to our monitor enumeration callback.
    cmParam.outputs = dx->EnumerateOutputs();
  }
  BOOL result = ::EnumDisplayMonitors(
      nullptr, nullptr, (MONITORENUMPROC)CollectMonitors, (LPARAM)&cmParam);
  if (!result) {
    NS_WARNING("Unable to EnumDisplayMonitors");
  }
  ScreenManager::Refresh(std::move(cmParam.screens));
}

}  // namespace widget
}  // namespace mozilla
