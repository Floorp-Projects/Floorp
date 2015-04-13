/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix_ScopedPtr_h
#define mozilla_pkix_ScopedPtr_h

namespace mozilla { namespace pkix {

// A subset polyfill of std::unique_ptr that does not support move construction
// or move assignment. This is used instead of std::unique_ptr because some
// important toolchains still don't provide std::unique_ptr, including in
// particular Android NDK projects with APP_STL=stlport_static or
// ALL_STL=stlport_shared.
template <typename T, void (&Destroyer)(T*)>
class ScopedPtr final
{
public:
  explicit ScopedPtr(T* value = nullptr) : mValue(value) { }

  ScopedPtr(const ScopedPtr&) = delete;

  ~ScopedPtr()
  {
    if (mValue) {
      Destroyer(mValue);
    }
  }

  void operator=(const ScopedPtr&) = delete;

  T& operator*() const { return *mValue; }
  T* operator->() const { return mValue; }

  explicit operator bool() const { return mValue; }

  T* get() const { return mValue; }

  T* release()
  {
    T* result = mValue;
    mValue = nullptr;
    return result;
  }

  void reset(T* newValue = nullptr)
  {
    // The C++ standard requires std::unique_ptr to destroy the old value
    // pointed to by mValue, if any, *after* assigning the new value to mValue.
    T* oldValue = mValue;
    mValue = newValue;
    if (oldValue) {
      Destroyer(oldValue);
    }
  }

private:
  T* mValue;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix_ScopedPtr_h
