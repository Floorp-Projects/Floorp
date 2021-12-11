/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TaskbarProgress_h_
#define TaskbarProgress_h_

#include "nsIGtkTaskbarProgress.h"

class nsWindow;

class TaskbarProgress final : public nsIGtkTaskbarProgress {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGTKTASKBARPROGRESS
  NS_DECL_NSITASKBARPROGRESS

  TaskbarProgress();

 protected:
  ~TaskbarProgress();

  // We track the progress value so we can avoid updating the X window property
  // unnecessarily.
  unsigned long mCurrentProgress;

  RefPtr<nsWindow> mPrimaryWindow;
};

#endif  // #ifndef TaskbarProgress_h_
