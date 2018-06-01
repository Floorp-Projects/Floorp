/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessScreenHelper.h"

#include "prenv.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {

/* static */ LayoutDeviceIntRect
HeadlessScreenHelper::GetScreenRect()
{
  char *ev = PR_GetEnv("MOZ_HEADLESS_WIDTH");
  int width = 1366;
  if (ev) {
    width = atoi(ev);
  }
  ev = PR_GetEnv("MOZ_HEADLESS_HEIGHT");
  int height = 768;
  if (ev) {
    height = atoi(ev);
  }
  return LayoutDeviceIntRect(0, 0, width, height);
}

HeadlessScreenHelper::HeadlessScreenHelper()
{
  AutoTArray<RefPtr<Screen>, 1> screenList;
  LayoutDeviceIntRect rect = GetScreenRect();
  RefPtr<Screen> ret = new Screen(rect, rect,
                                  24, 24,
                                  DesktopToLayoutDeviceScale(),
                                  CSSToLayoutDeviceScale(),
                                  96.0f);
  screenList.AppendElement(ret.forget());
  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.Refresh(std::move(screenList));
}

} // namespace widget
} // namespace mozilla
