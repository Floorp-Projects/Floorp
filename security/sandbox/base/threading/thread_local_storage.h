// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_LOCAL_STORAGE_H_
#define BASE_THREADING_THREAD_LOCAL_STORAGE_H_

#include "base/base_export.h"
#include "base/basictypes.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

namespace base {

// Wrapper for thread local storage.  This class doesn't do much except provide
// an API for portability.
class BASE_EXPORT ThreadLocalStorage {
 public:

  // Prototype for the TLS destructor function, which can be optionally used to
  // cleanup thread local storage on thread exit.  'value' is the data that is
  // stored in thread local storage.
  typedef void (*TLSDestructorFunc)(void* value);

  // StaticSlot uses its own struct initializer-list style static
  // initialization, as base's LINKER_INITIALIZED requires a constructor and on
  // some compilers (notably gcc 4.4) this still ends up needing runtime
  // initialization.
  #define TLS_INITIALIZER {0}

  // A key representing one value stored in TLS.
  // Initialize like
  //   ThreadLocalStorage::StaticSlot my_slot = TLS_INITIALIZER;
  // If you're not using a static variable, use the convenience class
  // ThreadLocalStorage::Slot (below) instead.
  struct BASE_EXPORT StaticSlot {
    // Set up the TLS slot.  Called by the constructor.
    // 'destructor' is a pointer to a function to perform per-thread cleanup of
    // this object.  If set to NULL, no cleanup is done for this TLS slot.
    // Returns false on error.
    bool Initialize(TLSDestructorFunc destructor);

    // Free a previously allocated TLS 'slot'.
    // If a destructor was set for this slot, removes
    // the destructor so that remaining threads exiting
    // will not free data.
    void Free();

    // Get the thread-local value stored in slot 'slot'.
    // Values are guaranteed to initially be zero.
    void* Get() const;

    // Set the thread-local value stored in slot 'slot' to
    // value 'value'.
    void Set(void* value);

    bool initialized() const { return initialized_; }

    // The internals of this struct should be considered private.
    bool initialized_;
#if defined(OS_WIN)
    int slot_;
#elif defined(OS_POSIX)
    pthread_key_t key_;
#endif

  };

  // A convenience wrapper around StaticSlot with a constructor. Can be used
  // as a member variable.
  class BASE_EXPORT Slot : public StaticSlot {
   public:
    // Calls StaticSlot::Initialize().
    explicit Slot(TLSDestructorFunc destructor = NULL);

   private:
    using StaticSlot::initialized_;
#if defined(OS_WIN)
    using StaticSlot::slot_;
#elif defined(OS_POSIX)
    using StaticSlot::key_;
#endif
    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorage);
};

}  // namespace base

#endif  // BASE_THREADING_THREAD_LOCAL_STORAGE_H_
