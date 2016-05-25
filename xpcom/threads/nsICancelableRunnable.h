/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICancelableRunnable_h__
#define nsICancelableRunnable_h__

#include "nsISupports.h"

#define NS_ICANCELABLERUNNABLE_IID \
{ 0xde93dc4c, 0x5eea, 0x4eb7, \
{ 0xb6, 0xd1, 0xdb, 0xf1, 0xe0, 0xce, 0xf6, 0x5c } }

class nsICancelableRunnable : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANCELABLERUNNABLE_IID)

  /*
   * Cancels a pending task.  If the task has already been executed this will
   * be a no-op.  Calling this method twice is considered an error.
   *
   * @throws NS_ERROR_UNEXPECTED
   *   Indicates that the runnable has already been canceled.
   */
  virtual nsresult Cancel() = 0;

protected:
  nsICancelableRunnable() { }
  virtual ~nsICancelableRunnable() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICancelableRunnable,
                              NS_ICANCELABLERUNNABLE_IID)

#endif // nsICancelableRunnable_h__
