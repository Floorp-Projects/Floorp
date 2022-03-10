/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A wrapper that uses RAII to ensure that a class invariant is checked
// before and after any public function is called

#ifndef CHECKINVARIANTWRAPPER_H_
#define CHECKINVARIANTWRAPPER_H_

#include "mozilla/Attributes.h"
#include <utility>

namespace mozilla {

//
// Wraps an object of type T and allows access to its public API by
// deferencing it using the pointer syntax "->".
//
// Using that operator will return a temporary RAII object that
// calls a method named "CheckInvariant" in its constructor, calls the
// requested method, and then calls "CheckInvariant" again in its
// destructor.
//
// The only thing your class requires is a method with the following signature:
//
// void CheckInvariant() const;
//
template <typename T>
class CheckInvariantWrapper {
 public:
  class Wrapper {
   public:
    explicit Wrapper(T& aObject) : mObject(aObject) {
      mObject.CheckInvariant();
    }
    ~Wrapper() { mObject.CheckInvariant(); }

    T* operator->() { return &mObject; }

   private:
    T& mObject;
  };

  class ConstWrapper {
   public:
    explicit ConstWrapper(const T& aObject) : mObject(aObject) {
      mObject.CheckInvariant();
    }
    ~ConstWrapper() { mObject.CheckInvariant(); }

    const T* operator->() const { return &mObject; }

   private:
    const T& mObject;
  };

  CheckInvariantWrapper() = default;

  MOZ_IMPLICIT CheckInvariantWrapper(T aObject) : mObject(std::move(aObject)) {}

  template <typename... Args>
  explicit CheckInvariantWrapper(std::in_place_t, Args&&... args)
      : mObject(std::forward<Args>(args)...) {}

  const ConstWrapper operator->() const { return ConstWrapper(mObject); }

  Wrapper operator->() { return Wrapper(mObject); }

 private:
  T mObject;
};

}  // namespace mozilla

#endif  // CHECKINVARIANTWRAPPER_H_
