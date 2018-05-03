/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SimpleBuffer_h__
#define SimpleBuffer_h__

/*
  This class is similar to a nsPipe except it does not have any locking, stores
  an unbounded amount of data, can only be used on one thread, and has much
  simpler result code semantics to deal with.
*/

#include "prtypes.h"
#include "mozilla/LinkedList.h"
#include "nsIThreadInternal.h"

namespace mozilla {
namespace net {

class SimpleBufferPage : public LinkedListElement<SimpleBufferPage>
{
public:
  SimpleBufferPage() : mReadOffset(0), mWriteOffset(0) {}
  static const size_t kSimpleBufferPageSize = 32000;

private:
  friend class SimpleBuffer;
  char mBuffer[kSimpleBufferPageSize];
  size_t mReadOffset;
  size_t mWriteOffset;
};

class SimpleBuffer
{
public:
  SimpleBuffer();
  ~SimpleBuffer() = default;

  nsresult Write(char *stc, size_t len); // return OK or OUT_OF_MEMORY
  size_t Read(char *dest, size_t maxLen); // return bytes read
  size_t Available();
  void Clear();

private:
  NS_DECL_OWNINGTHREAD

  nsresult mStatus;
  AutoCleanLinkedList<SimpleBufferPage> mBufferList;
  size_t mAvailable;
};

} // namespace net
} // namespace mozilla

#endif
