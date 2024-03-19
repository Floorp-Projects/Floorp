/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenManager.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#ifdef MOZ_WAYLAND
#  include "mozilla/WidgetUtilsGtk.h"
#endif /* MOZ_WAYLAND */

static mozilla::LazyLogModule sScreenLog("WidgetScreen");

namespace mozilla::widget {

NS_IMPL_ISUPPORTS(ScreenManager, nsIScreenManager)

ScreenManager::ScreenManager() = default;

ScreenManager::~ScreenManager() = default;

static StaticRefPtr<ScreenManager> sSingleton;

ScreenManager& ScreenManager::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new ScreenManager();
    ClearOnShutdown(&sSingleton);
  }
  return *sSingleton;
}

already_AddRefed<ScreenManager> ScreenManager::GetAddRefedSingleton() {
  RefPtr<ScreenManager> sm = &GetSingleton();
  return sm.forget();
}

void ScreenManager::SetHelper(UniquePtr<Helper> aHelper) {
  mHelper = std::move(aHelper);
}

// static
void ScreenManager::Refresh(nsTArray<RefPtr<Screen>>&& aScreens) {
  if (PastShutdownPhase(ShutdownPhase::XPCOMShutdown)) {
    // We don't refresh screen data if starting XPCOM shutdown path.
    // GetSingleton returns invalid data since it is freed.
    return;
  }
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refresh screens"));
  GetSingleton().RefreshInternal(std::move(aScreens));
}

void ScreenManager::Refresh(nsTArray<mozilla::dom::ScreenDetails>&& aScreens) {
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refresh screens from IPC"));

  AutoTArray<RefPtr<Screen>, 4> screens;
  for (auto& screen : aScreens) {
    screens.AppendElement(new Screen(screen));
  }
  RefreshInternal(std::move(screens));
}

void ScreenManager::RefreshInternal(nsTArray<RefPtr<Screen>>&& aScreens) {
  mScreenList = std::move(aScreens);

  CopyScreensToAllRemotesIfIsParent();
  if (nsCOMPtr<nsIObserverService> s = services::GetObserverService()) {
    s->NotifyObservers(nullptr, "screen-information-changed", nullptr);
  }
}

template <class Range>
void ScreenManager::CopyScreensToRemoteRange(Range aRemoteRange) {
  AutoTArray<dom::ScreenDetails, 4> screens;
  for (auto& screen : mScreenList) {
    screens.AppendElement(screen->ToScreenDetails());
  }
  for (auto cp : aRemoteRange) {
    MOZ_LOG(sScreenLog, LogLevel::Debug,
            ("Send screens to [Pid %d]", cp->Pid()));
    if (!cp->SendRefreshScreens(screens)) {
      MOZ_LOG(sScreenLog, LogLevel::Error,
              ("SendRefreshScreens to [Pid %d] failed", cp->Pid()));
    }
  }
}

void ScreenManager::CopyScreensToRemote(dom::ContentParent* aContentParent) {
  MOZ_ASSERT(aContentParent);
  MOZ_ASSERT(XRE_IsParentProcess());

  auto range = {aContentParent};
  CopyScreensToRemoteRange(range);
}

void ScreenManager::CopyScreensToAllRemotesIfIsParent() {
  if (XRE_IsContentProcess()) {
    return;
  }

  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refreshing all ContentParents"));

  CopyScreensToRemoteRange(
      dom::ContentParent::AllProcesses(dom::ContentParent::eLive));
}

// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in desktop pixels.
//
NS_IMETHODIMP
ScreenManager::ScreenForRect(int32_t aX, int32_t aY, int32_t aWidth,
                             int32_t aHeight, nsIScreen** aOutScreen) {
  DesktopIntRect rect(aX, aY, aWidth, aHeight);
  nsCOMPtr<nsIScreen> screen = ScreenForRect(rect);
  screen.forget(aOutScreen);
  return NS_OK;
}

already_AddRefed<Screen> ScreenManager::ScreenForRect(
    const DesktopIntRect& aRect) {
#if defined(MOZ_WAYLAND) && defined(MOZ_LOGGING)
  static bool inWayland = GdkIsWaylandDisplay();
  if (inWayland) {
    MOZ_LOG(sScreenLog, LogLevel::Warning,
            ("Getting screen in wayland, primary display will be returned."));
  }
#endif

  if (mScreenList.IsEmpty()) {
    MOZ_LOG(sScreenLog, LogLevel::Warning,
            ("No screen available. This can happen in xpcshell."));
    auto screen = MakeRefPtr<Screen>(
        LayoutDeviceIntRect(), LayoutDeviceIntRect(), 0, 0, 0,
        DesktopToLayoutDeviceScale(), CSSToLayoutDeviceScale(), 96 /* dpi */,
        Screen::IsPseudoDisplay::No, hal::ScreenOrientation::None, 0);
    return screen.forget();
  }

  // Optimize for the common case. If the number of screens is only
  // one then just return the primary screen.
  if (mScreenList.Length() == 1) {
    return GetPrimaryScreen();
  }

  // which screen should we return?
  Screen* which = mScreenList[0].get();

  // walk the list of screens and find the one that has the most
  // surface area.
  uint32_t area = 0;
  for (auto& screen : mScreenList) {
    int32_t x, y, width, height;
    x = y = width = height = 0;
    screen->GetRectDisplayPix(&x, &y, &width, &height);
    // calculate the surface area
    DesktopIntRect screenRect(x, y, width, height);
    screenRect.IntersectRect(screenRect, aRect);
    uint32_t tempArea = screenRect.Area();
    if (tempArea > area) {
      which = screen.get();
      area = tempArea;
    }
  }

  // If the rect intersects one or more screen,
  // return the screen that has the largest intersection.
  if (area > 0) {
    RefPtr<Screen> ret = which;
    return ret.forget();
  }

  // If the rect does not intersect a screen, find
  // a screen that is nearest to the rect.
  uint32_t distance = UINT32_MAX;
  for (auto& screen : mScreenList) {
    int32_t x, y, width, height;
    x = y = width = height = 0;
    screen->GetRectDisplayPix(&x, &y, &width, &height);

    uint32_t distanceX = 0;
    if (aRect.x > (x + width)) {
      distanceX = aRect.x - (x + width);
    } else if (aRect.XMost() < x) {
      distanceX = x - aRect.XMost();
    }

    uint32_t distanceY = 0;
    if (aRect.y > (y + height)) {
      distanceY = aRect.y - (y + height);
    } else if (aRect.YMost() < y) {
      distanceY = y - aRect.YMost();
    }

    uint32_t tempDistance = distanceX * distanceX + distanceY * distanceY;
    if (tempDistance < distance) {
      which = screen.get();
      distance = tempDistance;
      if (distance == 0) {
        break;
      }
    }
  }

  RefPtr<Screen> ret = which;
  return ret.forget();
}

// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
already_AddRefed<Screen> ScreenManager::GetPrimaryScreen() {
  if (mScreenList.IsEmpty()) {
    MOZ_LOG(sScreenLog, LogLevel::Warning,
            ("No screen available. This can happen in xpcshell."));
    return MakeAndAddRef<Screen>(
        LayoutDeviceIntRect(), LayoutDeviceIntRect(), 0, 0, 0,
        DesktopToLayoutDeviceScale(), CSSToLayoutDeviceScale(), 96 /* dpi */,
        Screen::IsPseudoDisplay::No, hal::ScreenOrientation::None, 0);
  }

  return do_AddRef(mScreenList[0]);
}

NS_IMETHODIMP
ScreenManager::GetPrimaryScreen(nsIScreen** aPrimaryScreen) {
  nsCOMPtr<nsIScreen> screen = GetPrimaryScreen();
  screen.forget(aPrimaryScreen);
  return NS_OK;
}

NS_IMETHODIMP
ScreenManager::GetTotalScreenPixels(int64_t* aTotalScreenPixels) {
  MOZ_ASSERT(aTotalScreenPixels);

  if (mScreenList.IsEmpty()) {
    MOZ_LOG(sScreenLog, LogLevel::Warning,
            ("No screen available. This can happen in xpcshell."));
    *aTotalScreenPixels = 0;
    return NS_OK;
  }

  int64_t pixels = 0;
  for (auto& screen : mScreenList) {
    int32_t x, y, width, height;
    x = y = width = height = 0;
    screen->GetRect(&x, &y, &width, &height);
    pixels += width * height;
  }

  *aTotalScreenPixels = pixels;
  return NS_OK;
}

}  // namespace mozilla::widget
