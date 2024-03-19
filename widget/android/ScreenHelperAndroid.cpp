/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperAndroid.h"
#include "AndroidRect.h"
#include "nsThreadUtils.h"

#include <mozilla/jni/Refs.h>

#include "AndroidVsync.h"
#include "mozilla/Atomics.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "mozilla/java/ScreenManagerHelperNatives.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsXULAppAPI.h"

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
  bool isHDR = false;  // Bug 1884960: report this accurately
  auto orientation =
      hal::ScreenOrientation(java::GeckoAppShell::GetScreenOrientation());
  uint16_t angle = java::GeckoAppShell::GetScreenAngle();
  float refreshRate = java::GeckoAppShell::GetScreenRefreshRate();
  return MakeAndAddRef<Screen>(
      bounds, bounds, depth, depth, refreshRate,
      DesktopToLayoutDeviceScale(density), CSSToLayoutDeviceScale(1.0f), dpi,
      Screen::IsPseudoDisplay::No, Screen::IsHDR(isHDR), orientation, angle);
}

ScreenHelperAndroid::ScreenHelperAndroid() {
  MOZ_ASSERT(!gHelper);
  gHelper = this;

  ScreenHelperSupport::Base::Init();

  Refresh();
}

ScreenHelperAndroid::~ScreenHelperAndroid() { gHelper = nullptr; }

void ScreenHelperAndroid::Refresh() {
  AutoTArray<RefPtr<Screen>, 1> screens;
  screens.AppendElement(MakePrimaryScreen());
  ScreenManager::Refresh(std::move(screens));

  if (RefPtr<AndroidVsync> vsync = AndroidVsync::GetInstance()) {
    vsync->OnMaybeUpdateRefreshRate();
  }
}
