/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessedStack.h"

namespace {

struct StackFrame {
  uintptr_t mPC;    // The program counter at this position in the call stack.
  uint16_t mIndex;  // The number of this frame in the call stack.
  uint16_t mModIndex;  // The index of module that has this program counter.
};

#ifdef MOZ_GECKO_PROFILER
static bool CompareByPC(const StackFrame& a, const StackFrame& b) {
  return a.mPC < b.mPC;
}

static bool CompareByIndex(const StackFrame& a, const StackFrame& b) {
  return a.mIndex < b.mIndex;
}
#endif

}  // namespace

namespace mozilla::Telemetry {

const size_t kMaxChromeStackDepth = 50;

ProcessedStack::ProcessedStack() = default;

size_t ProcessedStack::GetStackSize() const { return mStack.size(); }

size_t ProcessedStack::GetNumModules() const { return mModules.size(); }

bool ProcessedStack::Module::operator==(const Module& aOther) const {
  return mName == aOther.mName && mBreakpadId == aOther.mBreakpadId;
}

const ProcessedStack::Frame& ProcessedStack::GetFrame(unsigned aIndex) const {
  MOZ_ASSERT(aIndex < mStack.size());
  return mStack[aIndex];
}

void ProcessedStack::AddFrame(const Frame& aFrame) { mStack.push_back(aFrame); }

const ProcessedStack::Module& ProcessedStack::GetModule(unsigned aIndex) const {
  MOZ_ASSERT(aIndex < mModules.size());
  return mModules[aIndex];
}

void ProcessedStack::AddModule(const Module& aModule) {
  mModules.push_back(aModule);
}

void ProcessedStack::Clear() {
  mModules.clear();
  mStack.clear();
}

ProcessedStack GetStackAndModules(const std::vector<uintptr_t>& aPCs) {
  return BatchProcessedStackGenerator().GetStackAndModules(aPCs);
}

BatchProcessedStackGenerator::BatchProcessedStackGenerator()
#ifdef MOZ_GECKO_PROFILER
    : mSortedRawModules(SharedLibraryInfo::GetInfoForSelf())
#endif
{
#ifdef MOZ_GECKO_PROFILER
  mSortedRawModules.SortByAddress();
#endif
}

#ifndef MOZ_GECKO_PROFILER
static ProcessedStack GetStackAndModulesInternal(
    std::vector<StackFrame>& aRawStack) {
#else
static ProcessedStack GetStackAndModulesInternal(
    std::vector<StackFrame>& aRawStack, SharedLibraryInfo& aSortedRawModules) {
  SharedLibraryInfo rawModules(aSortedRawModules);
  // Remove all modules not referenced by a PC on the stack
  std::sort(aRawStack.begin(), aRawStack.end(), CompareByPC);

  size_t moduleIndex = 0;
  size_t stackIndex = 0;
  size_t stackSize = aRawStack.size();

  while (moduleIndex < rawModules.GetSize()) {
    const SharedLibrary& module = rawModules.GetEntry(moduleIndex);
    uintptr_t moduleStart = module.GetStart();
    uintptr_t moduleEnd = module.GetEnd() - 1;
    // the interval is [moduleStart, moduleEnd)

    bool moduleReferenced = false;
    for (; stackIndex < stackSize; ++stackIndex) {
      uintptr_t pc = aRawStack[stackIndex].mPC;
      if (pc >= moduleEnd) break;

      if (pc >= moduleStart) {
        // If the current PC is within the current module, mark
        // module as used
        moduleReferenced = true;
        aRawStack[stackIndex].mPC -= moduleStart;
        aRawStack[stackIndex].mModIndex = moduleIndex;
      } else {
        // PC does not belong to any module. It is probably from
        // the JIT. Use a fixed mPC so that we don't get different
        // stacks on different runs.
        aRawStack[stackIndex].mPC = std::numeric_limits<uintptr_t>::max();
      }
    }

    if (moduleReferenced) {
      ++moduleIndex;
    } else {
      // Remove module if no PCs within its address range
      rawModules.RemoveEntries(moduleIndex, moduleIndex + 1);
    }
  }

  for (; stackIndex < stackSize; ++stackIndex) {
    // These PCs are past the last module.
    aRawStack[stackIndex].mPC = std::numeric_limits<uintptr_t>::max();
  }

  std::sort(aRawStack.begin(), aRawStack.end(), CompareByIndex);
#endif

  // Copy the information to the return value.
  ProcessedStack Ret;
  for (auto& rawFrame : aRawStack) {
    mozilla::Telemetry::ProcessedStack::Frame frame = {rawFrame.mPC,
                                                       rawFrame.mModIndex};
    Ret.AddFrame(frame);
  }

#ifdef MOZ_GECKO_PROFILER
  for (unsigned i = 0, n = rawModules.GetSize(); i != n; ++i) {
    const SharedLibrary& info = rawModules.GetEntry(i);
    mozilla::Telemetry::ProcessedStack::Module module = {info.GetDebugName(),
                                                         info.GetBreakpadId()};
    Ret.AddModule(module);
  }
#endif

  return Ret;
}

ProcessedStack BatchProcessedStackGenerator::GetStackAndModules(
    const std::vector<uintptr_t>& aPCs) {
  std::vector<StackFrame> rawStack;
  auto stackEnd = aPCs.begin() + std::min(aPCs.size(), kMaxChromeStackDepth);
  for (auto i = aPCs.begin(); i != stackEnd; ++i) {
    uintptr_t aPC = *i;
    StackFrame Frame = {aPC, static_cast<uint16_t>(rawStack.size()),
                        std::numeric_limits<uint16_t>::max()};
    rawStack.push_back(Frame);
  }

#if defined(MOZ_GECKO_PROFILER)
  return GetStackAndModulesInternal(rawStack, mSortedRawModules);
#else
  return GetStackAndModulesInternal(rawStack);
#endif
}

ProcessedStack BatchProcessedStackGenerator::GetStackAndModules(
    const uintptr_t* aBegin, const uintptr_t* aEnd) {
  std::vector<StackFrame> rawStack;
  for (auto i = aBegin; i != aEnd; ++i) {
    uintptr_t aPC = *i;
    StackFrame Frame = {aPC, static_cast<uint16_t>(rawStack.size()),
                        std::numeric_limits<uint16_t>::max()};
    rawStack.push_back(Frame);
  }

#if defined(MOZ_GECKO_PROFILER)
  return GetStackAndModulesInternal(rawStack, mSortedRawModules);
#else
  return GetStackAndModulesInternal(rawStack);
#endif
}

}  // namespace mozilla::Telemetry
