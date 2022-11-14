/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ViaductRequest_h
#define mozilla_ViaductRequest_h

#include "mozilla/Monitor.h"
#include "mozilla/fetch_msg_types.pb.h"
#include "mozilla/Viaduct.h"

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIStreamListener.h"
#include "nsITimer.h"

namespace mozilla {

// A mapping of the ByteBuffer repr(C) Rust struct.
struct ViaductByteBuffer {
  int64_t len;
  uint8_t* data;
};

class ViaductRequest final : public nsIStreamListener,
                             public nsITimerCallback,
                             public nsINamed,
                             public nsIChannelEventSink {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
  NS_DECL_NSICHANNELEVENTSINK

  ViaductRequest()
      : mDone(false), mChannel(nullptr), mMonitor("ViaductRequest") {}
  ViaductByteBuffer MakeRequest(ViaductByteBuffer reqBuf);

 private:
  nsresult LaunchRequest(appservices::httpconfig::protobuf::Request&);
  void ClearTimers();
  void NotifyMonitor();
  bool mDone;
  nsCOMPtr<nsIChannel> mChannel;
  nsCString mBodyBuffer;
  nsCOMPtr<nsITimer> mConnectTimeoutTimer;
  nsCOMPtr<nsITimer> mReadTimeoutTimer;
  appservices::httpconfig::protobuf::Response mResponse;
  Monitor mMonitor MOZ_UNANNOTATED;
  ~ViaductRequest();
};

};  // namespace mozilla

#endif  // mozilla_ViaductRequest_h
