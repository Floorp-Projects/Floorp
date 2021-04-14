/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VsyncTaskManager_h
#define mozilla_VsyncTaskManager_h

#include "TaskController.h"

namespace mozilla {
class VsyncTaskManager : public TaskManager {
 public:
  static VsyncTaskManager* Get() { return gHighPriorityTaskManager.get(); }
  static void Cleanup() { gHighPriorityTaskManager = nullptr; }
  static void Init();

  void DidRunTask() override;

 private:
  static StaticRefPtr<VsyncTaskManager> gHighPriorityTaskManager;
};
}  // namespace mozilla
#endif
