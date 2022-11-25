/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_BACKGROUNDTASKS_BACKGROUNDTASKSRUNNER_H_
#define TOOLKIT_COMPONENTS_BACKGROUNDTASKS_BACKGROUNDTASKSRUNNER_H_

#include "nsString.h"
#include "nsIBackgroundTasksRunner.h"

namespace mozilla {

class BackgroundTasksRunner final : public nsIBackgroundTasksRunner {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIBACKGROUNDTASKSRUNNER
 protected:
  ~BackgroundTasksRunner() = default;
};

}  // namespace mozilla

#endif  // TOOLKIT_COMPONENTS_BACKGROUNDTASKS_BACKGROUNDTASKSRUNNER_H_
