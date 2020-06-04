/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CombinedStacks_h__
#define CombinedStacks_h__

#include <vector>

#include "ipc/IPCMessageUtils.h"
#include "ProcessedStack.h"

class JSObject;
struct JSContext;

namespace mozilla {
namespace Telemetry {

/**
 * This class is conceptually a list of ProcessedStack objects, but it
 * represents them more efficiently by keeping a single global list of modules.
 */
class CombinedStacks {
 public:
  explicit CombinedStacks();
  explicit CombinedStacks(size_t aMaxStacksCount);

  CombinedStacks(CombinedStacks&&) = default;
  CombinedStacks& operator=(CombinedStacks&&) = default;

  void Swap(CombinedStacks& aOther);

  typedef std::vector<Telemetry::ProcessedStack::Frame> Stack;
  const Telemetry::ProcessedStack::Module& GetModule(unsigned aIndex) const;
  size_t GetModuleCount() const;
  const Stack& GetStack(unsigned aIndex) const;
  size_t AddStack(const Telemetry::ProcessedStack& aStack);
  size_t GetStackCount() const;
  size_t SizeOfExcludingThis() const;
  void RemoveStack(unsigned aIndex);

#if defined(MOZ_GECKO_PROFILER)
  /** Clears the contents of vectors and resets the index. */
  void Clear();
#endif

 private:
  std::vector<Telemetry::ProcessedStack::Module> mModules;
  // A circular buffer to hold the stacks.
  std::vector<Stack> mStacks;
  // The index of the next buffer element to write to in mStacks.
  size_t mNextIndex;
  // The maximum number of stacks to keep in the CombinedStacks object.
  size_t mMaxStacksCount;

  friend struct ::IPC::ParamTraits<CombinedStacks>;
};

/**
 * Creates a JSON representation of given combined stacks object.
 */
JSObject* CreateJSStackObject(JSContext* cx, const CombinedStacks& stacks);

}  // namespace Telemetry
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::Telemetry::CombinedStacks> {
  typedef mozilla::Telemetry::CombinedStacks paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mModules);
    WriteParam(aMsg, aParam.mStacks);
    WriteParam(aMsg, aParam.mNextIndex);
    WriteParam(aMsg, aParam.mMaxStacksCount);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mModules)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mStacks)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mNextIndex)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mMaxStacksCount)) {
      return false;
    }

    return true;
  }
};

}  // namespace IPC

#endif  // CombinedStacks_h__
