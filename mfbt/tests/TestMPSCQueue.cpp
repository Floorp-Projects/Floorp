/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MPSCQueue.h"
#include "mozilla/PodOperations.h"
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>

using namespace mozilla;

struct NativeStack {
  void* mPCs[32];
  void* mSPs[32];
  size_t mCount;
  size_t mTid;
};

void StackWalkCallback(void* aPC, void* aSP, NativeStack* nativeStack) {
  nativeStack->mSPs[nativeStack->mCount] = aSP;
  nativeStack->mPCs[nativeStack->mCount] = aPC;
  nativeStack->mCount++;
}

void FillNativeStack(NativeStack* aStack) {
  StackWalkCallback((void*)0x1234, (void*)0x9876, aStack);
  StackWalkCallback((void*)0x3456, (void*)0x5432, aStack);
  StackWalkCallback((void*)0x7890, (void*)0x1098, aStack);
  StackWalkCallback((void*)0x1234, (void*)0x7654, aStack);
  StackWalkCallback((void*)0x5678, (void*)0x3210, aStack);
  StackWalkCallback((void*)0x9012, (void*)0x9876, aStack);
  StackWalkCallback((void*)0x1334, (void*)0x9786, aStack);
  StackWalkCallback((void*)0x3546, (void*)0x5342, aStack);
  StackWalkCallback((void*)0x7809, (void*)0x0198, aStack);
  StackWalkCallback((void*)0x4123, (void*)0x7645, aStack);
  StackWalkCallback((void*)0x5768, (void*)0x3120, aStack);
  StackWalkCallback((void*)0x9102, (void*)0x9867, aStack);
  StackWalkCallback((void*)0x1243, (void*)0x8976, aStack);
  StackWalkCallback((void*)0x6345, (void*)0x4325, aStack);
  StackWalkCallback((void*)0x8790, (void*)0x1908, aStack);
  StackWalkCallback((void*)0x134, (void*)0x654, aStack);
  StackWalkCallback((void*)0x567, (void*)0x320, aStack);
  StackWalkCallback((void*)0x901, (void*)0x976, aStack);
}

void BasicAPITestWithStack(MPSCQueue<NativeStack>& aQueue, size_t aCap) {
  MOZ_RELEASE_ASSERT(aQueue.Capacity() == aCap);

  NativeStack s = {.mCount = 0};
  FillNativeStack(&s);
  MOZ_RELEASE_ASSERT(s.mCount == 18);

  int store = -1;
  for (size_t i = 0; i < aCap; ++i) {
    store = aQueue.Send(s);
    MOZ_RELEASE_ASSERT(store > 0);
  }

  int retrieve = -1;
  for (size_t i = 0; i < aCap; ++i) {
    NativeStack sr{};
    retrieve = aQueue.Recv(&sr);

    MOZ_RELEASE_ASSERT(retrieve > 0);
    MOZ_RELEASE_ASSERT(&s != &sr);
    MOZ_RELEASE_ASSERT(s.mCount == sr.mCount);

    for (size_t i = 0; i < s.mCount; ++i) {
      MOZ_RELEASE_ASSERT(s.mPCs[i] == sr.mPCs[i]);
      MOZ_RELEASE_ASSERT(s.mSPs[i] == sr.mSPs[i]);
    }
  }
}

void BasicAPITestMP(MPSCQueue<NativeStack>& aQueue, size_t aThreads) {
  MOZ_RELEASE_ASSERT(aQueue.Capacity() == 15);

  std::thread consumer([&aQueue, aThreads] {
    size_t received = 0;
    NativeStack v{};
    do {
      int deq = aQueue.Recv(&v);
      if (deq > 0) {
        received++;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    } while (received < aThreads);
  });

  std::thread producers[aThreads];
  for (size_t t = 0; t < aThreads; ++t) {
    producers[t] = std::thread([&aQueue, t] {
      NativeStack s = {.mCount = 0, .mTid = t};
      FillNativeStack(&s);
      MOZ_RELEASE_ASSERT(s.mCount == 18);

      int sent = 0;
      // wrap in a do { } while () because Send() will return 0 on message being
      // dropped so we want to retry
      do {
        std::this_thread::sleep_for(std::chrono::microseconds(5));
        sent = aQueue.Send(s);
      } while (sent == 0);
    });
  }

  for (size_t t = 0; t < aThreads; ++t) {
    producers[t].join();
  }
  consumer.join();
}

int main() {
  size_t caps[] = {2, 5, 7, 10, 15};
  for (auto maxCap : caps) {
    MPSCQueue<NativeStack> s(maxCap);
    BasicAPITestWithStack(s, maxCap);
  }

  {
    NativeStack e{};
    MPSCQueue<NativeStack> deq(2);

    // Dequeue with nothing should return 0 and not fail later
    int retrieve = deq.Recv(&e);
    MOZ_RELEASE_ASSERT(retrieve == 0);

    NativeStack real = {.mCount = 0};
    FillNativeStack(&real);
    MOZ_RELEASE_ASSERT(real.mCount == 18);

    int store = deq.Send(real);
    MOZ_RELEASE_ASSERT(store > 0);
    store = deq.Send(real);
    MOZ_RELEASE_ASSERT(store > 0);

    // should be full we should get 0
    store = deq.Send(real);
    MOZ_RELEASE_ASSERT(store == 0);

    // try to dequeue
    NativeStack e1{};
    retrieve = deq.Recv(&e1);
    MOZ_RELEASE_ASSERT(retrieve > 0);
    MOZ_RELEASE_ASSERT(e1.mCount == 18);

    NativeStack e2{};
    retrieve = deq.Recv(&e2);
    MOZ_RELEASE_ASSERT(retrieve > 0);
    MOZ_RELEASE_ASSERT(e2.mCount == 18);

    retrieve = deq.Recv(&e);
    MOZ_RELEASE_ASSERT(retrieve == 0);
  }

  size_t nbThreads[] = {8, 16, 64, 128, 512, 1024};
  for (auto threads : nbThreads) {
    MPSCQueue<NativeStack> s(15);
    BasicAPITestMP(s, threads);
  }

  return 0;
}
