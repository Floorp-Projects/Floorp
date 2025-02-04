/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

// static
std::unique_ptr<DesktopFrameIOSurface> DesktopFrameIOSurface::Wrap(
    rtc::ScopedCFTypeRef<IOSurfaceRef> io_surface, CGRect rect) {
  if (!io_surface) {
    return nullptr;
  }

  IOSurfaceIncrementUseCount(io_surface.get());
  IOReturn status = IOSurfaceLock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
  if (status != kIOReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock the IOSurface with status " << status;
    IOSurfaceDecrementUseCount(io_surface.get());
    return nullptr;
  }

  // Verify that the image has 32-bit depth.
  int bytes_per_pixel = IOSurfaceGetBytesPerElement(io_surface.get());
  if (bytes_per_pixel != DesktopFrame::kBytesPerPixel) {
    RTC_LOG(LS_ERROR) << "CGDisplayStream handler returned IOSurface with " << (8 * bytes_per_pixel)
                      << " bits per pixel. Only 32-bit depth is supported.";
    IOSurfaceUnlock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
    IOSurfaceDecrementUseCount(io_surface.get());
    return nullptr;
  }

  size_t surfaceWidth = IOSurfaceGetWidth(io_surface.get());
  size_t surfaceHeight = IOSurfaceGetHeight(io_surface.get());
  uint8_t* data = static_cast<uint8_t*>(IOSurfaceGetBaseAddress(io_surface.get()));
  size_t offset = 0;
  size_t width = surfaceWidth;
  size_t height = surfaceHeight;
  size_t offsetColumns = 0;
  size_t offsetRows = 0;
  int32_t stride = IOSurfaceGetBytesPerRow(io_surface.get());
  if (rect.size.width > 0 && rect.size.height > 0) {
    width = std::floor(rect.size.width);
    height = std::floor(rect.size.height);
    offsetColumns = std::ceil(rect.origin.x);
    offsetRows = std::ceil(rect.origin.y);
    RTC_CHECK_GE(surfaceWidth,  offsetColumns + width);
    RTC_CHECK_GE(surfaceHeight, offsetRows + height);
    offset = stride * offsetRows + bytes_per_pixel * offsetColumns;
  }

  RTC_LOG(LS_VERBOSE) << "DesktopFrameIOSurface wrapping IOSurface with size " << surfaceWidth << "x"
      << surfaceHeight << ". Cropping to (" << offsetColumns << "," << offsetRows << "; "
      << width << "x" << height << "). Stride=" << stride / bytes_per_pixel
      << ", buffer-offset-px=" << offset / bytes_per_pixel << ", buffer-offset-bytes=" << offset;

  return std::unique_ptr<DesktopFrameIOSurface>(new DesktopFrameIOSurface(io_surface, data + offset, width, height, stride));
}

DesktopFrameIOSurface::DesktopFrameIOSurface(
  rtc::ScopedCFTypeRef<IOSurfaceRef> io_surface, uint8_t* data, int32_t width, int32_t height, int32_t stride)
    : DesktopFrame(
          DesktopSize(width, height),
          stride,
          data,
          nullptr),
      io_surface_(io_surface) {
  RTC_DCHECK(io_surface_);
}

DesktopFrameIOSurface::~DesktopFrameIOSurface() {
  IOSurfaceUnlock(io_surface_.get(), kIOSurfaceLockReadOnly, nullptr);
  IOSurfaceDecrementUseCount(io_surface_.get());
}

}  // namespace webrtc
