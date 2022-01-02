/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* functions for restoring saved values at the end of a C++ scope */

#ifndef mozilla_AutoRestore_h_
#define mozilla_AutoRestore_h_

#include "mozilla/Attributes.h"  // MOZ_STACK_CLASS

namespace mozilla {

/**
 * Save the current value of a variable and restore it when the object
 * goes out of scope.  For example:
 *   {
 *     AutoRestore<bool> savePainting(mIsPainting);
 *     mIsPainting = true;
 *
 *     // ... your code here ...
 *
 *     // mIsPainting is reset to its old value at the end of this block
 *   }
 */
template <class T>
class MOZ_RAII AutoRestore {
 private:
  T& mLocation;
  T mValue;

 public:
  explicit AutoRestore(T& aValue) : mLocation(aValue), mValue(aValue) {}
  ~AutoRestore() { mLocation = mValue; }
  T SavedValue() const { return mValue; }
};

}  // namespace mozilla

#endif /* !defined(mozilla_AutoRestore_h_) */
