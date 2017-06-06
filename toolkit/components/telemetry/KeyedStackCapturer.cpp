/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyedStackCapturer.h"
#include "nsPrintfCString.h"
#include "mozilla/StackWalk.h"
#include "ProcessedStack.h"
#include "jsapi.h"

namespace {

#if defined(MOZ_GECKO_PROFILER)

/** Defines the size of the keyed stack dictionary. */
const uint8_t kMaxKeyLength = 50;

/** The maximum number of captured stacks that we're keeping. */
const size_t kMaxCapturedStacksKept = 50;

/**
 * Checks if a single character of the key string is valid.
 *
 * @param aChar a character to validate.
 * @return True, if the char is valid, False - otherwise.
 */
bool
IsKeyCharValid(const char aChar)
{
  return (aChar >= 'A' && aChar <= 'Z')
      || (aChar >= 'a' && aChar <= 'z')
      || (aChar >= '0' && aChar <= '9')
      || aChar == '-';
}

/**
 * Checks if a given string is a valid telemetry key.
 *
 * @param aKey is the key string.
 * @return True, if the key is valid, False - otherwise.
 */
bool
IsKeyValid(const nsACString& aKey)
{
  // Check key length.
  if (aKey.Length() > kMaxKeyLength) {
    return false;
  }

  // Check key characters.
  const char* cur = aKey.BeginReading();
  const char* end = aKey.EndReading();

  for (; cur < end; ++cur) {
      if (!IsKeyCharValid(*cur)) {
        return false;
      }
  }
  return true;
}

} // anonymous namespace

namespace mozilla {
namespace Telemetry {

void KeyedStackCapturer::Capture(const nsACString& aKey) {
  MutexAutoLock captureStackMutex(mStackCapturerMutex);

  // Check if the key is ok.
  if (!IsKeyValid(aKey)) {
    NS_WARNING(nsPrintfCString(
      "Invalid key is used to capture stack in telemetry: '%s'",
      PromiseFlatCString(aKey).get()
    ).get());
    return;
  }

  // Trying to find and update the stack information.
  StackFrequencyInfo* info = mStackInfos.Get(aKey);
  if (info) {
    // We already recorded this stack before, only increase the count.
    info->mCount++;
    return;
  }

  // Check if we have room for new captures.
  if (mStackInfos.Count() >= kMaxCapturedStacksKept) {
    // Addressed by Bug 1316793.
    return;
  }

  // We haven't captured a stack for this key before, do it now.
  // Note that this does a stackwalk and is an expensive operation.
  std::vector<uintptr_t> rawStack;
  auto callback = [](uint32_t, void* aPC, void*, void* aClosure) {
    std::vector<uintptr_t>* stack =
      static_cast<std::vector<uintptr_t>*>(aClosure);
    stack->push_back(reinterpret_cast<uintptr_t>(aPC));
  };
  MozStackWalk(callback, /* skipFrames */ 0,
              /* maxFrames */ 0, reinterpret_cast<void*>(&rawStack), 0, nullptr);
  ProcessedStack stack = GetStackAndModules(rawStack);

  // Store the new stack info.
  size_t stackIndex = mStacks.AddStack(stack);
  mStackInfos.Put(aKey, new StackFrequencyInfo(1, stackIndex));
}

NS_IMETHODIMP
KeyedStackCapturer::ReflectCapturedStacks(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  MutexAutoLock capturedStackMutex(mStackCapturerMutex);

  // this adds the memoryMap and stacks properties.
  JS::RootedObject fullReportObj(cx, CreateJSStackObject(cx, mStacks));
  if (!fullReportObj) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject keysArray(cx, JS_NewArrayObject(cx, 0));
  if (!keysArray) {
    return NS_ERROR_FAILURE;
  }

  bool ok = JS_DefineProperty(cx, fullReportObj, "captures",
                              keysArray, JSPROP_ENUMERATE);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  size_t keyIndex = 0;
  for (auto iter = mStackInfos.ConstIter(); !iter.Done(); iter.Next(), ++keyIndex) {
    const StackFrequencyInfo* info = iter.Data();

    JS::RootedObject infoArray(cx, JS_NewArrayObject(cx, 0));
    if (!keysArray) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedString str(cx, JS_NewStringCopyZ(cx,
                         PromiseFlatCString(iter.Key()).get()));
    if (!str ||
        !JS_DefineElement(cx, infoArray, 0, str, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, infoArray, 1, info->mIndex, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, infoArray, 2, info->mCount, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, keysArray, keyIndex, infoArray, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  ret.setObject(*fullReportObj);
  return NS_OK;
}

void
KeyedStackCapturer::Clear()
{
  MutexAutoLock captureStackMutex(mStackCapturerMutex);
  mStackInfos.Clear();
  mStacks.Clear();
}
#endif

} // namespace Telemetry
} // namespace mozilla
