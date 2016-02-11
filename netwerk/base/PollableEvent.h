/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PollableEvent_h__
#define PollableEvent_h__

#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

// class must be called locked
class PollableEvent
{
public:
  PollableEvent();
  ~PollableEvent();

  // Signal/Clear return false only if they fail
  bool Signal();
  bool Clear();
  bool Valid() { return mWriteFD && mReadFD; }

  PRFileDesc *PollableFD() { return mReadFD; }

private:
  PRFileDesc *mWriteFD;
  PRFileDesc *mReadFD;
  bool        mSignaled;
};

} // namespace net
} // namespace mozilla

#endif
