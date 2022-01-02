/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_WindowOcclusionState_h
#define widget_WindowOcclusionState_h

namespace mozilla {
namespace widget {

// nsWindow's window occlusion state. On Windows, it is tracked by
// WinWindowOcclusionTracker.
enum class OcclusionState {
  // The window's occlusion state isn't tracked (NotifyOcclusionState()) or
  // hasn't been computed yet.
  UNKNOWN = 0,
  // The window IsWindowVisibleAndFullyOpaque() [1] and:
  // - Its bounds aren't completely covered by fully opaque windows [2]
  VISIBLE = 1,
  // The window IsWindowVisibleAndFullyOpaque() [1], but they all:
  // - Have bounds completely covered by fully opaque windows [2]
  OCCLUDED = 2,
  // The window is not IsWindowVisibleAndFullyOpaque() [1].
  HIDDEN = 3,

  kMaxValue = HIDDEN,
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_WindowOcclusionState_h
