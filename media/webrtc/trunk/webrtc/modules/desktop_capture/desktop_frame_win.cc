/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_frame_win.h"

#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

DesktopFrameWin::DesktopFrameWin(DesktopSize size,
                                 int stride,
                                 uint8_t* data,
                                 SharedMemory* shared_memory,
                                 HBITMAP bitmap)
    : DesktopFrame(size, stride, data, shared_memory),
      bitmap_(bitmap),
      owned_shared_memory_(shared_memory_) {
}

DesktopFrameWin::~DesktopFrameWin() {
  DeleteObject(bitmap_);
}

// static
DesktopFrameWin* DesktopFrameWin::Create(DesktopSize size,
                                         SharedMemory* shared_memory,
                                         HDC hdc) {
  int bytes_per_row = size.width() * kBytesPerPixel;

  // Describe a device independent bitmap (DIB) that is the size of the desktop.
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biHeight = -size.height();
  bmi.bmiHeader.biWidth = size.width();
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = DesktopFrameWin::kBytesPerPixel * 8;
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biSizeImage = bytes_per_row * size.height();

  HANDLE section_handle = NULL;
  if (shared_memory)
    section_handle = shared_memory->handle();
  void* data = NULL;
  HBITMAP bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &data,
                                    section_handle, 0);
  if (!bitmap) {
    LOG(LS_WARNING) << "Failed to allocate new window frame " << GetLastError();
    delete shared_memory;
    return NULL;
  }

  return new DesktopFrameWin(size, bytes_per_row,
                             reinterpret_cast<uint8_t*>(data),
                             shared_memory, bitmap);
}

}  // namespace webrtc
