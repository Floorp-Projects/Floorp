/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspControllerChild_h
#define RtspControllerChild_h

#include "mozilla/net/PRtspControllerChild.h"
#include "nsIStreamingProtocolController.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/net/RtspChannelChild.h"
#include "mozilla/Mutex.h"
#include "nsITimer.h"

namespace mozilla {
namespace net {

class RtspControllerChild : public nsIStreamingProtocolController
                          , public nsIStreamingProtocolListener
                          , public PRtspControllerChild
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMINGPROTOCOLCONTROLLER
  NS_DECL_NSISTREAMINGPROTOCOLLISTENER

  RtspControllerChild(nsIChannel *channel);
  ~RtspControllerChild();

  bool RecvOnConnected(const uint8_t& index,
                       const InfallibleTArray<RtspMetadataParam>& meta);

  bool RecvOnMediaDataAvailable(
         const uint8_t& index,
         const nsCString& data,
         const uint32_t& length,
         const uint32_t& offset,
         const InfallibleTArray<RtspMetadataParam>& meta);

  bool RecvOnDisconnected(const uint8_t& index,
                          const nsresult& reason);

  bool RecvAsyncOpenFailed(const nsresult& reason);
  void AddIPDLReference();
  void ReleaseIPDLReference();
  void AddMetaData(already_AddRefed<nsIStreamingProtocolMetaData>&& meta);
  int  GetMetaDataLength();
  bool OKToSendIPC();
  void AllowIPC();
  void DisallowIPC();

  // These callbacks will be called when mPlayTimer/mPauseTimer fires.
  static void PlayTimerCallback(nsITimer *aTimer, void *aClosure);
  static void PauseTimerCallback(nsITimer *aTimer, void *aClosure);

 private:
  bool mIPCOpen;
  // The intention of this variable is just to avoid any IPC message to be sent
  // when this flag is set as false. Nothing more.
  bool mIPCAllowed;
  // Dummy channel used to aid MediaResource creation in HTMLMediaElement.
  nsCOMPtr<nsIChannel> mChannel;
  // The nsIStreamingProtocolListener implementation.
  nsCOMPtr<nsIStreamingProtocolListener> mListener;
  // RTSP URL refer to a stream or an aggregate of streams.
  nsCOMPtr<nsIURI> mURI;
  // Array refer to metadata of the media stream.
  nsTArray<nsCOMPtr<nsIStreamingProtocolMetaData>> mMetaArray;
  // ASCII encoded URL spec
  nsCString mSpec;
  // The total tracks for the given media stream session.
  uint32_t mTotalTracks;
  // Current suspension depth for this channel object
  uint32_t mSuspendCount;
  // Detach channel-controller relationship.
  void ReleaseChannel();
  // This lock protects mPlayTimer and mPauseTimer.
  Mutex mTimerLock;
  // Timers to delay the play and pause operations.
  // They are used for optimization and to avoid sending unnecessary requests to
  // the server.
  nsCOMPtr<nsITimer> mPlayTimer;
  nsCOMPtr<nsITimer> mPauseTimer;
  // Timers should be stopped if we are going to terminate, such as when
  // receiving Stop command or OnDisconnected event.
  void StopPlayAndPauseTimer();
};
} // namespace net
} // namespace mozilla

#endif // RtspControllerChild_h
