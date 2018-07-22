/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_InfallibleVector_h
#define mozilla_recordreplay_InfallibleVector_h

#include "mozilla/Vector.h"

namespace mozilla {
namespace recordreplay {

// This file declares two classes, InfallibleVector and StaticInfallibleVector,
// which behave like normal vectors except that all their operations are
// infallible: we will immediately crash if any operation on the underlying
// vector fails.
//
// StaticInfallibleVector is designed for use in static storage, and does not
// have a static constructor or destructor in release builds.

template<typename Outer, typename T, size_t MinInlineCapacity, class AllocPolicy>
class InfallibleVectorOperations
{
  typedef Vector<T, MinInlineCapacity, AllocPolicy> InnerVector;
  InnerVector& Vector() { return static_cast<Outer*>(this)->Vector(); }
  const InnerVector& Vector() const { return static_cast<const Outer*>(this)->Vector(); }

public:
  size_t length() const { return Vector().length(); }
  bool empty() const { return Vector().empty(); }
  T* begin() { return Vector().begin(); }
  const T* begin() const { return Vector().begin(); }
  T* end() { return Vector().end(); }
  const T* end() const { return Vector().end(); }
  T& operator[](size_t aIndex) { return Vector()[aIndex]; }
  const T& operator[](size_t aIndex) const { return Vector()[aIndex]; }
  T& back() { return Vector().back(); }
  const T& back() const { return Vector().back(); }
  void popBack() { Vector().popBack(); }
  T popCopy() { return Vector().popCopy(); }
  void erase(T* aT) { Vector().erase(aT); }
  void clear() { Vector().clear(); }

  void reserve(size_t aRequest) {
    if (!Vector().reserve(aRequest)) {
      MOZ_CRASH();
    }
  }

  void resize(size_t aNewLength) {
    if (!Vector().resize(aNewLength)) {
      MOZ_CRASH();
    }
  }

  template<typename U> void append(U&& aU) {
    if (!Vector().append(std::forward<U>(aU))) {
      MOZ_CRASH();
    }
  }

  template<typename U> void append(const U* aBegin, size_t aLength) {
    if (!Vector().append(aBegin, aLength)) {
      MOZ_CRASH();
    }
  }

  void appendN(const T& aT, size_t aN) {
    if (!Vector().appendN(aT, aN)) {
      MOZ_CRASH();
    }
  }

  template<typename... Args> void emplaceBack(Args&&... aArgs) {
    if (!Vector().emplaceBack(std::forward<Args>(aArgs)...)) {
      MOZ_CRASH();
    }
  }

  template<typename... Args> void infallibleEmplaceBack(Args&&... aArgs) {
    Vector().infallibleEmplaceBack(std::forward<Args>(aArgs)...);
  }

  template<typename U> void insert(T* aP, U&& aVal) {
    if (!Vector().insert(aP, std::forward<U>(aVal))) {
      MOZ_CRASH();
    }
  }
};

template<typename T,
         size_t MinInlineCapacity = 0,
         class AllocPolicy = MallocAllocPolicy>
class InfallibleVector
  : public InfallibleVectorOperations<InfallibleVector<T, MinInlineCapacity, AllocPolicy>,
                                      T, MinInlineCapacity, AllocPolicy>
{
  typedef Vector<T, MinInlineCapacity, AllocPolicy> InnerVector;
  InnerVector mVector;

public:
  InnerVector& Vector() { return mVector; }
  const InnerVector& Vector() const { return mVector; }
};

template<typename T,
         size_t MinInlineCapacity = 0,
         class AllocPolicy = MallocAllocPolicy>
class StaticInfallibleVector
  : public InfallibleVectorOperations<StaticInfallibleVector<T, MinInlineCapacity, AllocPolicy>,
                                      T, MinInlineCapacity, AllocPolicy>
{
  typedef Vector<T, MinInlineCapacity, AllocPolicy> InnerVector;
  mutable InnerVector* mVector;

  void EnsureVector() const {
    if (!mVector) {
      // N.B. This class can only be used with alloc policies that have a
      // default constructor.
      AllocPolicy policy;
      void* memory = policy.template pod_malloc<InnerVector>(1);
      MOZ_RELEASE_ASSERT(memory);
      mVector = new(memory) InnerVector();
    }
  }

public:
  // InfallibleVectors are allocated in static storage and should not have
  // constructors. Their memory will be initially zero.

  InnerVector& Vector() { EnsureVector(); return *mVector; }
  const InnerVector& Vector() const { EnsureVector(); return *mVector; }
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_InfallibleVector_h
