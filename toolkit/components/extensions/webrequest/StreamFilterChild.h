/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilterChild_h
#define mozilla_extensions_StreamFilterChild_h

#include "StreamFilterBase.h"
#include "mozilla/extensions/PStreamFilterChild.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace extensions {

using mozilla::ipc::IPCResult;

class StreamFilter;
class StreamFilterChild final : public PStreamFilterChild
                              , public StreamFilterBase
{
public:
  NS_INLINE_DECL_REFCOUNTING(StreamFilterChild)

  StreamFilterChild()
    : mState(State::Uninitialized)
    , mReceivedOnStop(false)
  {}

  enum class State {
    // Uninitialized, waiting for constructor response from parent.
    Uninitialized,
    // Initialized, but channel has not begun transferring data.
    Initialized,
    // The stream's OnStartRequest event has been dispatched, and the channel is
    // transferring data.
    TransferringData,
    // The channel's OnStopRequest event has been dispatched, and the channel is
    // no longer transferring data. Data may still be written to the output
    // stream listener.
    FinishedTransferringData,
    // The channel is being suspended, and we're waiting for confirmation of
    // suspension from the parent.
    Suspending,
    // The channel has been suspended in the parent. Data may still be written
    // to the output stream listener in this state.
    Suspended,
    // The channel is suspended. Resume has been called, and we are waiting for
    // confirmation of resumption from the parent.
    Resuming,
    // The close() method has been called, and no further output may be written.
    // We are waiting for confirmation from the parent.
    Closing,
    // The close() method has been called, and we have been disconnected from
    // our parent.
    Closed,
    // The channel is being disconnected from the parent, and all further events
    // and data will pass unfiltered. Data received by the child in this state
    // will be automatically written ot the output stream listener. No data may
    // be explicitly written.
    Disconnecting,
    // The channel has been disconnected from the parent, and all further data
    // and events will be transparently passed to the output stream listener
    // without passing through the child.
    Disconnected,
    // An error has occurred and the child is disconnected from the parent.
    Error,
  };

  void Suspend(ErrorResult& aRv);
  void Resume(ErrorResult& aRv);
  void Disconnect(ErrorResult& aRv);
  void Close(ErrorResult& aRv);
  void Cleanup();

  void Write(Data&& aData, ErrorResult& aRv);

  State GetState() const
  {
    return mState;
  }

protected:
  virtual IPCResult RecvInitialized(const bool& aSuccess) override;

  virtual IPCResult RecvStartRequest() override;
  virtual IPCResult RecvData(Data&& data) override;
  virtual IPCResult RecvStopRequest(const nsresult& aStatus) override;

  virtual IPCResult RecvClosed() override;
  virtual IPCResult RecvSuspended() override;
  virtual IPCResult RecvResumed() override;
  virtual IPCResult RecvFlushData() override;

  virtual IPCResult Recv__delete__() override { return IPC_OK(); }

private:
  ~StreamFilterChild() {}

  void SetNextState();

  void MaybeStopRequest();

  void EmitData(const Data& aData);

  bool
  CanFlushData()
  {
    return (mState == State::TransferringData ||
            mState == State::Resuming);
  }

  void FlushBufferedData();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;


  State mState;
  State mNextState;
  bool mReceivedOnStop;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_StreamFilterChild_h
