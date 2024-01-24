/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProcessedStack_h__
#define ProcessedStack_h__

#include <vector>

#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/Vector.h"
#include "nsStringFwd.h"
#include "shared-libraries.h"

namespace mozilla {
namespace Telemetry {

// This class represents a stack trace and the modules referenced in that trace.
// It is designed to be easy to read and write to disk or network and doesn't
// include any logic on how to collect or read the information it stores.
class ProcessedStack {
 public:
  ProcessedStack();
  size_t GetStackSize() const;
  size_t GetNumModules() const;

  struct Frame {
    // The offset of this program counter in its module or an absolute pc.
    uintptr_t mOffset;
    // The index to pass to GetModule to get the module this program counter
    // was in.
    uint16_t mModIndex;
  };
  struct Module {
    // The file name, /foo/bar/libxul.so for example.
    // It can contain unicode characters.
    nsString mName;
    nsCString mBreakpadId;

    bool operator==(const Module& other) const;
  };

  const Frame& GetFrame(unsigned aIndex) const;
  void AddFrame(const Frame& aFrame);
  const Module& GetModule(unsigned aIndex) const;
  void AddModule(const Module& aFrame);

  void Clear();

 private:
  std::vector<Module> mModules;
  std::vector<Frame> mStack;
};

// Get the current list of loaded modules, filter and pair it to the provided
// stack. We let the caller collect the stack since different callers have
// different needs (current thread X main thread, stopping the thread, etc).
ProcessedStack GetStackAndModules(const std::vector<uintptr_t>& aPCs);

// This class optimizes repeated calls to GetStackAndModules.
class BatchProcessedStackGenerator {
 public:
  BatchProcessedStackGenerator();
  ProcessedStack GetStackAndModules(const std::vector<uintptr_t>& aPCs);

  template <typename AllocatorPolicy>
  ProcessedStack GetStackAndModules(
      const Vector<void*, 0, AllocatorPolicy>& aPCs) {
    return GetStackAndModules(reinterpret_cast<const uintptr_t*>(aPCs.begin()),
                              reinterpret_cast<const uintptr_t*>(aPCs.end()));
  }

 private:
  ProcessedStack GetStackAndModules(const uintptr_t* aBegin,
                                    const uintptr_t* aEnd);
#if defined(MOZ_GECKO_PROFILER)
  SharedLibraryInfo mSortedRawModules;
#endif
};

}  // namespace Telemetry
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::Telemetry::ProcessedStack::Module> {
  typedef mozilla::Telemetry::ProcessedStack::Module paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mName);
    WriteParam(aWriter, aParam.mBreakpadId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mName)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mBreakpadId)) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::Telemetry::ProcessedStack::Frame> {
  typedef mozilla::Telemetry::ProcessedStack::Frame paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, aParam.mModIndex);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mOffset)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mModIndex)) {
      return false;
    }

    return true;
  }
};

}  // namespace IPC

#endif  // ProcessedStack_h__
