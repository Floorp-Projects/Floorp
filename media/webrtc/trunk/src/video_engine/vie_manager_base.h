/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_MANAGER_BASE_H_
#define WEBRTC_VIDEO_ENGINE_VIE_MANAGER_BASE_H_

namespace webrtc {

class RWLockWrapper;

class ViEManagerBase {
  friend class ViEManagerScopedBase;
  friend class ViEManagedItemScopedBase;
  friend class ViEManagerWriteScoped;
 public:
  ViEManagerBase();
  ~ViEManagerBase();

 private:
  // Exclusive lock, used by ViEManagerWriteScoped
  void WriteLockManager();

  // Releases exclusive lock, used by ViEManagerWriteScoped.
  void ReleaseWriteLockManager();

  // Increases lock count, used by ViEManagerScopedBase.
  void ReadLockManager() const;

  // Releases the lock count, used by ViEManagerScopedBase.
  void ReleaseLockManager() const;

  RWLockWrapper& instance_rwlock_;
};

class ViEManagerWriteScoped {
 public:
  explicit ViEManagerWriteScoped(ViEManagerBase& vie_manager);
  ~ViEManagerWriteScoped();

 private:
  ViEManagerBase* vie_manager_;
};

class ViEManagerScopedBase {
  friend class ViEManagedItemScopedBase;
 public:
  explicit ViEManagerScopedBase(const ViEManagerBase& vie_manager);
  ~ViEManagerScopedBase();

 protected:
  const ViEManagerBase* vie_manager_;

 private:
  int ref_count_;
};

class ViEManagedItemScopedBase {
 public:
  explicit ViEManagedItemScopedBase(ViEManagerScopedBase& vie_scoped_manager);
  ~ViEManagedItemScopedBase();
 protected:
  ViEManagerScopedBase& vie_scoped_manager_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_MANAGER_BASE_H_
