/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INCLUDE_STATIC_INSTANCE_H_
#define WEBRTC_SYSTEM_WRAPPERS_INCLUDE_STATIC_INSTANCE_H_

#include <assert.h>

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_GONK)
#define OS_LINUX
#endif
#include "base/singleton.h"

namespace webrtc {

enum CountOperation {
  kRelease,
  kAddRef,
  kAddRefNoCreate
};
enum CreateOperation {
  kInstanceExists,
  kCreate,
  kDestroy
};

template <class T>
// Construct On First Use idiom. Avoids
// "static initialization order fiasco".
static T* GetStaticInstance(CountOperation count_operation) {
  // Simple solution since we don't use this for large objects anymore
  return Singleton<T>::get();
}

}  // namspace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INCLUDE_STATIC_INSTANCE_H_
