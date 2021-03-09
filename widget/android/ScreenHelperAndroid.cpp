/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperAndroid.h"
#include "AndroidRect.h"
#include "nsThreadUtils.h"

#include <mozilla/jni/Refs.h>

#include "mozilla/Atomics.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "mozilla/java/ScreenManagerHelperNatives.h"
#include "mozilla/widget/ScreenManager.h"

using namespace mozilla;
using namespace mozilla::widget;

static ScreenHelperAndroid* gHelper = nullptr;

class ScreenHelperAndroid::ScreenHelperSupport final
    : public java::ScreenManagerHelper::Natives<ScreenHelperSupport> {
 public:
  typedef java::ScreenManagerHelper::Natives<ScreenHelperSupport> Base;

  static void RefreshScreenInfo() { gHelper->Refresh(); }

  static int32_t AddDisplay(int32_t aDisplayType, int32_t aWidth,
                            int32_t aHeight, float aDensity) {
    static Atomic<uint32_t> nextId;

    uint32_t screenId = ++nextId;
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(
            "ScreenHelperAndroid::ScreenHelperSupport::AddDisplay",
            [aDisplayType, aWidth, aHeight, aDensity, screenId] {
              MOZ_ASSERT(NS_IsMainThread());

              gHelper->AddScreen(
                  screenId, static_cast<DisplayType>(aDisplayType),
                  LayoutDeviceIntRect(0, 0, aWidth, aHeight), aDensity);
            })
            .take());
    return screenId;
  }

  static void RemoveDisplay(int32_t aScreenId) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(
            "ScreenHelperAndroid::ScreenHelperSupport::RemoveDisplay",
            [aScreenId] {
              MOZ_ASSERT(NS_IsMainThread());

              gHelper->RemoveScreen(aScreenId);
            })
            .take());
  }
};

static already_AddRefed<Screen> MakePrimaryScreen() {
  MOZ_ASSERT(XRE_IsParentProcess());
  java::sdk::Rect::LocalRef rect = java::GeckoAppShell::GetScreenSize();
  LayoutDeviceIntRect bounds = LayoutDeviceIntRect(
      rect->Left(), rect->Top(), rect->Width(), rect->Height());
  uint32_t depth = java::GeckoAppShell::GetScreenDepth();
  float density = java::GeckoAppShell::GetDensity();
  float dpi = java::GeckoAppShell::GetDpi();
  RefPtr<Screen> screen = new Screen(bounds, bounds, depth, depth,
                                     DesktopToLayoutDeviceScale(density),
                                     CSSToLayoutDeviceScale(1.0f), dpi);
  return screen.forget();
}

ScreenHelperAndroid::ScreenHelperAndroid() {
  MOZ_ASSERT(!gHelper);
  gHelper = this;

  ScreenHelperSupport::Base::Init();

  Refresh();
}

ScreenHelperAndroid::~ScreenHelperAndroid() { gHelper = nullptr; }

/* static */
ScreenHelperAndroid* ScreenHelperAndroid::GetSingleton() { return gHelper; }

void ScreenHelperAndroid::Refresh() {
  mScreens.Remove(0);

  AutoTArray<RefPtr<Screen>, 1> screenList;
  if (RefPtr<Screen> screen = MakePrimaryScreen()) {
    mScreens.InsertOrUpdate(0, std::move(screen));
  }

  for (auto iter = mScreens.ConstIter(); !iter.Done(); iter.Next()) {
    screenList.AppendElement(iter.Data());
  }

  ScreenManager& manager = ScreenManager::GetSingleton();
  manager.Refresh(std::move(screenList));
}

void ScreenHelperAndroid::AddScreen(uint32_t aScreenId,
                                    DisplayType aDisplayType,
                                    LayoutDeviceIntRect aRect, float aDensity) {
  MOZ_ASSERT(aScreenId > 0);
  MOZ_ASSERT(!mScreens.Get(aScreenId, nullptr));

  mScreens.InsertOrUpdate(
      aScreenId, MakeRefPtr<Screen>(aRect, aRect, 24, 24,
                                    DesktopToLayoutDeviceScale(aDensity),
                                    CSSToLayoutDeviceScale(1.0f), 160.0f));
  Refresh();
}

void ScreenHelperAndroid::RemoveScreen(uint32_t aScreenId) {
  mScreens.Remove(aScreenId);
  Refresh();
}

already_AddRefed<Screen> ScreenHelperAndroid::ScreenForId(uint32_t aScreenId) {
  RefPtr<Screen> screen = mScreens.Get(aScreenId);
  return screen.forget();
}
