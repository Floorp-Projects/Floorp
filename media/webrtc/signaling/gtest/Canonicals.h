/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_WEBRTC_SIGNALING_GTEST_CANONICALS_H_
#define MEDIA_WEBRTC_SIGNALING_GTEST_CANONICALS_H_

#include "mozilla/gtest/WaitFor.h"
#include "MediaConduitControl.h"
#include "MediaPipeline.h"

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
        INIT_CANONICAL(mMid, std::string()),
        INIT_CANONICAL(mSyncGroup, std::string()),
        INIT_CANONICAL(mLocalSendRtpExtensions, RtpExtList()),
        INIT_CANONICAL(mLocalRecvRtpExtensions, RtpExtList()),
        INIT_CANONICAL(mRemoteSsrc, 0),
        INIT_CANONICAL(mRemoteVideoRtxSsrc, 0),
        INIT_CANONICAL(mFrameTransformerProxySend, nullptr),
        INIT_CANONICAL(mFrameTransformerProxyRecv, nullptr),
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
  Canonical<std::string> mMid;
  Canonical<std::string> mSyncGroup;
  Canonical<RtpExtList> mLocalSendRtpExtensions;
  Canonical<RtpExtList> mLocalRecvRtpExtensions;
  Canonical<Ssrc> mRemoteSsrc;
  Canonical<Ssrc> mRemoteVideoRtxSsrc;
  Canonical<RefPtr<FrameTransformerProxy>> mFrameTransformerProxySend;
  Canonical<RefPtr<FrameTransformerProxy>> mFrameTransformerProxyRecv;

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

class ConcreteControl : public AudioConduitControlInterface,
                        public VideoConduitControlInterface,
                        public MediaPipelineReceiveControlInterface,
                        public MediaPipelineTransmitControlInterface,
                        private ConcreteCanonicals {
 private:
  RefPtr<nsISerialEventTarget> mTarget;

 public:
  explicit ConcreteControl(RefPtr<nsISerialEventTarget> aTarget)
      : mTarget(std::move(aTarget)) {}

  template <typename Function>
  void Update(Function aFunction) {
    mTarget->Dispatch(NS_NewRunnableFunction(
        __func__, [&] { aFunction(*static_cast<ConcreteCanonicals*>(this)); }));
    WaitForMirrors(mTarget);
  }

  // MediaConduitControlInterface
  // -- MediaPipelineReceiveControlInterface
  Canonical<bool>& CanonicalReceiving() override { return mReceiving; }
  // -- MediaPipelineTransmitControlInterface
  Canonical<bool>& CanonicalTransmitting() override { return mTransmitting; }
  Canonical<Ssrcs>& CanonicalLocalSsrcs() override { return mLocalSsrcs; }
  Canonical<Ssrcs>& CanonicalLocalVideoRtxSsrcs() override {
    return mLocalVideoRtxSsrcs;
  }
  Canonical<std::string>& CanonicalLocalCname() override { return mLocalCname; }
  Canonical<std::string>& CanonicalMid() override { return mMid; }
  Canonical<Ssrc>& CanonicalRemoteSsrc() override { return mRemoteSsrc; }
  Canonical<Ssrc>& CanonicalRemoteVideoRtxSsrc() override {
    return mRemoteVideoRtxSsrc;
  }
  Canonical<std::string>& CanonicalSyncGroup() override { return mSyncGroup; }
  Canonical<RtpExtList>& CanonicalLocalRecvRtpExtensions() override {
    return mLocalRecvRtpExtensions;
  }
  Canonical<RtpExtList>& CanonicalLocalSendRtpExtensions() override {
    return mLocalSendRtpExtensions;
  }
  Canonical<RefPtr<FrameTransformerProxy>>& CanonicalFrameTransformerProxySend()
      override {
    return mFrameTransformerProxySend;
  }
  Canonical<RefPtr<FrameTransformerProxy>>& CanonicalFrameTransformerProxyRecv()
      override {
    return mFrameTransformerProxyRecv;
  }

  // AudioConduitControlInterface
  Canonical<Maybe<AudioCodecConfig>>& CanonicalAudioSendCodec() override {
    return mAudioSendCodec;
  }
  Canonical<std::vector<AudioCodecConfig>>& CanonicalAudioRecvCodecs()
      override {
    return mAudioRecvCodecs;
  }
  MediaEventSource<DtmfEvent>& OnDtmfEvent() override { return mDtmfEvent; }

  // VideoConduitControlInterface
  Canonical<Maybe<VideoCodecConfig>>& CanonicalVideoSendCodec() override {
    return mVideoSendCodec;
  }
  Canonical<Maybe<RtpRtcpConfig>>& CanonicalVideoSendRtpRtcpConfig() override {
    return mVideoSendRtpRtcpConfig;
  }
  Canonical<std::vector<VideoCodecConfig>>& CanonicalVideoRecvCodecs()
      override {
    return mVideoRecvCodecs;
  }
  Canonical<Maybe<RtpRtcpConfig>>& CanonicalVideoRecvRtpRtcpConfig() override {
    return mVideoRecvRtpRtcpConfig;
  }
  Canonical<webrtc::VideoCodecMode>& CanonicalVideoCodecMode() override {
    return mVideoCodecMode;
  }
};

}  // namespace mozilla

#endif
