/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_REGION_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_REGION_H_

#include <vector>

#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// DesktopRegion represents a region of the screen or window.
//
// TODO(sergeyu): Current implementation just stores list of rectangles that may
// overlap. Optimize it.
class DesktopRegion {
 public:
  // Iterator that can be used to iterate over rectangles of a DesktopRegion.
  // The region must not be mutated while the iterator is used.
  class Iterator {
   public:
    explicit Iterator(const DesktopRegion& target);

    bool IsAtEnd() const;
    void Advance();

    const DesktopRect& rect() const { return *it_; }

   private:
    const DesktopRegion& region_;
    std::vector<DesktopRect>::const_iterator it_;
  };

  DesktopRegion();
  DesktopRegion(const DesktopRegion& other);
  ~DesktopRegion();

  bool is_empty() const { return rects_.empty(); }

  void Clear();
  void SetRect(const DesktopRect& rect);
  void AddRect(const DesktopRect& rect);
  void AddRegion(const DesktopRegion& region);

  // Clips the region by the |rect|.
  void IntersectWith(const DesktopRect& rect);

  // Adds (dx, dy) to the position of the region.
  void Translate(int32_t dx, int32_t dy);

  void Swap(DesktopRegion* region);

 private:
  typedef std::vector<DesktopRect> RectsList;
  RectsList rects_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_REGION_H_

