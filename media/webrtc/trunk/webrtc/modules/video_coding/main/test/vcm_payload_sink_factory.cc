/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/vcm_payload_sink_factory.h"

#include <algorithm>
#include <cassert>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {
namespace rtpplayer {

class VcmPayloadSinkFactory::VcmPayloadSink
    : public PayloadSinkInterface,
      public VCMPacketRequestCallback,
      public VCMFrameStorageCallback {
 public:
  VcmPayloadSink(VcmPayloadSinkFactory* factory,
                 RtpStreamInterface* stream,
                 scoped_ptr<VideoCodingModule>* vcm,
                 scoped_ptr<VideoCodingModule>* vcm_playback,
                 scoped_ptr<FileOutputFrameReceiver>* frame_receiver)
      : factory_(factory),
        stream_(stream),
        vcm_(),
        vcm_playback_(),
        frame_receiver_() {
    assert(factory);
    assert(stream);
    assert(vcm);
    assert(vcm->get());
    assert(vcm_playback);
    assert(frame_receiver);
    assert(frame_receiver->get());
    vcm_.swap(*vcm);
    vcm_playback_.swap(*vcm_playback);
    frame_receiver_.swap(*frame_receiver);
    vcm_->RegisterPacketRequestCallback(this);
    if (vcm_playback_.get() == NULL) {
      vcm_->RegisterReceiveCallback(frame_receiver_.get());
    } else {
      vcm_->RegisterFrameStorageCallback(this);
      vcm_playback_->RegisterReceiveCallback(frame_receiver_.get());
    }
  }

  virtual ~VcmPayloadSink() {
    factory_->Remove(this);
  }

  // PayloadSinkInterface
  virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_size,
      const WebRtcRTPHeader* rtp_header) {
    return vcm_->IncomingPacket(payload_data, payload_size, *rtp_header);
  }

  // VCMPacketRequestCallback
  virtual WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequence_numbers,
                                      WebRtc_UWord16 length) {
    stream_->ResendPackets(sequence_numbers, length);
    return 0;
  }

  // VCMFrameStorageCallback
  virtual WebRtc_Word32 StoreReceivedFrame(
      const EncodedVideoData& frame_to_store) {
    vcm_playback_->DecodeFromStorage(frame_to_store);
    return VCM_OK;
  }

  int DecodeAndProcess(bool should_decode, bool decode_dual_frame) {
    if (should_decode) {
      if (vcm_->Decode() < 0) {
        return -1;
      }
    }
    while (decode_dual_frame && vcm_->DecodeDualFrame(0) == 1) {
    }
    return Process() ? 0 : -1;
  }

  bool Process() {
    if (vcm_->TimeUntilNextProcess() <= 0) {
      if (vcm_->Process() < 0) {
        return false;
      }
    }
    return true;
  }

  bool Decode() {
    vcm_->Decode(10000);
    while (vcm_->DecodeDualFrame(0) == 1) {
    }
    return true;
  }

 private:
  VcmPayloadSinkFactory* factory_;
  RtpStreamInterface* stream_;
  scoped_ptr<VideoCodingModule> vcm_;
  scoped_ptr<VideoCodingModule> vcm_playback_;
  scoped_ptr<FileOutputFrameReceiver> frame_receiver_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VcmPayloadSink);
};

VcmPayloadSinkFactory::VcmPayloadSinkFactory(
    const std::string& base_out_filename, Clock* clock, bool protection_enabled,
    VCMVideoProtection protection_method, uint32_t rtt_ms,
    uint32_t render_delay_ms, uint32_t min_playout_delay_ms,
    bool use_frame_storage)
    : base_out_filename_(base_out_filename),
      clock_(clock),
      protection_enabled_(protection_enabled),
      protection_method_(protection_method),
      rtt_ms_(rtt_ms),
      render_delay_ms_(render_delay_ms),
      min_playout_delay_ms_(min_playout_delay_ms),
      use_frame_storage_(use_frame_storage),
      null_event_factory_(new NullEventFactory()),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      sinks_(),
      next_id_(1) {
  assert(clock);
  assert(crit_sect_.get());
}

VcmPayloadSinkFactory::~VcmPayloadSinkFactory() {
  assert(sinks_.empty());
}

PayloadSinkInterface* VcmPayloadSinkFactory::Create(
    RtpStreamInterface* stream) {
  assert(stream);
  CriticalSectionScoped cs(crit_sect_.get());

  scoped_ptr<VideoCodingModule> vcm(
      VideoCodingModule::Create(next_id_++, clock_, null_event_factory_.get()));
  if (vcm.get() == NULL) {
    return NULL;
  }
  if (vcm->InitializeReceiver() < 0) {
    return NULL;
  }

  scoped_ptr<VideoCodingModule> vcm_playback;
  if (use_frame_storage_) {
    vcm_playback.reset(
        VideoCodingModule::Create(next_id_++, clock_,
                                  null_event_factory_.get()));
    if (vcm_playback.get() == NULL) {
      return NULL;
    }
    if (vcm_playback->InitializeReceiver() < 0) {
      return NULL;
    }
  }

  const PayloadTypes& plt = stream->payload_types();
  for (PayloadTypesIterator it = plt.begin(); it != plt.end();
      ++it) {
    if (it->codec_type() != kVideoCodecULPFEC &&
        it->codec_type() != kVideoCodecRED) {
      VideoCodec codec;
      if (VideoCodingModule::Codec(it->codec_type(), &codec) < 0) {
        return NULL;
      }
      codec.plType = it->payload_type();
      if (vcm->RegisterReceiveCodec(&codec, 1) < 0) {
        return NULL;
      }
      if (use_frame_storage_) {
        if (vcm_playback->RegisterReceiveCodec(&codec, 1) < 0) {
          return NULL;
        }
      }
    }
  }

  vcm->SetChannelParameters(0, 0, rtt_ms_);
  vcm->SetVideoProtection(protection_method_, protection_enabled_);
  vcm->SetRenderDelay(render_delay_ms_);
  vcm->SetMinimumPlayoutDelay(min_playout_delay_ms_);
  vcm->SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack);

  scoped_ptr<FileOutputFrameReceiver> frame_receiver(
      new FileOutputFrameReceiver(base_out_filename_, stream->ssrc()));
  scoped_ptr<VcmPayloadSink> sink(
      new VcmPayloadSink(this, stream, &vcm, &vcm_playback, &frame_receiver));

  sinks_.push_back(sink.get());
  return sink.release();
}

int VcmPayloadSinkFactory::DecodeAndProcessAll(bool decode_dual_frame) {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(clock_);
  bool should_decode = (clock_->TimeInMilliseconds() % 5) == 0;
  for (Sinks::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if ((*it)->DecodeAndProcess(should_decode, decode_dual_frame) < 0) {
      return -1;
    }
  }
  return 0;
}

bool VcmPayloadSinkFactory::ProcessAll() {
  CriticalSectionScoped cs(crit_sect_.get());
  for (Sinks::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (!(*it)->Process()) {
      return false;
    }
  }
  return true;
}

bool VcmPayloadSinkFactory::DecodeAll() {
  CriticalSectionScoped cs(crit_sect_.get());
  for (Sinks::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (!(*it)->Decode()) {
      return false;
    }
  }
  return true;
}

void VcmPayloadSinkFactory::Remove(VcmPayloadSink* sink) {
  assert(sink);
  CriticalSectionScoped cs(crit_sect_.get());
  Sinks::iterator it = std::find(sinks_.begin(), sinks_.end(), sink);
  assert(it != sinks_.end());
  sinks_.erase(it);
}

}  // namespace rtpplayer
}  // namespace webrtc
