/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_region.h"

namespace webrtc {

DesktopRegion::DesktopRegion() {}

DesktopRegion::DesktopRegion(const DesktopRegion& other)
    : rects_(other.rects_) {
}

DesktopRegion::~DesktopRegion() {}

void DesktopRegion::Clear() {
  rects_.clear();
}

void DesktopRegion::SetRect(const DesktopRect& rect) {
  Clear();
  AddRect(rect);
}

void DesktopRegion::AddRect(const DesktopRect& rect) {
  if (!rect.is_empty())
    rects_.push_back(rect);
}

void DesktopRegion::AddRegion(const DesktopRegion& region) {
  for (Iterator it(region); !it.IsAtEnd(); it.Advance()) {
    AddRect(it.rect());
  }
}

void DesktopRegion::IntersectWith(const DesktopRect& rect) {
  bool remove_empty_rects = false;
  for (RectsList::iterator it = rects_.begin(); it != rects_.end(); ++it) {
    it->IntersectWith(rect);
    remove_empty_rects = remove_empty_rects | it->is_empty();
  }
  if (remove_empty_rects) {
    RectsList new_rects(rects_.size());
    for (RectsList::iterator it = rects_.begin(); it != rects_.end(); ++it) {
      if (!it->is_empty())
        new_rects.push_back(*it);
    }
    rects_.swap(new_rects);
  }
}

void DesktopRegion::Translate(int32_t dx, int32_t dy) {
  for (RectsList::iterator it = rects_.begin(); it != rects_.end(); ++it) {
    it->Translate(dx, dy);
  }
}

void DesktopRegion::Swap(DesktopRegion* region) {
  rects_.swap(region->rects_);
}

DesktopRegion::Iterator::Iterator(const DesktopRegion& region)
    : region_(region),
      it_(region.rects_.begin()) {
}

bool DesktopRegion::Iterator::IsAtEnd() const {
  return it_ == region_.rects_.end();
}

void DesktopRegion::Iterator::Advance() {
  ++it_;
}

}  // namespace webrtc

