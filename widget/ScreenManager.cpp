/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenManager.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"

static LazyLogModule sScreenLog("WidgetScreen");

NS_IMPL_ISUPPORTS(ScreenManager, nsIScreenManager)

namespace mozilla {
namespace widget {

ScreenManager::ScreenManager()
{
}

ScreenManager::~ScreenManager()
{
}

static StaticRefPtr<ScreenManager> sSingleton;

ScreenManager&
ScreenManager::GetSingleton()
{
  if (!sSingleton) {
    sSingleton = new ScreenManager();
    ClearOnShutdown(&sSingleton);
  }
  return *sSingleton;
}

already_AddRefed<ScreenManager>
ScreenManager::GetAddRefedSingleton()
{
  RefPtr<ScreenManager> sm = &GetSingleton();
  return sm.forget();
}

void
ScreenManager::SetHelper(UniquePtr<Helper> aHelper)
{
  mHelper = Move(aHelper);
}

void
ScreenManager::Refresh(nsTArray<RefPtr<Screen>>&& aScreens)
{
  mScreenList = Move(aScreens);
}

NS_IMETHODIMP
ScreenManager::ScreenForId(uint32_t aId, nsIScreen** aOutScreen)
{
  *aOutScreen = nullptr;

  nsresult rv;
  for (auto& screen : mScreenList) {
    uint32_t id;
    rv = screen->GetId(&id);
    if (NS_SUCCEEDED(rv) && id == aId) {
      RefPtr<Screen> ret = screen;
      ret.forget(aOutScreen);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in desktop pixels.
//
NS_IMETHODIMP
ScreenManager::ScreenForRect(int32_t aX, int32_t aY,
                             int32_t aWidth, int32_t aHeight,
                             nsIScreen** aOutScreen)
{
  if (mScreenList.IsEmpty()) {
    MOZ_LOG(sScreenLog, LogLevel::Warning,
            ("No screen available. This can happen in xpcshell."));
    RefPtr<Screen> ret = new Screen(LayoutDeviceIntRect(), LayoutDeviceIntRect(),
                                    0, 0,
                                    DesktopToLayoutDeviceScale(),
                                    CSSToLayoutDeviceScale());
    ret.forget(aOutScreen);
    return NS_OK;
  }

  // Optimize for the common case. If the number of screens is only
  // one then just return the primary screen.
  if (mScreenList.Length() == 1) {
    return GetPrimaryScreen(aOutScreen);
  }

  // which screen should we return?
  Screen* which = mScreenList[0].get();

  // walk the list of screens and find the one that has the most
  // surface area.
  uint32_t area = 0;
  DesktopIntRect windowRect(aX, aY, aWidth, aHeight);
  for (auto& screen : mScreenList) {
    int32_t  x, y, width, height;
    x = y = width = height = 0;
    screen->GetRectDisplayPix(&x, &y, &width, &height);
    // calculate the surface area
    DesktopIntRect screenRect(x, y, width, height);
    screenRect.IntersectRect(screenRect, windowRect);
    uint32_t tempArea = screenRect.width * screenRect.height;
    if (tempArea >= area) {
      which = screen.get();
      area = tempArea;
    }
  }

  RefPtr<Screen> ret = which;
  ret.forget(aOutScreen);
  return NS_OK;
}

// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP
ScreenManager::GetPrimaryScreen(nsIScreen** aPrimaryScreen)
{
  if (mScreenList.IsEmpty()) {
    MOZ_LOG(sScreenLog, LogLevel::Warning,
            ("No screen available. This can happen in xpcshell."));
    RefPtr<Screen> ret = new Screen(LayoutDeviceIntRect(), LayoutDeviceIntRect(),
                                    0, 0,
                                    DesktopToLayoutDeviceScale(),
                                    CSSToLayoutDeviceScale());
    ret.forget(aPrimaryScreen);
    return NS_OK;
  }

  RefPtr<Screen> ret = mScreenList[0];
  ret.forget(aPrimaryScreen);
  return NS_OK;
}

NS_IMETHODIMP
ScreenManager::GetSystemDefaultScale(float* aDefaultScale)
{
  if (mHelper) {
    *aDefaultScale = mHelper->GetSystemDefaultScale();
    return NS_OK;
  }
  *aDefaultScale = 1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenManager::ScreenForNativeWidget(void* aWidget, nsIScreen** aOutScreen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace widget
} // namespace mozilla
