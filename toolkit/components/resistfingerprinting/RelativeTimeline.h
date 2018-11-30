/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RelativeTimeline_h__
#define __RelativeTimeline_h__

namespace mozilla {

class RelativeTimeline {
 public:
  RelativeTimeline() : mRandomTimelineSeed(0) {}

  int64_t GetRandomTimelineSeed();

 private:
  uint64_t mRandomTimelineSeed;
};

}  // namespace mozilla

#endif /* __RelativeTimeline_h__ */