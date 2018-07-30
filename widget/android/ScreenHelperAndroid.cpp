/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperAndroid.h"
#include "AndroidRect.h"
#include "GeneratedJNINatives.h"
#include "nsThreadUtils.h"

#include <mozilla/jni/Refs.h>

#include "mozilla/widget/ScreenManager.h"

using namespace mozilla;
using namespace mozilla::java;
using namespace mozilla::widget;

static already_AddRefed<Screen>
MakePrimaryScreen() {
    MOZ_ASSERT(XRE_IsParentProcess());
    if (!jni::IsAvailable()) {
        return nullptr;
    }

    java::sdk::Rect::LocalRef rect = GeckoAppShell::GetScreenSize();
    LayoutDeviceIntRect bounds = LayoutDeviceIntRect(rect->Left(), rect->Top(),
                                                     rect->Width(), rect->Height());
    uint32_t depth = GeckoAppShell::GetScreenDepth();
    float density = GeckoAppShell::GetDensity();
    float dpi = GeckoAppShell::GetDpi();
    RefPtr<Screen> screen = new Screen(bounds, bounds, depth, depth,
                      DesktopToLayoutDeviceScale(density),
                      CSSToLayoutDeviceScale(1.0f),
                      dpi);
    return screen.forget();
}

void
ScreenHelperAndroid::Refresh() {
    AutoTArray<RefPtr<Screen>, 1> screenList;
    RefPtr<Screen> screen = MakePrimaryScreen();
    if (screen) {
        screenList.AppendElement(screen);
    }

    ScreenManager& manager = ScreenManager::GetSingleton();
    manager.Refresh(std::move(screenList));
}
