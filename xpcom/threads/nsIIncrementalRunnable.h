/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIIncrementalRunnable_h__
#define nsIIncrementalRunnable_h__

#include "nsISupports.h"
#include "mozilla/TimeStamp.h"

#define NS_IINCREMENTALRUNNABLE_IID \
{ 0x688be92e, 0x7ade, 0x4fdc, \
{ 0x9d, 0x83, 0x74, 0xcb, 0xef, 0xf4, 0xa5, 0x2c } }


/**
 * A task interface for tasks that can schedule their work to happen
 * in increments bounded by a deadline.
 */
class nsIIncrementalRunnable : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IINCREMENTALRUNNABLE_IID)

  /**
   * Notify the task of a point in time in the future when the task
   * should stop executing.
   */
  virtual void SetDeadline(mozilla::TimeStamp aDeadline) = 0;

protected:
  nsIIncrementalRunnable() { }
  virtual ~nsIIncrementalRunnable() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIncrementalRunnable,
                              NS_IINCREMENTALRUNNABLE_IID)

#endif // nsIIncrementalRunnable_h__
