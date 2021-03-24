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

  if (RefPtr<Screen> screen = MakePrimaryScreen()) {
    mScreens.InsertOrUpdate(0, std::move(screen));
  }

  ScreenManager& manager = ScreenManager::GetSingleton();
  manager.Refresh(ToTArray<AutoTArray<RefPtr<Screen>, 1>>(mScreens.Values()));
}

already_AddRefed<Screen> ScreenHelperAndroid::ScreenForId(uint32_t aScreenId) {
  RefPtr<Screen> screen = mScreens.Get(aScreenId);
  return screen.forget();
}
