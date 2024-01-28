/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidWidgetUtils_h__
#define mozilla_widget_AndroidWidgetUtils_h__

#include "Units.h"

namespace mozilla {

namespace gfx {
class SourceSurface;
class DataSourceSurface;
}  // namespace gfx

namespace widget {

class AndroidWidgetUtils final {
 public:
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;

  /**
   * Return Android's bitmap object compatible data surface.
   */
  static already_AddRefed<gfx::DataSourceSurface>
  GetDataSourceSurfaceForAndroidBitmap(
      gfx::SourceSurface* aSurface, const LayoutDeviceIntRect* aRect = nullptr,
      uint32_t aStride = 0);
};

}  // namespace widget
}  // namespace mozilla

#endif
