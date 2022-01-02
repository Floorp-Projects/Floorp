/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LiveResizeListener_h
#define mozilla_LiveResizeListener_h

#include "nsISupportsImpl.h"

namespace mozilla {

class LiveResizeListener {
 public:
  virtual void LiveResizeStarted() = 0;
  virtual void LiveResizeStopped() = 0;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

 protected:
  virtual ~LiveResizeListener() = default;
};

}  // namespace mozilla

#endif  // mozilla_LiveResizeListener_h
