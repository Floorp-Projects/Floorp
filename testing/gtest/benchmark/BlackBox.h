// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GTEST_BLACKBOX_H
#define GTEST_BLACKBOX_H

#include "mozilla/Attributes.h"
#if defined(_MSC_VER)
#include <intrin.h>
#endif // _MSC_VER

namespace mozilla {

#if defined(_MSC_VER)

char volatile* UseCharPointer(char volatile*);

MOZ_ALWAYS_INLINE_EVEN_DEBUG void* BlackBoxVoidPtr(void* aPtr) {
  aPtr = const_cast<char*>(UseCharPointer(reinterpret_cast<char*>(aPtr)));
  _ReadWriteBarrier();
  return aPtr;
}

#else

// See: https://youtu.be/nXaxk27zwlk?t=2441
MOZ_ALWAYS_INLINE_EVEN_DEBUG void* BlackBoxVoidPtr(void* aPtr) {
  // "g" is what we want here, but the comment in the Google
  // benchmark code suggests that GCC likes "i,r,m" better.
  // However, on Mozilla try server i,r,m breaks GCC but g
  // works in GCC, so using g for both clang and GCC.
  // godbolt.org indicates that g works already in GCC 4.9,
  // which is the oldest GCC we support at the time of this
  // code landing. godbolt.org suggests that this clearly
  // works is LLVM 5, but it's unclear if this inhibits
  // all relevant optimizations properly on earlier LLVM.
  asm volatile("" : "+g"(aPtr) : "g"(aPtr) : "memory");
  return aPtr;
}

#endif // _MSC_VER

template<class T>
MOZ_ALWAYS_INLINE_EVEN_DEBUG T* BlackBox(T* aPtr) {
  return static_cast<T*>(BlackBoxVoidPtr(aPtr));
}

} // namespace mozilla

#endif // GTEST_BLACKBOX_H
