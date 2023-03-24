/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSegmentedBuffer.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/ScopeExit.h"

nsresult nsSegmentedBuffer::Init(uint32_t aSegmentSize) {
  if (mSegmentArrayCount != 0) {
    return NS_ERROR_FAILURE;  // initialized more than once
  }
  mSegmentSize = aSegmentSize;
  mSegmentArrayCount = NS_SEGMENTARRAY_INITIAL_COUNT;
  return NS_OK;
}

char* nsSegmentedBuffer::AppendNewSegment() {
  if (!mSegmentArray) {
    uint32_t bytes = mSegmentArrayCount * sizeof(char*);
    mSegmentArray = (char**)moz_xmalloc(bytes);
    memset(mSegmentArray, 0, bytes);
  }

  if (IsFull()) {
    mozilla::CheckedInt<uint32_t> newArraySize =
        mozilla::CheckedInt<uint32_t>(mSegmentArrayCount) * 2;
    mozilla::CheckedInt<uint32_t> bytes = newArraySize * sizeof(char*);
    if (!bytes.isValid()) {
      return nullptr;
    }

    mSegmentArray = (char**)moz_xrealloc(mSegmentArray, bytes.value());
    // copy wrapped content to new extension
    if (mFirstSegmentIndex > mLastSegmentIndex) {
      // deal with wrap around case
      memcpy(&mSegmentArray[mSegmentArrayCount], mSegmentArray,
             mLastSegmentIndex * sizeof(char*));
      memset(mSegmentArray, 0, mLastSegmentIndex * sizeof(char*));
      mLastSegmentIndex += mSegmentArrayCount;
      memset(&mSegmentArray[mLastSegmentIndex], 0,
             (newArraySize.value() - mLastSegmentIndex) * sizeof(char*));
    } else {
      memset(&mSegmentArray[mLastSegmentIndex], 0,
             (newArraySize.value() - mLastSegmentIndex) * sizeof(char*));
    }
    mSegmentArrayCount = newArraySize.value();
  }

  char* seg = (char*)malloc(mSegmentSize);
  if (!seg) {
    return nullptr;
  }
  mSegmentArray[mLastSegmentIndex] = seg;
  mLastSegmentIndex = ModSegArraySize(mLastSegmentIndex + 1);
  return seg;
}

bool nsSegmentedBuffer::DeleteFirstSegment() {
  NS_ASSERTION(mSegmentArray[mFirstSegmentIndex] != nullptr,
               "deleting bad segment");
  FreeOMT(mSegmentArray[mFirstSegmentIndex]);
  mSegmentArray[mFirstSegmentIndex] = nullptr;
  int32_t last = ModSegArraySize(mLastSegmentIndex - 1);
  if (mFirstSegmentIndex == last) {
    mLastSegmentIndex = last;
    return true;
  } else {
    mFirstSegmentIndex = ModSegArraySize(mFirstSegmentIndex + 1);
    return false;
  }
}

bool nsSegmentedBuffer::DeleteLastSegment() {
  int32_t last = ModSegArraySize(mLastSegmentIndex - 1);
  NS_ASSERTION(mSegmentArray[last] != nullptr, "deleting bad segment");
  FreeOMT(mSegmentArray[last]);
  mSegmentArray[last] = nullptr;
  mLastSegmentIndex = last;
  return (bool)(mLastSegmentIndex == mFirstSegmentIndex);
}

bool nsSegmentedBuffer::ReallocLastSegment(size_t aNewSize) {
  int32_t last = ModSegArraySize(mLastSegmentIndex - 1);
  NS_ASSERTION(mSegmentArray[last] != nullptr, "realloc'ing bad segment");
  char* newSegment = (char*)realloc(mSegmentArray[last], aNewSize);
  if (newSegment) {
    mSegmentArray[last] = newSegment;
    return true;
  }
  return false;
}

void nsSegmentedBuffer::Empty() {
  auto clearMembers = mozilla::MakeScopeExit([&] {
    mSegmentArray = nullptr;
    mSegmentArrayCount = NS_SEGMENTARRAY_INITIAL_COUNT;
    mFirstSegmentIndex = mLastSegmentIndex = 0;
  });

  // If mSegmentArray is null, there's no need to actually free anything
  if (!mSegmentArray) {
    return;
  }

  // Dispatch a task that frees up the array. This may run immediately or on
  // a background thread.
  FreeOMT([segmentArray = mSegmentArray, arrayCount = mSegmentArrayCount]() {
    for (uint32_t i = 0; i < arrayCount; i++) {
      if (segmentArray[i]) {
        free(segmentArray[i]);
      }
    }
    free(segmentArray);
  });
}

void nsSegmentedBuffer::FreeOMT(void* aPtr) {
  FreeOMT([aPtr]() { free(aPtr); });
}

void nsSegmentedBuffer::FreeOMT(std::function<void()>&& aTask) {
  if (!NS_IsMainThread()) {
    aTask();
    return;
  }

  if (mFreeOMT) {
    // There is a runnable pending which will handle this object
    if (mFreeOMT->AddTask(std::move(aTask)) > 1) {
      return;
    }
  } else {
    mFreeOMT = mozilla::MakeRefPtr<FreeOMTPointers>();
    mFreeOMT->AddTask(std::move(aTask));
  }

  if (!mIOThread) {
    mIOThread = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  }

  // During the shutdown we are not able to obtain the IOThread and/or the
  // dispatching of runnable fails.
  if (!mIOThread || NS_FAILED(mIOThread->Dispatch(NS_NewRunnableFunction(
                        "nsSegmentedBuffer::FreeOMT",
                        [obj = mFreeOMT]() { obj->FreeAll(); })))) {
    mFreeOMT->FreeAll();
  }
}

void nsSegmentedBuffer::FreeOMTPointers::FreeAll() {
  // Take all the tasks from the object. If AddTask is called after we release
  // the lock, then another runnable will be dispatched for that task. This is
  // necessary to avoid blocking the main thread while memory is being freed.
  nsTArray<std::function<void()>> tasks = [this]() {
    auto t = mTasks.Lock();
    return std::move(*t);
  }();

  // Finally run all the tasks to free memory.
  for (auto& task : tasks) {
    task();
  }
}

////////////////////////////////////////////////////////////////////////////////
