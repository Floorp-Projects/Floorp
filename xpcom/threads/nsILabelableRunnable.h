/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsILabelableRunnable_h
#define nsILabelableRunnable_h

#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"

namespace mozilla {
class SchedulerGroup;
}

#define NS_ILABELABLERUNNABLE_IID \
{ 0x40da1ea1, 0x0b81, 0x4249, \
  { 0x96, 0x46, 0x61, 0x92, 0x23, 0x39, 0xc3, 0xe8 } }

// In some cases, it is not possible to assign a SchedulerGroup to a runnable
// when it is dispatched. For example, the vsync runnable affects whichever tabs
// happen to be in the foreground when the vsync runs. The nsILabelableRunnable
// interfaces makes it possible to query a runnable to see what SchedulerGroups
// it could belong to if it ran right now. For vsyncs, for example, this would
// return whichever tabs are foreground at the moment.
//
// This interface should be used very sparingly. In general, it is far
// preferable to label a runnable when it is dispatched since that gives the
// scheduler more flexibility and will improve performance.
//
// To use this interface, QI a runnable to nsILabelableRunnable and then call
// GetAffectedSchedulerGroups.
class nsILabelableRunnable : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILABELABLERUNNABLE_IID);

  // Returns true if the runnable can be labeled right now. In this case,
  // aGroups will contain the set of SchedulerGroups that can be affected by the
  // runnable. If this returns false, no assumptions can be made about which
  // SchedulerGroups are affected by the runnable.
  virtual bool GetAffectedSchedulerGroups(nsTArray<RefPtr<mozilla::SchedulerGroup>>& aGroups) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILabelableRunnable, NS_ILABELABLERUNNABLE_IID);

#endif // nsILabelableRunnable_h
