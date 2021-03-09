/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: ts=4 sw=2 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScreenHelperAndroid_h___
#define ScreenHelperAndroid_h___

#include "mozilla/widget/ScreenManager.h"
#include "nsTHashMap.h"

namespace mozilla {
namespace widget {

class ScreenHelperAndroid final : public ScreenManager::Helper {
 public:
  class ScreenHelperSupport;

  ScreenHelperAndroid();
  ~ScreenHelperAndroid();

  static ScreenHelperAndroid* GetSingleton();

  void Refresh();

  void AddScreen(uint32_t aScreenId, DisplayType aDisplayType,
                 LayoutDeviceIntRect aRect = LayoutDeviceIntRect(),
                 float aDensity = 1.0f);
  void RemoveScreen(uint32_t aId);
  already_AddRefed<Screen> ScreenForId(uint32_t aScreenId);

 private:
  nsTHashMap<uint32_t, RefPtr<Screen>> mScreens;
};

}  // namespace widget
}  // namespace mozilla

#endif /* ScreenHelperAndroid_h___ */
