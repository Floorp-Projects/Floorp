/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Adapted from
// https://github.com/WebKit/WebKit/blob/ad340677c00a4b4b6a299c93a6e18cd073b3f4e9/Source/WTF/wtf/OSObjectPtr.h

#ifndef mozilla_DarwinObjectPtr_h
#define mozilla_DarwinObjectPtr_h

#include <os/object.h>
#include <utility>
#include "mozilla/Attributes.h"

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#  define AdoptDarwinObject AdoptDarwinObjectArc
#  define RetainDarwinObject RetainDarwinObjectArc
#  define ReleaseDarwinObject ReleaseDarwinObjectArc
#endif

namespace mozilla {

template <typename>
class DarwinObjectPtr;
template <typename T>
[[nodiscard]] DarwinObjectPtr<T> AdoptDarwinObject(T);

template <typename T>
static inline void RetainDarwinObject(T aPtr) {
#if !__has_feature(objc_arc)
  os_retain(aPtr);
#endif
}

template <typename T>
static inline void ReleaseDarwinObject(T aPtr) {
#if !__has_feature(objc_arc)
  os_release(aPtr);
#endif
}

template <typename T>
class DarwinObjectPtr {
 public:
  DarwinObjectPtr() : mPtr(nullptr) {}

  ~DarwinObjectPtr() {
    if (mPtr) {
      ReleaseDarwinObject(mPtr);
    }
  }

  T get() const { return mPtr; }

  explicit operator bool() const { return mPtr; }
  bool operator!() const { return !mPtr; }

  DarwinObjectPtr(const DarwinObjectPtr& aOther) : mPtr(aOther.mPtr) {
    if (mPtr) {
      RetainDarwinObject(mPtr);
    }
  }

  DarwinObjectPtr(DarwinObjectPtr&& aOther) : mPtr(std::move(aOther.mPtr)) {
    aOther.mPtr = nullptr;
  }

  MOZ_IMPLICIT DarwinObjectPtr(T aPtr) : mPtr(std::move(aPtr)) {
    if (mPtr) {
      RetainDarwinObject(mPtr);
    }
  }

  DarwinObjectPtr& operator=(const DarwinObjectPtr& aOther) {
    DarwinObjectPtr ptr = aOther;
    swap(ptr);
    return *this;
  }

  DarwinObjectPtr& operator=(DarwinObjectPtr&& aOther) {
    DarwinObjectPtr ptr = std::move(aOther);
    swap(ptr);
    return *this;
  }

  DarwinObjectPtr& operator=(std::nullptr_t) {
    if (mPtr) {
      ReleaseDarwinObject(mPtr);
    }
    mPtr = nullptr;
    return *this;
  }

  DarwinObjectPtr& operator=(T aOther) {
    DarwinObjectPtr ptr = std::move(aOther);
    swap(ptr);
    return *this;
  }

  void swap(DarwinObjectPtr& aOther) { std::swap(mPtr, aOther.mPtr); }

  [[nodiscard]] T forget() { return std::exchange(mPtr, nullptr); }

  friend DarwinObjectPtr AdoptDarwinObject<T>(T);

 private:
  struct AdoptDarwinObjectTag {};
  DarwinObjectPtr(AdoptDarwinObjectTag, T aPtr) : mPtr(std::move(aPtr)) {}

  T mPtr;
};

template <typename T>
inline DarwinObjectPtr<T> AdoptDarwinObject(T aPtr) {
  return DarwinObjectPtr<T>{typename DarwinObjectPtr<T>::AdoptDarwinObjectTag{},
                            std::move(aPtr)};
}

}  // namespace mozilla

#endif
