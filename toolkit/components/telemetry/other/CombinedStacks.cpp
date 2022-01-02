/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CombinedStacks.h"

#include "jsapi.h"
#include "js/Array.h"               // JS::NewArrayObject
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_DefineProperty
#include "js/String.h"
#include "mozilla/HangAnnotations.h"

namespace mozilla::Telemetry {

// The maximum number of chrome hangs stacks that we're keeping.
const size_t kMaxChromeStacksKept = 50;

CombinedStacks::CombinedStacks() : CombinedStacks(kMaxChromeStacksKept) {}

CombinedStacks::CombinedStacks(size_t aMaxStacksCount)
    : mNextIndex(0), mMaxStacksCount(aMaxStacksCount) {}

size_t CombinedStacks::GetModuleCount() const { return mModules.size(); }

const Telemetry::ProcessedStack::Module& CombinedStacks::GetModule(
    unsigned aIndex) const {
  return mModules[aIndex];
}

void CombinedStacks::AddFrame(
    size_t aStackIndex, const ProcessedStack::Frame& aFrame,
    const std::function<const ProcessedStack::Module&(int)>& aModuleGetter) {
  uint16_t modIndex;
  if (aFrame.mModIndex == std::numeric_limits<uint16_t>::max()) {
    modIndex = aFrame.mModIndex;
  } else {
    const ProcessedStack::Module& module = aModuleGetter(aFrame.mModIndex);
    auto modIterator = std::find(mModules.begin(), mModules.end(), module);
    if (modIterator == mModules.end()) {
      mModules.push_back(module);
      modIndex = mModules.size() - 1;
    } else {
      modIndex = modIterator - mModules.begin();
    }
  }
  mStacks[aStackIndex].push_back(
      ProcessedStack::Frame{aFrame.mOffset, modIndex});
}

size_t CombinedStacks::AddStack(const Telemetry::ProcessedStack& aStack) {
  size_t index = mNextIndex;
  // Advance the indices of the circular queue holding the stacks.
  mNextIndex = (mNextIndex + 1) % mMaxStacksCount;
  // Grow the vector up to the maximum size, if needed.
  if (mStacks.size() < mMaxStacksCount) {
    mStacks.resize(mStacks.size() + 1);
  }

  // Clear the old stack before set.
  mStacks[index].clear();

  size_t stackSize = aStack.GetStackSize();
  for (size_t i = 0; i < stackSize; ++i) {
    // Need to specify a return type in the following lambda,
    // otherwise it's incorrectly deduced to be a non-reference type.
    AddFrame(index, aStack.GetFrame(i),
             [&aStack](int aIdx) -> const ProcessedStack::Module& {
               return aStack.GetModule(aIdx);
             });
  }
  return index;
}

void CombinedStacks::AddStacks(const CombinedStacks& aStacks) {
  mStacks.resize(
      std::min(mStacks.size() + aStacks.GetStackCount(), mMaxStacksCount));

  for (const auto& stack : aStacks.mStacks) {
    size_t index = mNextIndex;
    // Advance the indices of the circular queue holding the stacks.
    mNextIndex = (mNextIndex + 1) % mMaxStacksCount;

    // Clear the old stack before set.
    mStacks[index].clear();

    for (const auto& frame : stack) {
      // Need to specify a return type in the following lambda,
      // otherwise it's incorrectly deduced to be a non-reference type.
      AddFrame(index, frame,
               [&aStacks](int aIdx) -> const ProcessedStack::Module& {
                 return aStacks.mModules[aIdx];
               });
    }
  }
}

const CombinedStacks::Stack& CombinedStacks::GetStack(unsigned aIndex) const {
  return mStacks[aIndex];
}

size_t CombinedStacks::GetStackCount() const { return mStacks.size(); }

size_t CombinedStacks::SizeOfExcludingThis() const {
  // This is a crude approximation. We would like to do something like
  // aMallocSizeOf(&mModules[0]), but on linux aMallocSizeOf will call
  // malloc_usable_size which is only safe on the pointers returned by malloc.
  // While it works on current libstdc++, it is better to be safe and not assume
  // that &vec[0] points to one. We could use a custom allocator, but
  // it doesn't seem worth it.
  size_t n = 0;
  n += mModules.capacity() * sizeof(Telemetry::ProcessedStack::Module);
  n += mStacks.capacity() * sizeof(Stack);
  for (const auto& s : mStacks) {
    n += s.capacity() * sizeof(Telemetry::ProcessedStack::Frame);
  }
  return n;
}

void CombinedStacks::RemoveStack(unsigned aIndex) {
  MOZ_ASSERT(aIndex < mStacks.size());

  mStacks.erase(mStacks.begin() + aIndex);

  if (aIndex < mNextIndex) {
    if (mNextIndex == 0) {
      mNextIndex = mStacks.size();
    } else {
      mNextIndex--;
    }
  }

  if (mNextIndex > mStacks.size()) {
    mNextIndex = mStacks.size();
  }
}

void CombinedStacks::Swap(CombinedStacks& aOther) {
  mModules.swap(aOther.mModules);
  mStacks.swap(aOther.mStacks);

  size_t nextIndex = aOther.mNextIndex;
  aOther.mNextIndex = mNextIndex;
  mNextIndex = nextIndex;

  size_t maxStacksCount = aOther.mMaxStacksCount;
  aOther.mMaxStacksCount = mMaxStacksCount;
  mMaxStacksCount = maxStacksCount;
}

#if defined(MOZ_GECKO_PROFILER)
void CombinedStacks::Clear() {
  mNextIndex = 0;
  mStacks.clear();
  mModules.clear();
}
#endif

JSObject* CreateJSStackObject(JSContext* cx, const CombinedStacks& stacks) {
  JS::Rooted<JSObject*> ret(cx, JS_NewPlainObject(cx));
  if (!ret) {
    return nullptr;
  }

  JS::Rooted<JSObject*> moduleArray(cx, JS::NewArrayObject(cx, 0));
  if (!moduleArray) {
    return nullptr;
  }
  bool ok =
      JS_DefineProperty(cx, ret, "memoryMap", moduleArray, JSPROP_ENUMERATE);
  if (!ok) {
    return nullptr;
  }

  const size_t moduleCount = stacks.GetModuleCount();
  for (size_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex) {
    // Current module
    const Telemetry::ProcessedStack::Module& module =
        stacks.GetModule(moduleIndex);

    JS::Rooted<JSObject*> moduleInfoArray(cx, JS::NewArrayObject(cx, 0));
    if (!moduleInfoArray) {
      return nullptr;
    }
    if (!JS_DefineElement(cx, moduleArray, moduleIndex, moduleInfoArray,
                          JSPROP_ENUMERATE)) {
      return nullptr;
    }

    unsigned index = 0;

    // Module name
    JS::Rooted<JSString*> str(cx, JS_NewUCStringCopyZ(cx, module.mName.get()));
    if (!str || !JS_DefineElement(cx, moduleInfoArray, index++, str,
                                  JSPROP_ENUMERATE)) {
      return nullptr;
    }

    // Module breakpad identifier
    JS::Rooted<JSString*> id(cx,
                             JS_NewStringCopyZ(cx, module.mBreakpadId.get()));
    if (!id ||
        !JS_DefineElement(cx, moduleInfoArray, index, id, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }

  JS::Rooted<JSObject*> reportArray(cx, JS::NewArrayObject(cx, 0));
  if (!reportArray) {
    return nullptr;
  }
  ok = JS_DefineProperty(cx, ret, "stacks", reportArray, JSPROP_ENUMERATE);
  if (!ok) {
    return nullptr;
  }

  const size_t length = stacks.GetStackCount();
  for (size_t i = 0; i < length; ++i) {
    // Represent call stack PCs as (module index, offset) pairs.
    JS::Rooted<JSObject*> pcArray(cx, JS::NewArrayObject(cx, 0));
    if (!pcArray) {
      return nullptr;
    }

    if (!JS_DefineElement(cx, reportArray, i, pcArray, JSPROP_ENUMERATE)) {
      return nullptr;
    }

    const CombinedStacks::Stack& stack = stacks.GetStack(i);
    const uint32_t pcCount = stack.size();
    for (size_t pcIndex = 0; pcIndex < pcCount; ++pcIndex) {
      const Telemetry::ProcessedStack::Frame& frame = stack[pcIndex];
      JS::Rooted<JSObject*> framePair(cx, JS::NewArrayObject(cx, 0));
      if (!framePair) {
        return nullptr;
      }
      int modIndex = (std::numeric_limits<uint16_t>::max() == frame.mModIndex)
                         ? -1
                         : frame.mModIndex;
      if (!JS_DefineElement(cx, framePair, 0, modIndex, JSPROP_ENUMERATE)) {
        return nullptr;
      }
      if (!JS_DefineElement(cx, framePair, 1,
                            static_cast<double>(frame.mOffset),
                            JSPROP_ENUMERATE)) {
        return nullptr;
      }
      if (!JS_DefineElement(cx, pcArray, pcIndex, framePair,
                            JSPROP_ENUMERATE)) {
        return nullptr;
      }
    }
  }

  return ret;
}

}  // namespace mozilla::Telemetry
