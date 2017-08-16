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
#include "mozilla/HangStack.h"
#include "mozilla/HangAnnotations.h"
#include "nsTArray.h"
#include "nsIHangDetails.h"

namespace mozilla {

/**
 * HangDetails is a POD struct which contains the information collected from the
 * hang. It can be wrapped in a nsHangDetails to provide an XPCOM interface for
 * extracting information from it easily.
 *
 * This type is separate, as it can be sent over IPC while nsHangDetails is an
 * XPCOM interface which is harder to serialize over IPC.
 */
class HangDetails
{
public:
  HangDetails()
    : mDuration(0)
    , mProcess(GeckoProcessType_Invalid)
  {}

  HangDetails(const HangDetails& aOther) = default;
  HangDetails(HangDetails&& aOther) = default;
  HangDetails(uint32_t aDuration,
              GeckoProcessType aProcess,
              const nsACString& aThreadName,
              const nsACString& aRunnableName,
              HangStack&& aPseudoStack,
              HangMonitor::HangAnnotations&& aAnnotations)
    : mDuration(aDuration)
    , mProcess(aProcess)
    , mThreadName(aThreadName)
    , mRunnableName(aRunnableName)
    , mPseudoStack(Move(aPseudoStack))
    , mAnnotations(Move(aAnnotations))
  {}

  // NOTE: Once we get merged stacks, we will move ProcessedStack into HangStack
  // and simplify everything. (bug 1367406).
  uint32_t mDuration;
  GeckoProcessType mProcess;
  nsCString mThreadName;
  nsCString mRunnableName;
  HangStack mPseudoStack;
  HangMonitor::HangAnnotations mAnnotations;

  // NOTE: Initialized by ProcessHangStackRunnable.
  Telemetry::ProcessedStack mStack;
};

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
    : mDetails(Move(aDetails))
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
  ProcessHangStackRunnable(HangDetails&& aHangDetails,
                           std::vector<uintptr_t>&& aNativeStack)
    : Runnable("ProcessHangStackRunnable")
    , mHangDetails(Move(aHangDetails))
    , mNativeStack(Move(aNativeStack))
  {}

  NS_IMETHOD Run() override;

private:
  HangDetails mHangDetails;
  std::vector<uintptr_t> mNativeStack;
};

} // namespace mozilla

// We implement the ability to send the HangDetails object over IPC. We need to
// do this rather than rely on StructuredClone of the objects created by the
// XPCOM getters on nsHangDetails because we want to run BHR in the GPU process
// which doesn't run any JS.
namespace IPC {

template<>
class ParamTraits<mozilla::HangDetails>
{
public:
  typedef mozilla::HangDetails paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   paramType* aResult);
};

} // namespace IPC

#endif // mozilla_HangDetails_h
