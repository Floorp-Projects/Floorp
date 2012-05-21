/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* API for getting a stack trace of the C/C++ */

#ifndef StackWalk_h_
#define StackWalk_h_

// XXX: it would be nice to eventually remove this header dependency on nsStackWalk.h
#include "nsStackWalk.h"

namespace mozilla {

nsresult
FramePointerStackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
                      void *aClosure, void **bp, void *stackEnd);

}

#endif /* !defined(StackWalk_h_) */
