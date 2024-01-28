/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidWidgetUtils.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Swizzle.h"

using namespace mozilla::gfx;

namespace mozilla::widget {

// static
already_AddRefed<DataSourceSurface>
AndroidWidgetUtils::GetDataSourceSurfaceForAndroidBitmap(
    gfx::SourceSurface* aSurface, const LayoutDeviceIntRect* aRect,
    uint32_t aStride) {
  RefPtr<DataSourceSurface> srcDataSurface = aSurface->GetDataSurface();
  if (NS_WARN_IF(!srcDataSurface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap sourceMap(srcDataSurface,
                                         DataSourceSurface::READ);

  RefPtr<DataSourceSurface> destDataSurface =
      gfx::Factory::CreateDataSourceSurfaceWithStride(
          aRect ? IntSize(aRect->width, aRect->height)
                : srcDataSurface->GetSize(),
          SurfaceFormat::R8G8B8A8, aStride ? aStride : sourceMap.GetStride());
  if (NS_WARN_IF(!destDataSurface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap destMap(destDataSurface,
                                       DataSourceSurface::READ_WRITE);

  SwizzleData(sourceMap.GetData(), sourceMap.GetStride(), aSurface->GetFormat(),
              destMap.GetData(), destMap.GetStride(), SurfaceFormat::R8G8B8A8,
              destDataSurface->GetSize());

  return destDataSurface.forget();
}

}  // namespace mozilla::widget
