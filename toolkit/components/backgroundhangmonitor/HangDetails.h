/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangDetails_h
#define mozilla_HangDetails_h

#include <utility>

#include "ipc/IPCMessageUtils.h"
#include "mozilla/HangAnnotations.h"
#include "mozilla/HangTypes.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/TimeStamp.h"
#include "nsIHangDetails.h"
#include "nsTArray.h"

namespace mozilla {

enum class PersistedToDisk {
  No,
  Yes,
};

/**
 * HangDetails is the concrete implementaion of nsIHangDetails, and contains the
 * infromation which we want to expose to observers of the bhr-thread-hang
 * observer notification.
 */
class nsHangDetails : public nsIHangDetails {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHANGDETAILS

  explicit nsHangDetails(HangDetails&& aDetails,
                         PersistedToDisk aPersistedToDisk)
      : mDetails(std::move(aDetails)), mPersistedToDisk(aPersistedToDisk) {}

  // Submit these HangDetails to the main thread. This will dispatch a runnable
  // to the main thread which will fire off the bhr-thread-hang observer
  // notification with this HangDetails as the subject.
  void Submit();

 private:
  virtual ~nsHangDetails() = default;

  HangDetails mDetails;
  PersistedToDisk mPersistedToDisk;
};

Result<Ok, nsresult> WriteHangDetailsToFile(HangDetails& aDetails,
                                            nsIFile* aFile);

/**
 * This runnable is run on the StreamTransportService threadpool in order to
 * process the stack off main thread before submitting it to the main thread as
 * an observer notification.
 *
 * This object should have the only remaining reference to aHangDetails, as it
 * will access its fields without synchronization.
 */
class ProcessHangStackRunnable final : public Runnable {
 public:
  explicit ProcessHangStackRunnable(HangDetails&& aHangDetails,
                                    PersistedToDisk aPersistedToDisk)
      : Runnable("ProcessHangStackRunnable"),
        mHangDetails(std::move(aHangDetails)),
        mPersistedToDisk(aPersistedToDisk) {}

  NS_IMETHOD Run() override;

 private:
  HangDetails mHangDetails;
  PersistedToDisk mPersistedToDisk;
};

/**
 * This runnable handles checking whether our last session wrote a permahang to
 * disk which we were unable to submit through telemetry. If so, we read the
 * permahang out and try again to submit it.
 */
class SubmitPersistedPermahangRunnable final : public Runnable {
 public:
  explicit SubmitPersistedPermahangRunnable(nsIFile* aPermahangFile)
      : Runnable("SubmitPersistedPermahangRunnable"),
        mPermahangFile(aPermahangFile) {}

  NS_IMETHOD Run() override;

 private:
  nsCOMPtr<nsIFile> mPermahangFile;
};

}  // namespace mozilla

#endif  // mozilla_HangDetails_h
