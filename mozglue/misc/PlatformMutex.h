/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PlatformMutex_h
#define mozilla_PlatformMutex_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"

namespace mozilla {

namespace detail {

class ConditionVariableImpl;

class MutexImpl
{
public:
  struct PlatformData;

  MFBT_API MutexImpl();
  MFBT_API ~MutexImpl();

  bool operator==(const MutexImpl& rhs) {
    return platformData_ == rhs.platformData_;
  }

protected:
  MFBT_API void lock();
  MFBT_API void unlock();

private:
  MutexImpl(const MutexImpl&) = delete;
  void operator=(const MutexImpl&) = delete;
  MutexImpl(MutexImpl&&) = delete;
  void operator=(MutexImpl&&) = delete;

  PlatformData* platformData();

// Linux and maybe other platforms define the storage size of pthread_mutex_t in
// bytes. However, we must define it as an array of void pointers to ensure
// proper alignment.
#if defined(__APPLE__) && defined(__MACH__) && defined(__i386__)
  void* platformData_[11];
#elif defined(__APPLE__) && defined(__MACH__) && defined(__amd64__)
  void* platformData_[8];
#elif defined(__linux__)
  void* platformData_[40 / sizeof(void*)];
#elif defined(_WIN32)
  void* platformData_[6];
#else
  void* platformData_[64 / sizeof(void*)];
#endif

  friend class mozilla::detail::ConditionVariableImpl;
};

} // namespace detail

} // namespace mozilla

#endif // mozilla_PlatformMutex_h
