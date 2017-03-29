// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SEQUENCE_CHECKER_IMPL_H_
#define BASE_SEQUENCE_CHECKER_IMPL_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequence_token.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker_impl.h"

namespace base {

// Real implementation of SequenceChecker for use in debug mode or for temporary
// use in release mode (e.g. to CHECK on a threading issue seen only in the
// wild).
//
// Note: You should almost always use the SequenceChecker class to get the right
// version for your build configuration.
class BASE_EXPORT SequenceCheckerImpl {
 public:
  SequenceCheckerImpl();
  ~SequenceCheckerImpl();

  // Returns true if called in sequence with previous calls to this method and
  // the constructor.
  bool CalledOnValidSequence() const WARN_UNUSED_RESULT;

  // Unbinds the checker from the currently associated sequence. The checker
  // will be re-bound on the next call to CalledOnValidSequence().
  void DetachFromSequence();

 private:
  void EnsureSequenceTokenAssigned() const;

  // Guards all variables below.
  mutable Lock lock_;

  // True when the SequenceChecker is bound to a sequence or a thread.
  mutable bool is_assigned_ = false;

  mutable SequenceToken sequence_token_;

  // TODO(gab): Remove this when SequencedWorkerPool is deprecated in favor of
  // TaskScheduler. crbug.com/622400
  mutable SequencedWorkerPool::SequenceToken sequenced_worker_pool_token_;

  // Used when |sequenced_worker_pool_token_| and |sequence_token_| are invalid.
  ThreadCheckerImpl thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SequenceCheckerImpl);
};

}  // namespace base

#endif  // BASE_SEQUENCE_CHECKER_IMPL_H_
