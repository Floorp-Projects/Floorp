// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"

namespace sandbox {

ErrorCode::ErrorCode(int err) {
  switch (err) {
    case ERR_ALLOWED:
      err_ = SECCOMP_RET_ALLOW;
      error_type_ = ET_SIMPLE;
      break;
    case ERR_MIN_ERRNO... ERR_MAX_ERRNO:
      err_ = SECCOMP_RET_ERRNO + err;
      error_type_ = ET_SIMPLE;
      break;
    default:
      SANDBOX_DIE("Invalid use of ErrorCode object");
  }
}

ErrorCode::ErrorCode(Trap::TrapFnc fnc, const void* aux, bool safe, uint16_t id)
    : error_type_(ET_TRAP),
      fnc_(fnc),
      aux_(const_cast<void*>(aux)),
      safe_(safe),
      err_(SECCOMP_RET_TRAP + id) {}

ErrorCode::ErrorCode(int argno,
                     ArgType width,
                     Operation op,
                     uint64_t value,
                     const ErrorCode* passed,
                     const ErrorCode* failed)
    : error_type_(ET_COND),
      value_(value),
      argno_(argno),
      width_(width),
      op_(op),
      passed_(passed),
      failed_(failed),
      err_(SECCOMP_RET_INVALID) {
  if (op < 0 || op >= OP_NUM_OPS) {
    SANDBOX_DIE("Invalid opcode in BPF sandbox rules");
  }
}

bool ErrorCode::Equals(const ErrorCode& err) const {
  if (error_type_ == ET_INVALID || err.error_type_ == ET_INVALID) {
    SANDBOX_DIE("Dereferencing invalid ErrorCode");
  }
  if (error_type_ != err.error_type_) {
    return false;
  }
  if (error_type_ == ET_SIMPLE || error_type_ == ET_TRAP) {
    return err_ == err.err_;
  } else if (error_type_ == ET_COND) {
    return value_ == err.value_ && argno_ == err.argno_ &&
           width_ == err.width_ && op_ == err.op_ &&
           passed_->Equals(*err.passed_) && failed_->Equals(*err.failed_);
  } else {
    SANDBOX_DIE("Corrupted ErrorCode");
  }
}

bool ErrorCode::LessThan(const ErrorCode& err) const {
  // Implementing a "LessThan()" operator allows us to use ErrorCode objects
  // as keys in STL containers; most notably, it also allows us to put them
  // into std::set<>. Actual ordering is not important as long as it is
  // deterministic.
  if (error_type_ == ET_INVALID || err.error_type_ == ET_INVALID) {
    SANDBOX_DIE("Dereferencing invalid ErrorCode");
  }
  if (error_type_ != err.error_type_) {
    return error_type_ < err.error_type_;
  } else {
    if (error_type_ == ET_SIMPLE || error_type_ == ET_TRAP) {
      return err_ < err.err_;
    } else if (error_type_ == ET_COND) {
      if (value_ != err.value_) {
        return value_ < err.value_;
      } else if (argno_ != err.argno_) {
        return argno_ < err.argno_;
      } else if (width_ != err.width_) {
        return width_ < err.width_;
      } else if (op_ != err.op_) {
        return op_ < err.op_;
      } else if (!passed_->Equals(*err.passed_)) {
        return passed_->LessThan(*err.passed_);
      } else if (!failed_->Equals(*err.failed_)) {
        return failed_->LessThan(*err.failed_);
      } else {
        return false;
      }
    } else {
      SANDBOX_DIE("Corrupted ErrorCode");
    }
  }
}

}  // namespace sandbox
