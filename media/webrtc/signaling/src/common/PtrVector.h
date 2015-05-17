/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PtrVector_h
#define PtrVector_h

#include <mozilla/Move.h>
#include <vector>

namespace mozilla
{

// Trivial wrapper class around a vector of ptrs.
// TODO: Remove this once our buildconfig allows us to put unique_ptr in stl
// containers, and just use std::vector<unique_ptr<T>> instead.
template <class T> class PtrVector
{
public:
  PtrVector() = default;
  PtrVector(const PtrVector&) = delete;
  PtrVector(PtrVector&& aOther)
    : values(Move(aOther.values))
  {}
  PtrVector& operator=(const PtrVector&) = delete;
  PtrVector& operator=(PtrVector&& aOther)
  {
    Swap(values, aOther.values);
    return *this;
  }

  ~PtrVector()
  {
    for (T* value : values) { delete value; }
  }

  std::vector<T*> values;
};

} // namespace mozilla

#endif // PtrVector_h

