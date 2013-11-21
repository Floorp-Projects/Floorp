/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeadlockDetector.h"
#include "nsStackWalk.h"
#include "mozilla/Util.h"

namespace mozilla {

static void
StackCallback(void* pc, void* sp, void* closure)
{
  nsTArray<void*>* stack = static_cast<nsTArray<void*>*>(closure);
  stack->AppendElement(pc);
}

nsTArray<void*>
CallStack::GetBacktrace()
{
  nsTArray<void*> callstack;
  callstack.SetCapacity(32); // Hopefully 32 frames is a good average
  NS_StackWalk(StackCallback, 2, 0, &callstack, 0, nullptr);
  return mozilla::Move(callstack);
}

void
CallStack::Print(FILE* f) const
{
  if (mCallStack.IsEmpty()) {
    fputs("  [stack trace unavailable]\n", f);
  } else {
    nsCodeAddressDetails addr;
    for (uint32_t i = 0; i < mCallStack.Length(); ++i) {
      nsresult rv = NS_DescribeCodeAddress(mCallStack[i], &addr);
      if (NS_SUCCEEDED(rv)) {
        char buf[1024];
        NS_FormatCodeAddressDetails(mCallStack[i], &addr, buf, ArrayLength(buf));
        fputs(buf, f);
      } else {
        fprintf(f, "Frame information for %p unavailable", mCallStack[i]);
      }
    }
  }
  fflush(f);
}

}
