/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_BASE_WIN32TOOLHELP_H_
#define WEBRTC_BASE_WIN32TOOLHELP_H_

#if !defined(WEBRTC_WIN)
#error WEBRTC_WIN Only
#endif

#include "webrtc/base/win32.h"

// Should be included first, but that causes redefinitions.
#include <tlhelp32.h>

#include "webrtc/base/constructormagic.h"

namespace rtc {

// The toolhelp api used to enumerate processes and their modules
// on Windows is very repetetive and clunky to use. This little
// template wraps it to make it a little more programmer friendly.
//
// Traits: Traits type that adapts the enumerator to the corresponding
//         win32 toolhelp api. Each traits class need to:
//         - define the type of the enumerated data as a public symbol Type
//
//         - implement bool First(HANDLE, T*) normally calls a
//           Xxxx32First method in the toolhelp API. Ex Process32First(...)
//
//         - implement bool Next(HANDLE, T*) normally calls a
//           Xxxx32Next method in the toolhelp API. Ex Process32Next(...)
//
//         - implement bool CloseHandle(HANDLE)
//
template<typename Traits>
class ToolhelpEnumeratorBase {
 public:
  ToolhelpEnumeratorBase(HANDLE snapshot)
      : snapshot_(snapshot), broken_(false), first_(true) {

    // Clear out the Traits::Type structure instance.
    Zero(&current_);
  }

  virtual ~ToolhelpEnumeratorBase() {
    Close();
  }

  // Moves forward to the next object using the First and Next
  // pointers. If either First or Next ever indicates an failure
  // all subsequent calls to this method will fail; the enumerator
  // object is considered broken.
  bool Next() {
    if (!Valid()) {
      return false;
    }

    // Move the iteration forward.
    current_.dwSize = sizeof(typename Traits::Type);
    bool incr_ok = false;
    if (first_) {
      incr_ok = Traits::First(snapshot_, &current_);
      first_ = false;
    } else {
      incr_ok = Traits::Next(snapshot_, &current_);
    }

    if (!incr_ok) {
      Zero(&current_);
      broken_ = true;
    }

    return incr_ok;
  }

  const typename Traits::Type& current() const {
    return current_;
  }

  void Close() {
    if (snapshot_ != INVALID_HANDLE_VALUE) {
      Traits::CloseHandle(snapshot_);
      snapshot_ = INVALID_HANDLE_VALUE;
    }
  }

 private:
  // Checks the state of the snapshot handle.
  bool Valid() {
    return snapshot_ != INVALID_HANDLE_VALUE && !broken_;
  }

  static void Zero(typename Traits::Type* buff) {
    ZeroMemory(buff, sizeof(typename Traits::Type));
  }

  HANDLE snapshot_;
  typename Traits::Type current_;
  bool broken_;
  bool first_;
};

class ToolhelpTraits {
 public:
  static HANDLE CreateSnapshot(uint32 flags, uint32 process_id) {
    return CreateToolhelp32Snapshot(flags, process_id);
  }

  static bool CloseHandle(HANDLE handle) {
    return ::CloseHandle(handle) == TRUE;
  }
};

class ToolhelpProcessTraits : public ToolhelpTraits {
 public:
  typedef PROCESSENTRY32 Type;

  static bool First(HANDLE handle, Type* t) {
    return ::Process32First(handle, t) == TRUE;
  }

  static bool Next(HANDLE handle, Type* t) {
    return ::Process32Next(handle, t) == TRUE;
  }
};

class ProcessEnumerator : public ToolhelpEnumeratorBase<ToolhelpProcessTraits> {
 public:
  ProcessEnumerator()
      : ToolhelpEnumeratorBase(
           ToolhelpProcessTraits::CreateSnapshot(TH32CS_SNAPPROCESS, 0)) {
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ProcessEnumerator);
};

class ToolhelpModuleTraits : public ToolhelpTraits {
 public:
  typedef MODULEENTRY32 Type;

  static bool First(HANDLE handle, Type* t) {
    return ::Module32First(handle, t) == TRUE;
  }

  static bool Next(HANDLE handle, Type* t) {
    return ::Module32Next(handle, t) == TRUE;
  }
};

class ModuleEnumerator : public ToolhelpEnumeratorBase<ToolhelpModuleTraits> {
 public:
  explicit ModuleEnumerator(uint32 process_id)
      : ToolhelpEnumeratorBase(
            ToolhelpModuleTraits::CreateSnapshot(TH32CS_SNAPMODULE,
                                                 process_id)) {
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ModuleEnumerator);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_WIN32TOOLHELP_H_
