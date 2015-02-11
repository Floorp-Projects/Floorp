// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/errorcode.h"

#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace sandbox {

ErrorCode::ErrorCode() : error_type_(ET_INVALID), err_(SECCOMP_RET_INVALID) {
}

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
      if ((err & ~SECCOMP_RET_DATA) == ERR_TRACE) {
        err_ = SECCOMP_RET_TRACE + (err & SECCOMP_RET_DATA);
        error_type_ = ET_SIMPLE;
        break;
      }
      SANDBOX_DIE("Invalid use of ErrorCode object");
  }
}

ErrorCode::ErrorCode(uint16_t trap_id,
                     Trap::TrapFnc fnc,
                     const void* aux,
                     bool safe)
    : error_type_(ET_TRAP),
      fnc_(fnc),
      aux_(const_cast<void*>(aux)),
      safe_(safe),
      err_(SECCOMP_RET_TRAP + trap_id) {
}

ErrorCode::ErrorCode(int argno,
                     ArgType width,
                     uint64_t mask,
                     uint64_t value,
                     const ErrorCode* passed,
                     const ErrorCode* failed)
    : error_type_(ET_COND),
      mask_(mask),
      value_(value),
      argno_(argno),
      width_(width),
      passed_(passed),
      failed_(failed),
      err_(SECCOMP_RET_INVALID) {
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
    return mask_ == err.mask_ && value_ == err.value_ && argno_ == err.argno_ &&
           width_ == err.width_ && passed_->Equals(*err.passed_) &&
           failed_->Equals(*err.failed_);
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
      if (mask_ != err.mask_) {
        return mask_ < err.mask_;
      } else if (value_ != err.value_) {
        return value_ < err.value_;
      } else if (argno_ != err.argno_) {
        return argno_ < err.argno_;
      } else if (width_ != err.width_) {
        return width_ < err.width_;
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
