// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HANDLE_H_
#define BASE_WIN_SCOPED_HANDLE_H_

#include <windows.h>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/move.h"

namespace base {
namespace win {

// TODO(rvargas): remove this with the rest of the verifier.
#if defined(COMPILER_MSVC)
// MSDN says to #include <intrin.h>, but that breaks the VS2005 build.
extern "C" {
  void* _ReturnAddress();
}
#define BASE_WIN_GET_CALLER _ReturnAddress()
#elif defined(COMPILER_GCC)
#define BASE_WIN_GET_CALLER __builtin_extract_return_addr(\\
    __builtin_return_address(0))
#endif

// Generic wrapper for raw handles that takes care of closing handles
// automatically. The class interface follows the style of
// the ScopedStdioHandle class with a few additions:
//   - IsValid() method can tolerate multiple invalid handle values such as NULL
//     and INVALID_HANDLE_VALUE (-1) for Win32 handles.
//   - Receive() method allows to receive a handle value from a function that
//     takes a raw handle pointer only.
template <class Traits, class Verifier>
class GenericScopedHandle {
  MOVE_ONLY_TYPE_FOR_CPP_03(GenericScopedHandle, RValue)

 public:
  typedef typename Traits::Handle Handle;

  // Helper object to contain the effect of Receive() to the function that needs
  // a pointer, and allow proper tracking of the handle.
  class Receiver {
   public:
    explicit Receiver(GenericScopedHandle* owner)
        : handle_(Traits::NullHandle()),
          owner_(owner) {}
    ~Receiver() { owner_->Set(handle_); }

    operator Handle*() { return &handle_; }

   private:
    Handle handle_;
    GenericScopedHandle* owner_;
  };

  GenericScopedHandle() : handle_(Traits::NullHandle()) {}

  explicit GenericScopedHandle(Handle handle) : handle_(Traits::NullHandle()) {
    Set(handle);
  }

  // Move constructor for C++03 move emulation of this type.
  GenericScopedHandle(RValue other) : handle_(Traits::NullHandle()) {
    Set(other.object->Take());
  }

  ~GenericScopedHandle() {
    Close();
  }

  bool IsValid() const {
    return Traits::IsHandleValid(handle_);
  }

  // Move operator= for C++03 move emulation of this type.
  GenericScopedHandle& operator=(RValue other) {
    if (this != other.object) {
      Set(other.object->Take());
    }
    return *this;
  }

  void Set(Handle handle) {
    if (handle_ != handle) {
      Close();

      if (Traits::IsHandleValid(handle)) {
        handle_ = handle;
        Verifier::StartTracking(handle, this, BASE_WIN_GET_CALLER,
                                tracked_objects::GetProgramCounter());
      }
    }
  }

  Handle Get() const {
    return handle_;
  }

  operator Handle() const {
    return handle_;
  }

  // This method is intended to be used with functions that require a pointer to
  // a destination handle, like so:
  //    void CreateRequiredHandle(Handle* out_handle);
  //    ScopedHandle a;
  //    CreateRequiredHandle(a.Receive());
  Receiver Receive() {
    DCHECK(!Traits::IsHandleValid(handle_)) << "Handle must be NULL";
    return Receiver(this);
  }

  // Transfers ownership away from this object.
  Handle Take() {
    Handle temp = handle_;
    handle_ = Traits::NullHandle();
    if (Traits::IsHandleValid(temp)) {
      Verifier::StopTracking(temp, this, BASE_WIN_GET_CALLER,
                             tracked_objects::GetProgramCounter());
    }
    return temp;
  }

  // Explicitly closes the owned handle.
  void Close() {
    if (Traits::IsHandleValid(handle_)) {
      Verifier::StopTracking(handle_, this, BASE_WIN_GET_CALLER,
                             tracked_objects::GetProgramCounter());

      if (!Traits::CloseHandle(handle_))
        CHECK(false);

      handle_ = Traits::NullHandle();
    }
  }

 private:
  Handle handle_;
};

#undef BASE_WIN_GET_CALLER

// The traits class for Win32 handles that can be closed via CloseHandle() API.
class HandleTraits {
 public:
  typedef HANDLE Handle;

  // Closes the handle.
  static bool CloseHandle(HANDLE handle) {
    return ::CloseHandle(handle) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(HANDLE handle) {
    return handle != NULL && handle != INVALID_HANDLE_VALUE;
  }

  // Returns NULL handle value.
  static HANDLE NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HandleTraits);
};

// Do-nothing verifier.
class DummyVerifierTraits {
 public:
  typedef HANDLE Handle;

  static void StartTracking(HANDLE handle, const void* owner,
                            const void* pc1, const void* pc2) {}
  static void StopTracking(HANDLE handle, const void* owner,
                           const void* pc1, const void* pc2) {}

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DummyVerifierTraits);
};

// Performs actual run-time tracking.
class BASE_EXPORT VerifierTraits {
 public:
  typedef HANDLE Handle;

  static void StartTracking(HANDLE handle, const void* owner,
                            const void* pc1, const void* pc2);
  static void StopTracking(HANDLE handle, const void* owner,
                           const void* pc1, const void* pc2);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(VerifierTraits);
};

typedef GenericScopedHandle<HandleTraits, VerifierTraits> ScopedHandle;

}  // namespace win
}  // namespace base

#endif  // BASE_SCOPED_HANDLE_WIN_H_
