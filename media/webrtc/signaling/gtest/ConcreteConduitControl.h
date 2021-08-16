/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_WEBRTC_SIGNALING_GTEST_CONCRETECONDUITCONTROL_H_
#define MEDIA_WEBRTC_SIGNALING_GTEST_CONCRETECONDUITCONTROL_H_

#include "MediaConduitControl.h"
#include "WaitFor.h"

namespace mozilla {

#define INIT_CANONICAL(name, val) \
  name(AbstractThread::MainThread(), val, "ConcreteCanonicals::" #name)
class ConcreteCanonicals {
 public:
  ConcreteCanonicals()
      : INIT_CANONICAL(mReceiving, false),
        INIT_CANONICAL(mTransmitting, false),
        INIT_CANONICAL(mLocalSsrcs, Ssrcs()),
        INIT_CANONICAL(mLocalVideoRtxSsrcs, Ssrcs()),
        INIT_CANONICAL(mLocalCname, std::string()),
        INIT_CANONICAL(mLocalMid, std::string()),
        INIT_CANONICAL(mSyncGroup, std::string()),
        INIT_CANONICAL(mLocalSendRtpExtensions, RtpExtList()),
        INIT_CANONICAL(mLocalRecvRtpExtensions, RtpExtList()),
        INIT_CANONICAL(mRemoteSsrc, 0),
        INIT_CANONICAL(mRemoteVideoRtxSsrc, 0),
        INIT_CANONICAL(mAudioRecvCodecs, std::vector<AudioCodecConfig>()),
        INIT_CANONICAL(mAudioSendCodec, Nothing()),
        INIT_CANONICAL(mVideoRecvCodecs, std::vector<VideoCodecConfig>()),
        INIT_CANONICAL(mVideoSendCodec, Nothing()),
        INIT_CANONICAL(mVideoRecvRtpRtcpConfig, Nothing()),
        INIT_CANONICAL(mVideoSendRtpRtcpConfig, Nothing()),
        INIT_CANONICAL(mVideoCodecMode,
                       webrtc::VideoCodecMode::kRealtimeVideo) {}

  Canonical<bool> mReceiving;
  Canonical<bool> mTransmitting;
  Canonical<Ssrcs> mLocalSsrcs;
  Canonical<Ssrcs> mLocalVideoRtxSsrcs;
  Canonical<std::string> mLocalCname;
  Canonical<std::string> mLocalMid;
  Canonical<std::string> mSyncGroup;
  Canonical<RtpExtList> mLocalSendRtpExtensions;
  Canonical<RtpExtList> mLocalRecvRtpExtensions;
  Canonical<Ssrc> mRemoteSsrc;
  Canonical<Ssrc> mRemoteVideoRtxSsrc;

  Canonical<std::vector<AudioCodecConfig>> mAudioRecvCodecs;
  Canonical<Maybe<AudioCodecConfig>> mAudioSendCodec;
  MediaEventProducer<DtmfEvent> mDtmfEvent;

  Canonical<std::vector<VideoCodecConfig>> mVideoRecvCodecs;
  Canonical<Maybe<VideoCodecConfig>> mVideoSendCodec;
  Canonical<Maybe<RtpRtcpConfig>> mVideoRecvRtpRtcpConfig;
  Canonical<Maybe<RtpRtcpConfig>> mVideoSendRtpRtcpConfig;
  Canonical<webrtc::VideoCodecMode> mVideoCodecMode;
};
#undef INIT_CANONICAL

class ConcreteConduitControl : public AudioConduitControlInterface,
                               public VideoConduitControlInterface,
                               private ConcreteCanonicals {
 private:
  RefPtr<nsISerialEventTarget> mTarget;

 public:
  explicit ConcreteConduitControl(RefPtr<nsISerialEventTarget> aTarget)
      : mTarget(std::move(aTarget)) {}

  template <typename Function>
  void Update(Function aFunction) {
    mTarget->Dispatch(NS_NewRunnableFunction(
        __func__, [&] { aFunction(*static_cast<ConcreteCanonicals*>(this)); }));
    WaitForMirrors(mTarget);
  }

  // MediaConduitControlInterface
  AbstractCanonical<bool>* CanonicalReceiving() override { return &mReceiving; }
  AbstractCanonical<bool>* CanonicalTransmitting() override {
    return &mTransmitting;
  }
  AbstractCanonical<Ssrcs>* CanonicalLocalSsrcs() override {
    return &mLocalSsrcs;
  }
  AbstractCanonical<Ssrcs>* CanonicalLocalVideoRtxSsrcs() override {
    return &mLocalVideoRtxSsrcs;
  }
  AbstractCanonical<std::string>* CanonicalLocalCname() override {
    return &mLocalCname;
  }
  AbstractCanonical<std::string>* CanonicalLocalMid() override {
    return &mLocalMid;
  }
  AbstractCanonical<Ssrc>* CanonicalRemoteSsrc() override {
    return &mRemoteSsrc;
  }
  AbstractCanonical<Ssrc>* CanonicalRemoteVideoRtxSsrc() override {
    return &mRemoteVideoRtxSsrc;
  }
  AbstractCanonical<std::string>* CanonicalSyncGroup() override {
    return &mSyncGroup;
  }
  AbstractCanonical<RtpExtList>* CanonicalLocalRecvRtpExtensions() override {
    return &mLocalRecvRtpExtensions;
  }
  AbstractCanonical<RtpExtList>* CanonicalLocalSendRtpExtensions() override {
    return &mLocalSendRtpExtensions;
  }

  // AudioConduitControlInterface
  AbstractCanonical<Maybe<AudioCodecConfig>>* CanonicalAudioSendCodec()
      override {
    return &mAudioSendCodec;
  }
  AbstractCanonical<std::vector<AudioCodecConfig>>* CanonicalAudioRecvCodecs()
      override {
    return &mAudioRecvCodecs;
  }
  MediaEventSource<DtmfEvent>& OnDtmfEvent() override { return mDtmfEvent; }

  // VideoConduitControlInterface
  AbstractCanonical<Maybe<VideoCodecConfig>>* CanonicalVideoSendCodec()
      override {
    return &mVideoSendCodec;
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoSendRtpRtcpConfig()
      override {
    return &mVideoSendRtpRtcpConfig;
  }
  AbstractCanonical<std::vector<VideoCodecConfig>>* CanonicalVideoRecvCodecs()
      override {
    return &mVideoRecvCodecs;
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoRecvRtpRtcpConfig()
      override {
    return &mVideoRecvRtpRtcpConfig;
  }
  AbstractCanonical<webrtc::VideoCodecMode>* CanonicalVideoCodecMode()
      override {
    return &mVideoCodecMode;
  }
};

}  // namespace mozilla

#endif
