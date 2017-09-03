/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilterParent_h
#define mozilla_extensions_StreamFilterParent_h

#include "StreamFilterBase.h"
#include "mozilla/extensions/PStreamFilterParent.h"

#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/WebRequestService.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

#if defined(_MSC_VER)
#  define FUNC __FUNCSIG__
#else
#  define FUNC __PRETTY_FUNCTION__
#endif

namespace mozilla {
namespace dom {
  class nsIContentParent;
}

namespace extensions {

using namespace mozilla::dom;
using mozilla::ipc::IPCResult;

class StreamFilterParent final
  : public PStreamFilterParent
  , public nsISupports
  , public StreamFilterBase
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit StreamFilterParent(uint64_t aChannelId, const nsAString& aAddonId);

  enum class State
  {
    // The parent has been created, but not yet constructed by the child.
    Uninitialized,
    // The parent has been successfully constructed.
    Initialized,
    // The OnRequestStarted event has been received, and data is being
    // transferred to the child.
    TransferringData,
    // The channel is suspended.
    Suspended,
    // The channel has been closed by the child, and will send or receive data.
    Closed,
    // The channel is being disconnected from the child, so that all further
    // data and events pass unfiltered to the output listener. Any data
    // currnetly in transit to, or buffered by, the child will be written to the
    // output listener before we enter the Disconnected atate.
    Disconnecting,
    // The channel has been disconnected from the child, and all further data
    // and events will be passed directly to the output listener.
    Disconnected,
  };

  static already_AddRefed<StreamFilterParent>
  Create(uint64_t aChannelId, const nsAString& aAddonId)
  {
    RefPtr<StreamFilterParent> filter = new StreamFilterParent(aChannelId, aAddonId);
    return filter.forget();
  }

  void Init(already_AddRefed<nsIContentParent> aContentParent);

protected:
  virtual ~StreamFilterParent();

  virtual IPCResult RecvWrite(Data&& aData) override;
  virtual IPCResult RecvFlushedData() override;
  virtual IPCResult RecvSuspend() override;
  virtual IPCResult RecvResume() override;
  virtual IPCResult RecvClose() override;
  virtual IPCResult RecvDisconnect() override;

private:
  bool IPCActive()
  {
    return (mState != State::Closed &&
            mState != State::Disconnecting &&
            mState != State::Disconnected);
  }

  void DoInit(already_AddRefed<nsIContentParent>&& aContentParent);

  nsresult FlushBufferedData();

  nsresult Write(Data& aData);

  void WriteMove(Data&& aData);

  void DoSendData(Data&& aData);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void Broken();

  void
  CheckResult(bool aResult)
  {
    if (NS_WARN_IF(!aResult)) {
      Broken();
    }
  }

  void
  AssertIsPBackgroundThread()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mPBackgroundThread);
  }

  void
  AssertIsIOThread()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mIOThread);
  }

  void
  AssertIsMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  template<typename Function>
  void
  RunOnMainThread(const char* aName, Function&& aFunc)
  {
    SystemGroup::Dispatch(TaskCategory::Network,
                          Move(NS_NewRunnableFunction(aName, aFunc)));
  }

  template<typename Function>
  void
  RunOnPBackgroundThread(const char* aName, Function&& aFunc)
  {
    mPBackgroundThread->Dispatch(Move(NS_NewRunnableFunction(aName, aFunc)),
                                 NS_DISPATCH_NORMAL);
  }

  template<typename Function>
  void
  RunOnIOThread(const char* aName, Function&& aFunc)
  {
    mIOThread->Dispatch(Move(NS_NewRunnableFunction(aName, aFunc)),
                        NS_DISPATCH_NORMAL);
  }

  const uint64_t mChannelId;
  const nsCOMPtr<nsIAtom> mAddonId;

  nsCOMPtr<nsIChannel> mChannel;

  nsCOMPtr<nsIThread> mPBackgroundThread;
  nsCOMPtr<nsIThread> mIOThread;

  Mutex mBufferMutex;

  bool mReceivedStop;
  bool mSentStop;

  volatile State mState;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_StreamFilterParent_h
