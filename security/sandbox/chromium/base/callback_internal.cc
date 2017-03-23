// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_internal.h"

#include "base/logging.h"

namespace base {
namespace internal {

namespace {

bool ReturnFalse(const BindStateBase*) {
  return false;
}

}  // namespace

BindStateBase::BindStateBase(InvokeFuncStorage polymorphic_invoke,
                             void (*destructor)(const BindStateBase*))
    : BindStateBase(polymorphic_invoke, destructor, &ReturnFalse) {
}

BindStateBase::BindStateBase(InvokeFuncStorage polymorphic_invoke,
                             void (*destructor)(const BindStateBase*),
                             bool (*is_cancelled)(const BindStateBase*))
    : polymorphic_invoke_(polymorphic_invoke),
      ref_count_(0),
      destructor_(destructor),
      is_cancelled_(is_cancelled) {}

void BindStateBase::AddRef() const {
  AtomicRefCountInc(&ref_count_);
}

void BindStateBase::Release() const {
  if (!AtomicRefCountDec(&ref_count_))
    destructor_(this);
}

CallbackBase<CopyMode::MoveOnly>::CallbackBase(CallbackBase&& c) = default;

CallbackBase<CopyMode::MoveOnly>&
CallbackBase<CopyMode::MoveOnly>::operator=(CallbackBase&& c) = default;

CallbackBase<CopyMode::MoveOnly>::CallbackBase(
    const CallbackBase<CopyMode::Copyable>& c)
    : bind_state_(c.bind_state_) {}

CallbackBase<CopyMode::MoveOnly>& CallbackBase<CopyMode::MoveOnly>::operator=(
    const CallbackBase<CopyMode::Copyable>& c) {
  bind_state_ = c.bind_state_;
  return *this;
}

void CallbackBase<CopyMode::MoveOnly>::Reset() {
  // NULL the bind_state_ last, since it may be holding the last ref to whatever
  // object owns us, and we may be deleted after that.
  bind_state_ = nullptr;
}

bool CallbackBase<CopyMode::MoveOnly>::IsCancelled() const {
  DCHECK(bind_state_);
  return bind_state_->IsCancelled();
}

bool CallbackBase<CopyMode::MoveOnly>::EqualsInternal(
    const CallbackBase& other) const {
  return bind_state_ == other.bind_state_;
}

CallbackBase<CopyMode::MoveOnly>::CallbackBase(
    BindStateBase* bind_state)
    : bind_state_(bind_state) {
  DCHECK(!bind_state_.get() || bind_state_->ref_count_ == 1);
}

CallbackBase<CopyMode::MoveOnly>::~CallbackBase() {}

CallbackBase<CopyMode::Copyable>::CallbackBase(
    const CallbackBase& c)
    : CallbackBase<CopyMode::MoveOnly>(nullptr) {
  bind_state_ = c.bind_state_;
}

CallbackBase<CopyMode::Copyable>::CallbackBase(CallbackBase&& c) = default;

CallbackBase<CopyMode::Copyable>&
CallbackBase<CopyMode::Copyable>::operator=(const CallbackBase& c) {
  bind_state_ = c.bind_state_;
  return *this;
}

CallbackBase<CopyMode::Copyable>&
CallbackBase<CopyMode::Copyable>::operator=(CallbackBase&& c) = default;

template class CallbackBase<CopyMode::MoveOnly>;
template class CallbackBase<CopyMode::Copyable>;

}  // namespace internal
}  // namespace base
