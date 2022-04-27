/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_KEEP_REF_UNTIL_DONE_H_
#define RTC_BASE_KEEP_REF_UNTIL_DONE_H_

#include "api/scoped_refptr.h"
#include "rtc_base/callback.h"

namespace rtc {

// KeepRefUntilDone keeps a reference to |object| until the returned
// callback goes out of scope. If the returned callback is copied, the
// reference will be released when the last callback goes out of scope.
template <class ObjectT>
static inline Callback0<void> KeepRefUntilDone(ObjectT* object) {
  scoped_refptr<ObjectT> p(object);
  return [p] {};
}

template <class ObjectT>
static inline Callback0<void> KeepRefUntilDone(
    const scoped_refptr<ObjectT>& object) {
  return [object] {};
}

}  // namespace rtc

#endif  // RTC_BASE_KEEP_REF_UNTIL_DONE_H_
