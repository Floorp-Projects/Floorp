/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangDetails_h
#define mozilla_HangDetails_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Move.h"
#include "mozilla/HangTypes.h"
#include "mozilla/HangAnnotations.h"
#include "nsTArray.h"
#include "nsIHangDetails.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

/**
 * HangDetails is the concrete implementaion of nsIHangDetails, and contains the
 * infromation which we want to expose to observers of the bhr-thread-hang
 * observer notification.
 */
class nsHangDetails : public nsIHangDetails
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHANGDETAILS

  explicit nsHangDetails(HangDetails&& aDetails)
    : mDetails(std::move(aDetails))
  {}

  // Submit these HangDetails to the main thread. This will dispatch a runnable
  // to the main thread which will fire off the bhr-thread-hang observer
  // notification with this HangDetails as the subject.
  void Submit();

private:
  virtual ~nsHangDetails() {}

  HangDetails mDetails;
};

/**
 * This runnable is run on the StreamTransportService threadpool in order to
 * process the stack off main thread before submitting it to the main thread as
 * an observer notification.
 *
 * This object should have the only remaining reference to aHangDetails, as it
 * will access its fields without synchronization.
 */
class ProcessHangStackRunnable final : public Runnable
{
public:
  explicit ProcessHangStackRunnable(HangDetails&& aHangDetails)
    : Runnable("ProcessHangStackRunnable")
    , mHangDetails(std::move(aHangDetails))
  {}

  NS_IMETHOD Run() override;

private:
  HangDetails mHangDetails;
};

} // namespace mozilla

#endif // mozilla_HangDetails_h
