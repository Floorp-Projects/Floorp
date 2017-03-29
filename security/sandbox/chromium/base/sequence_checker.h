// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SEQUENCE_CHECKER_H_
#define BASE_SEQUENCE_CHECKER_H_

#include "base/sequence_checker_impl.h"

namespace base {

// Do nothing implementation, for use in release mode.
//
// Note: You should almost always use the SequenceChecker class to get
// the right version for your build configuration.
class SequenceCheckerDoNothing {
 public:
  bool CalledOnValidSequence() const { return true; }

  void DetachFromSequence() {}
};

// SequenceChecker is a helper class to verify that calls to some methods of a
// class are sequenced. Calls are sequenced when they are issued:
// - From tasks posted to SequencedTaskRunners or SingleThreadTaskRunners bound
//   to the same sequence, or,
// - From a single thread outside of any task.
//
// Example:
// class MyClass {
//  public:
//   void Foo() {
//     DCHECK(sequence_checker_.CalledOnValidSequence());
//     ... (do stuff) ...
//   }
//
//  private:
//   SequenceChecker sequence_checker_;
// }
//
// In Release mode, CalledOnValidSequence() will always return true.
#if DCHECK_IS_ON()
class SequenceChecker : public SequenceCheckerImpl {
};
#else
class SequenceChecker : public SequenceCheckerDoNothing {
};
#endif  // DCHECK_IS_ON()

}  // namespace base

#endif  // BASE_SEQUENCE_CHECKER_H_
