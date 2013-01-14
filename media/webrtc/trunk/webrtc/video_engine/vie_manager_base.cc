/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "system_wrappers/interface/rw_lock_wrapper.h"
#include "video_engine/vie_manager_base.h"

namespace webrtc {

ViEManagerBase::ViEManagerBase()
    : instance_rwlock_(*RWLockWrapper::CreateRWLock()) {
}

ViEManagerBase::~ViEManagerBase() {
  delete &instance_rwlock_;
}

void ViEManagerBase::ReadLockManager() const {
  instance_rwlock_.AcquireLockShared();
}

void ViEManagerBase::ReleaseLockManager() const {
  instance_rwlock_.ReleaseLockShared();
}

void ViEManagerBase::WriteLockManager() {
  instance_rwlock_.AcquireLockExclusive();
}

void ViEManagerBase::ReleaseWriteLockManager() {
  instance_rwlock_.ReleaseLockExclusive();
}

ViEManagerScopedBase::ViEManagerScopedBase(const ViEManagerBase& ViEManagerBase)
    : vie_manager_(&ViEManagerBase),
      ref_count_(0) {
  vie_manager_->ReadLockManager();
}

ViEManagerScopedBase::~ViEManagerScopedBase() {
  assert(ref_count_ == 0);
  vie_manager_->ReleaseLockManager();
}

ViEManagerWriteScoped::ViEManagerWriteScoped(ViEManagerBase* vie_manager)
    : vie_manager_(vie_manager) {
  vie_manager_->WriteLockManager();
}

ViEManagerWriteScoped::~ViEManagerWriteScoped() {
  vie_manager_->ReleaseWriteLockManager();
}

ViEManagedItemScopedBase::ViEManagedItemScopedBase(
    ViEManagerScopedBase* vie_scoped_manager)
    : vie_scoped_manager_(vie_scoped_manager) {
  vie_scoped_manager_->ref_count_++;
}

ViEManagedItemScopedBase::~ViEManagedItemScopedBase() {
  vie_scoped_manager_->ref_count_--;
}

}  // namespace webrtc
