/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdint>

// Run `func`, called with `param`, on a separate and possibly larger stack.
// This does not allocate a new thread.
//
// (The initial stack size will be the program's default thread stack size, but
// the stack may grow by as much as `reserved_stack_size`. This additional space
// will be allocated only if it's needed, and even then only a page at a time,
// so it's probably better to overestimate than underestimate.)
//
// https://docs.microsoft.com/en-us/windows/win32/procthread/thread-stack-size
void RunOnTemporaryStack(void (*func)(void*), void* param,
                         size_t reserved_stack_size);
