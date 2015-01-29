/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TALK_APP_BASE_REFCOUNT_H_
#define TALK_APP_BASE_REFCOUNT_H_

#include <string.h>

#include "webrtc/base/criticalsection.h"

namespace rtc {

// Reference count interface.
class RefCountInterface {
 public:
  virtual int AddRef() = 0;
  virtual int Release() = 0;
 protected:
  virtual ~RefCountInterface() {}
};

template <class T>
class RefCountedObject : public T {
 public:
  RefCountedObject() : ref_count_(0) {
  }

  template<typename P>
  explicit RefCountedObject(P p) : T(p), ref_count_(0) {
  }

  template<typename P1, typename P2>
  RefCountedObject(P1 p1, P2 p2) : T(p1, p2), ref_count_(0) {
  }

  template<typename P1, typename P2, typename P3>
  RefCountedObject(P1 p1, P2 p2, P3 p3) : T(p1, p2, p3), ref_count_(0) {
  }

  template<typename P1, typename P2, typename P3, typename P4>
  RefCountedObject(P1 p1, P2 p2, P3 p3, P4 p4)
      : T(p1, p2, p3, p4), ref_count_(0) {
  }

  template<typename P1, typename P2, typename P3, typename P4, typename P5>
  RefCountedObject(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
      : T(p1, p2, p3, p4, p5), ref_count_(0) {
  }

  virtual int AddRef() {
    return rtc::AtomicOps::Increment(&ref_count_);
  }

  virtual int Release() {
    int count = rtc::AtomicOps::Decrement(&ref_count_);
    if (!count) {
      delete this;
    }
    return count;
  }

 protected:
  virtual ~RefCountedObject() {
  }

  int ref_count_;
};

}  // namespace rtc

#endif  // TALK_APP_BASE_REFCOUNT_H_
