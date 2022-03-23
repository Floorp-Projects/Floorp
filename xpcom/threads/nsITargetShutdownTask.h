/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_NSITARGETSHUTDOWNTASK_H_
#define XPCOM_THREADS_NSITARGETSHUTDOWNTASK_H_

#include "nsISupports.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"

#define NS_ITARGETSHUTDOWNTASK_IID                   \
  {                                                  \
    0xb08647aa, 0xcfb5, 0x4630, {                    \
      0x8e, 0x26, 0x9a, 0xbe, 0xb3, 0x3f, 0x08, 0x40 \
    }                                                \
  }

class NS_NO_VTABLE nsITargetShutdownTask : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISHUTDOWNTASK_IID)

  virtual void TargetShutdown() = 0;

  already_AddRefed<nsIRunnable> AsRunnable() {
    // FIXME: Try QI to nsINamed if available?
    return mozilla::NewRunnableMethod("nsITargetShutdownTask::TargetShutdown",
                                      this,
                                      &nsITargetShutdownTask::TargetShutdown);
  }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITargetShutdownTask, NS_ITARGETSHUTDOWNTASK_IID)

#endif
