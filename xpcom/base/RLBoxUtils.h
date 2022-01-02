/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_RLBOX_UTILS_H_
#define SECURITY_RLBOX_UTILS_H_

#include "mozilla/rlbox/rlbox_types.hpp"

namespace mozilla {

/* The RLBoxTransferBufferToSandbox class is used to copy (or directly expose in
 * the noop-sandbox case) buffers into the sandbox that are automatically freed
 * when the RLBoxTransferBufferToSandbox is out of scope.  NOTE: The sandbox
 * lifetime must outlive all of its RLBoxTransferBufferToSandbox.
 */
template <typename T, typename S>
class MOZ_STACK_CLASS RLBoxTransferBufferToSandbox {
 public:
  RLBoxTransferBufferToSandbox() = delete;
  RLBoxTransferBufferToSandbox(rlbox::rlbox_sandbox<S>* aSandbox, const T* aBuf,
                               const size_t aLen)
      : mSandbox(aSandbox), mCopied(false), mBuf(nullptr) {
    if (aBuf) {
      mBuf = rlbox::copy_memory_or_grant_access(*mSandbox, aBuf, aLen, false,
                                                mCopied);
    }
  };
  ~RLBoxTransferBufferToSandbox() {
    if (mCopied) {
      mSandbox->free_in_sandbox(mBuf);
    }
  };
  rlbox::tainted<const T*, S> operator*() const { return mBuf; };

 private:
  rlbox::rlbox_sandbox<S>* mSandbox;
  bool mCopied;
  rlbox::tainted<const T*, S> mBuf;
};

/* The RLBoxAllocateInSandbox class is used to allocate data int sandbox that is
 * automatically freed when the RLBoxAllocateInSandbox is out of scope.  NOTE:
 * The sandbox lifetime must outlive all of its RLBoxAllocateInSandbox'ations.
 */
template <typename T, typename S>
class MOZ_STACK_CLASS RLBoxAllocateInSandbox {
 public:
  RLBoxAllocateInSandbox() = delete;
  explicit RLBoxAllocateInSandbox(rlbox::rlbox_sandbox<S>* aSandbox)
      : mSandbox(aSandbox) {
    mPtr = mSandbox->template malloc_in_sandbox<T>();
  };
  ~RLBoxAllocateInSandbox() {
    if (mPtr) {
      mSandbox->free_in_sandbox(mPtr);
    }
  };
  rlbox::tainted<T*, S> get() const { return mPtr; };

 private:
  rlbox::rlbox_sandbox<S>* mSandbox;
  rlbox::tainted<T*, S> mPtr;
};

}  // namespace mozilla

#endif
