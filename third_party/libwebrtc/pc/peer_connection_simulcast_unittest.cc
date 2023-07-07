/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/opus_audio_decoder_factory.h"
#include "api/audio_codecs/opus_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtcstats_objects.h"
#include "api/uma_metrics.h"
#include "api/video/video_codec_constants.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/base/media_constants.h"
#include "media/base/rid_description.h"
#include "media/base/stream_params.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "pc/channel_interface.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/sdp_utils.h"
#include "pc/session_description.h"
#include "pc/simulcast_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/test/peer_connection_test_wrapper.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/thread.h"
#include "rtc_base/unique_id_generator.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrCaseEq;
using ::testing::StrEq;

using cricket::MediaContentDescription;
using cricket::RidDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::StreamParams;

namespace cricket {

std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const SimulcastLayer& layer) {
  if (layer.is_paused) {
    os << "~";
  }
  return os << layer.rid;
}

}  // namespace cricket

namespace {

std::vector<SimulcastLayer> CreateLayers(const std::vector<std::string>& rids,
                                         const std::vector<bool>& active) {
  RTC_DCHECK_EQ(rids.size(), active.size());
  std::vector<SimulcastLayer> result;
  absl::c_transform(rids, active, std::back_inserter(result),
                    [](const std::string& rid, bool is_active) {
                      return SimulcastLayer(rid, !is_active);
                    });
  return result;
}
std::vector<SimulcastLayer> CreateLayers(const std::vector<std::string>& rids,
                                         bool active) {
  return CreateLayers(rids, std::vector<bool>(rids.size(), active));
}

#if RTC_METRICS_ENABLED
std::vector<SimulcastLayer> CreateLayers(int num_layers, bool active) {
  rtc::UniqueStringGenerator rid_generator;
  std::vector<std::string> rids;
  for (int i = 0; i < num_layers; ++i) {
    rids.push_back(rid_generator.GenerateString());
  }
  return CreateLayers(rids, active);
}
#endif

// RTX, RED and FEC are reliability mechanisms used in combinations with other
// codecs, but are not themselves a specific codec. Typically you don't want to
// filter these out of the list of codec preferences.
bool IsReliabilityMechanism(const webrtc::RtpCodecCapability& codec) {
  return absl::EqualsIgnoreCase(codec.name, cricket::kRtxCodecName) ||
         absl::EqualsIgnoreCase(codec.name, cricket::kRedCodecName) ||
         absl::EqualsIgnoreCase(codec.name, cricket::kUlpfecCodecName);
}

std::string GetCurrentCodecMimeType(
    rtc::scoped_refptr<const webrtc::RTCStatsReport> report,
    const webrtc::RTCOutboundRtpStreamStats& outbound_rtp) {
  return outbound_rtp.codec_id.is_defined()
             ? *report->GetAs<webrtc::RTCCodecStats>(*outbound_rtp.codec_id)
                    ->mime_type
             : "";
}

struct RidAndResolution {
  std::string rid;
  uint32_t width;
  uint32_t height;
};

const webrtc::RTCOutboundRtpStreamStats* FindOutboundRtpByRid(
    const std::vector<const webrtc::RTCOutboundRtpStreamStats*>& outbound_rtps,
    const absl::string_view& rid) {
  for (const auto* outbound_rtp : outbound_rtps) {
    if (outbound_rtp->rid.is_defined() && *outbound_rtp->rid == rid) {
      return outbound_rtp;
    }
  }
  return nullptr;
}

}  // namespace

namespace webrtc {

constexpr TimeDelta kDefaultTimeout = TimeDelta::Seconds(5);

// Only used by tests disabled under ASAN, avoids unsued variable compile error.
#if !defined(ADDRESS_SANITIZER)
constexpr TimeDelta kLongTimeoutForRampingUp = TimeDelta::Seconds(30);
#endif  // !defined(ADDRESS_SANITIZER)

class PeerConnectionSimulcastTests : public ::testing::Test {
 public:
  PeerConnectionSimulcastTests()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        CreateBuiltinVideoEncoderFactory(),
                                        CreateBuiltinVideoDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      MockPeerConnectionObserver* observer) {
    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    PeerConnectionDependencies pcd(observer);
    auto result =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(pcd));
    EXPECT_TRUE(result.ok());
    observer->SetPeerConnectionInterface(result.value().get());
    return result.MoveValue();
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWrapper() {
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    auto pc = CreatePeerConnection(observer.get());
    return std::make_unique<PeerConnectionWrapper>(pc_factory_, pc,
                                                   std::move(observer));
  }

  void ExchangeOfferAnswer(PeerConnectionWrapper* local,
                           PeerConnectionWrapper* remote,
                           const std::vector<SimulcastLayer>& answer_layers) {
    auto offer = local->CreateOfferAndSetAsLocal();
    // Remove simulcast as the second peer connection won't support it.
    RemoveSimulcast(offer.get());
    std::string err;
    EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &err)) << err;
    auto answer = remote->CreateAnswerAndSetAsLocal();
    // Setup the answer to look like a server response.
    auto mcd_answer = answer->description()->contents()[0].media_description();
    auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
    for (const SimulcastLayer& layer : answer_layers) {
      receive_layers.AddLayer(layer);
    }
    EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &err)) << err;
  }

  RtpTransceiverInit CreateTransceiverInit(
      const std::vector<SimulcastLayer>& layers) {
    RtpTransceiverInit init;
    for (const SimulcastLayer& layer : layers) {
      RtpEncodingParameters encoding;
      encoding.rid = layer.rid;
      encoding.active = !layer.is_paused;
      init.send_encodings.push_back(encoding);
    }
    return init;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      PeerConnectionWrapper* pc,
      const std::vector<SimulcastLayer>& layers,
      cricket::MediaType media_type = cricket::MEDIA_TYPE_VIDEO) {
    auto init = CreateTransceiverInit(layers);
    return pc->AddTransceiver(media_type, init);
  }

  SimulcastDescription RemoveSimulcast(SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    auto result = mcd->simulcast_description();
    mcd->set_simulcast_description(SimulcastDescription());
    return result;
  }

  void AddRequestToReceiveSimulcast(const std::vector<SimulcastLayer>& layers,
                                    SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    SimulcastDescription simulcast;
    auto& receive_layers = simulcast.receive_layers();
    for (const SimulcastLayer& layer : layers) {
      receive_layers.AddLayer(layer);
    }
    mcd->set_simulcast_description(simulcast);
  }

  void ValidateTransceiverParameters(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
      const std::vector<SimulcastLayer>& layers) {
    auto parameters = transceiver->sender()->GetParameters();
    std::vector<SimulcastLayer> result_layers;
    absl::c_transform(parameters.encodings, std::back_inserter(result_layers),
                      [](const RtpEncodingParameters& encoding) {
                        return SimulcastLayer(encoding.rid, !encoding.active);
                      });
    EXPECT_THAT(result_layers, ElementsAreArray(layers));
  }

 private:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

#if RTC_METRICS_ENABLED
// This class is used to test the metrics emitted for simulcast.
class PeerConnectionSimulcastMetricsTests
    : public PeerConnectionSimulcastTests,
      public ::testing::WithParamInterface<int> {
 protected:
  PeerConnectionSimulcastMetricsTests() { webrtc::metrics::Reset(); }

  std::map<int, int> LocalDescriptionSamples() {
    return metrics::Samples(
        "WebRTC.PeerConnection.Simulcast.ApplyLocalDescription");
  }
  std::map<int, int> RemoteDescriptionSamples() {
    return metrics::Samples(
        "WebRTC.PeerConnection.Simulcast.ApplyRemoteDescription");
  }
};
#endif

// Validates that RIDs are supported arguments when adding a transceiver.
TEST_F(PeerConnectionSimulcastTests, CanCreateTransceiverWithRid) {
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f"}, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  ASSERT_TRUE(transceiver);
  auto parameters = transceiver->sender()->GetParameters();
  // Single RID should be removed.
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq(""))));
}

TEST_F(PeerConnectionSimulcastTests, CanCreateTransceiverWithSimulcast) {
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  ASSERT_TRUE(transceiver);
  ValidateTransceiverParameters(transceiver, layers);
}

TEST_F(PeerConnectionSimulcastTests, RidsAreAutogeneratedIfNotProvided) {
  auto pc = CreatePeerConnectionWrapper();
  auto init = CreateTransceiverInit(CreateLayers({"f", "h", "q"}, true));
  for (RtpEncodingParameters& parameters : init.send_encodings) {
    parameters.rid = "";
  }
  auto transceiver = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  auto parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(3u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              Each(Field("rid", &RtpEncodingParameters::rid, Ne(""))));
}

// Validates that an error is returned when there is a mix of supplied and not
// supplied RIDs in a call to AddTransceiver.
TEST_F(PeerConnectionSimulcastTests, MustSupplyAllOrNoRidsInSimulcast) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  // Cannot create a layer with empty RID. Remove the RID after init is created.
  auto layers = CreateLayers({"f", "h", "remove"}, true);
  auto init = CreateTransceiverInit(layers);
  init.send_encodings[2].rid = "";
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.error().type());
}

// Validates that an error is returned when illegal RIDs are supplied.
TEST_F(PeerConnectionSimulcastTests, ChecksForIllegalRidValues) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  auto layers = CreateLayers({"f", "h", "~q"}, true);
  auto init = CreateTransceiverInit(layers);
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.error().type());
}

// Validates that a single RID is removed from the encoding layer.
TEST_F(PeerConnectionSimulcastTests, SingleRidIsRemovedFromSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  auto transceiver = AddTransceiver(pc.get(), CreateLayers({"1"}, true));
  auto offer = pc->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_THAT(contents[0].media_description()->streams(),
              ElementsAre(Property(&StreamParams::has_rids, false)));
}

TEST_F(PeerConnectionSimulcastTests, SimulcastLayersRemovedFromTail) {
  static_assert(
      kMaxSimulcastStreams < 8,
      "Test assumes that the platform does not allow 8 simulcast layers");
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4", "5", "6", "7", "8"}, true);
  std::vector<SimulcastLayer> expected_layers;
  std::copy_n(layers.begin(), kMaxSimulcastStreams,
              std::back_inserter(expected_layers));
  auto transceiver = AddTransceiver(pc.get(), layers);
  ValidateTransceiverParameters(transceiver, expected_layers);
}

// Checks that an offfer to send simulcast contains a SimulcastDescription.
TEST_F(PeerConnectionSimulcastTests, SimulcastAppearsInSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  auto layers = CreateLayers(rids, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  auto offer = pc->CreateOffer();
  ASSERT_TRUE(offer);
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto content = contents[0];
  auto mcd = content.media_description();
  ASSERT_TRUE(mcd->HasSimulcast());
  auto simulcast = mcd->simulcast_description();
  EXPECT_THAT(simulcast.receive_layers(), IsEmpty());
  // The size is validated separately because GetAllLayers() flattens the list.
  EXPECT_THAT(simulcast.send_layers(), SizeIs(3));
  std::vector<SimulcastLayer> result = simulcast.send_layers().GetAllLayers();
  EXPECT_THAT(result, ElementsAreArray(layers));
  auto streams = mcd->streams();
  ASSERT_EQ(1u, streams.size());
  auto stream = streams[0];
  EXPECT_FALSE(stream.has_ssrcs());
  EXPECT_TRUE(stream.has_rids());
  std::vector<std::string> result_rids;
  absl::c_transform(stream.rids(), std::back_inserter(result_rids),
                    [](const RidDescription& rid) { return rid.rid; });
  EXPECT_THAT(result_rids, ElementsAreArray(rids));
}

// Checks that Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, SimulcastLayersAreSetInSender) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  {
    SCOPED_TRACE("after create offer");
    ValidateTransceiverParameters(transceiver, layers);
  }
  // Remove simulcast as the second peer connection won't support it.
  auto simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto simulcast_layers = simulcast.send_layers().GetAllLayers();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  {
    SCOPED_TRACE("after set remote");
    ValidateTransceiverParameters(transceiver, layers);
  }
}

// Checks that paused Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, PausedSimulcastLayersAreDisabledInSender) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, {true, false, true});
  auto server_layers = CreateLayers({"f", "h", "q"}, {true, false, false});
  RTC_DCHECK_EQ(layers.size(), server_layers.size());
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  {
    SCOPED_TRACE("after create offer");
    ValidateTransceiverParameters(transceiver, layers);
  }

  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const SimulcastLayer& layer : server_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  {
    SCOPED_TRACE("after set remote");
    ValidateTransceiverParameters(transceiver, server_layers);
  }
}

// Checks that when Simulcast is not supported by the remote party, then all
// the layers (except the first) are removed.
TEST_F(PeerConnectionSimulcastTests, SimulcastRejectedRemovesExtraLayers) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), {});
  auto parameters = transceiver->sender()->GetParameters();
  // Should only have the first layer.
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq("1"))));
}

// Checks that if Simulcast is supported by remote party, but some layers are
// rejected, then only rejected layers are removed from the sender.
TEST_F(PeerConnectionSimulcastTests, RejectedSimulcastLayersAreDeactivated) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto expected_layers = CreateLayers({"2", "3"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  {
    SCOPED_TRACE("after create offer");
    ValidateTransceiverParameters(transceiver, layers);
  }
  // Remove simulcast as the second peer connection won't support it.
  auto removed_simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  auto mcd_answer = answer->description()->contents()[0].media_description();
  // Setup the answer to look like a server response.
  // Remove one of the layers to reject it in the answer.
  auto simulcast_layers = removed_simulcast.send_layers().GetAllLayers();
  simulcast_layers.erase(simulcast_layers.begin());
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  ASSERT_TRUE(mcd_answer->HasSimulcast());
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  {
    SCOPED_TRACE("after set remote");
    ValidateTransceiverParameters(transceiver, expected_layers);
  }
}

// Checks that simulcast is set up correctly when the server sends an offer
// requesting to receive simulcast.
TEST_F(PeerConnectionSimulcastTests, ServerSendsOfferToReceiveSimulcast) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceiver = remote->pc()->GetTransceivers()[0];
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  ValidateTransceiverParameters(transceiver, layers);
}

// Checks that SetRemoteDescription doesn't attempt to associate a transceiver
// when simulcast is requested by the server.
TEST_F(PeerConnectionSimulcastTests, TransceiverIsNotRecycledWithSimulcast) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  // Call AddTrack so that a transceiver is created.
  remote->AddVideoTrack("fake_track");
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceivers = remote->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  auto transceiver = transceivers[1];
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  ValidateTransceiverParameters(transceiver, layers);
}

// Checks that if the number of layers changes during negotiation, then any
// outstanding get/set parameters transaction is invalidated.
TEST_F(PeerConnectionSimulcastTests, ParametersAreInvalidatedWhenLayersChange) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(3u, parameters.encodings.size());
  // Response will reject simulcast altogether.
  ExchangeOfferAnswer(local.get(), remote.get(), {});
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());
}

// Checks that even though negotiation modifies the sender's parameters, an
// outstanding get/set parameters transaction is not invalidated.
// This test negotiates twice because initial parameters before negotiation
// is missing critical information and cannot be set on the sender.
TEST_F(PeerConnectionSimulcastTests,
       NegotiationDoesNotInvalidateParameterTransactions) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto expected_layers = CreateLayers({"1", "2", "3"}, false);
  auto transceiver = AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), expected_layers);

  // Verify that negotiation does not invalidate the parameters.
  auto parameters = transceiver->sender()->GetParameters();
  ExchangeOfferAnswer(local.get(), remote.get(), expected_layers);

  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_TRUE(result.ok());
  ValidateTransceiverParameters(transceiver, expected_layers);
}

// Tests that simulcast is disabled if the RID extension is not negotiated
// regardless of if the RIDs and simulcast attribute were negotiated properly.
TEST_F(PeerConnectionSimulcastTests, NegotiationDoesNotHaveRidExtension) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto expected_layers = CreateLayers({"1"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string err;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &err)) << err;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  // Setup the answer to look like a server response.
  // Drop the RID header extension.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const SimulcastLayer& layer : layers) {
    receive_layers.AddLayer(layer);
  }
  cricket::RtpHeaderExtensions extensions;
  for (auto extension : mcd_answer->rtp_header_extensions()) {
    if (extension.uri != RtpExtension::kRidUri) {
      extensions.push_back(extension);
    }
  }
  mcd_answer->set_rtp_header_extensions(extensions);
  EXPECT_EQ(layers.size(), mcd_answer->simulcast_description()
                               .receive_layers()
                               .GetAllLayers()
                               .size());
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &err)) << err;
  ValidateTransceiverParameters(transceiver, expected_layers);
}

TEST_F(PeerConnectionSimulcastTests, SimulcastAudioRejected) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4"}, true);
  auto transceiver =
      AddTransceiver(local.get(), layers, cricket::MEDIA_TYPE_AUDIO);
  // Should only have the first layer.
  auto parameters = transceiver->sender()->GetParameters();
  EXPECT_EQ(1u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq(""))));
  ExchangeOfferAnswer(local.get(), remote.get(), {});
  // Still have a single layer after negotiation
  parameters = transceiver->sender()->GetParameters();
  EXPECT_EQ(1u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq(""))));
}

// Check that modifying the offer to remove simulcast and at the same
// time leaving in a RID line does not cause an exception.
TEST_F(PeerConnectionSimulcastTests, SimulcastSldModificationRejected) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOffer();
  std::string as_string;
  EXPECT_TRUE(offer->ToString(&as_string));
  auto simulcast_marker = "a=rid:3 send\r\na=simulcast:send 1;2;3\r\n";
  auto pos = as_string.find(simulcast_marker);
  EXPECT_NE(pos, std::string::npos);
  as_string.erase(pos, strlen(simulcast_marker));
  SdpParseError parse_error;
  auto modified_offer =
      CreateSessionDescription(SdpType::kOffer, as_string, &parse_error);
  EXPECT_TRUE(modified_offer);
  EXPECT_TRUE(local->SetLocalDescription(std::move(modified_offer)));
}

#if RTC_METRICS_ENABLED
//
// Checks the logged metrics when simulcast is not used.
TEST_F(PeerConnectionSimulcastMetricsTests, NoSimulcastUsageIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(0, true);
  AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), layers);

  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
}

// Checks the logged metrics when spec-compliant simulcast is used.
TEST_F(PeerConnectionSimulcastMetricsTests, SpecComplianceIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(3, true);
  AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), layers);

  // Expecting 2 invocations of each, because we have 2 peer connections.
  // Only the local peer connection will be aware of simulcast.
  // The remote peer connection will think that there is no simulcast.
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionSpecCompliant, 1)));
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionSpecCompliant, 1)));
}

// Checks the logged metrics when and incoming request to send spec-compliant
// simulcast is received from the remote party.
TEST_F(PeerConnectionSimulcastMetricsTests, IncomingSimulcastIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(3, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 1)));

  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 1)));

  auto transceiver = remote->pc()->GetTransceivers()[0];
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 2)));
}

// Checks that a spec-compliant simulcast offer that is rejected is logged.
TEST_F(PeerConnectionSimulcastMetricsTests, RejectedSimulcastIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 1)));
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1)));

  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionSpecCompliant, 1)));
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
}

// Checks the logged metrics when legacy munging simulcast technique is used.
TEST_F(PeerConnectionSimulcastMetricsTests, LegacySimulcastIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(0, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOffer();
  // Munge the SDP to set up legacy simulcast.
  const std::string end_line = "\r\n";
  std::string sdp;
  offer->ToString(&sdp);
  rtc::StringBuilder builder(sdp);
  builder << "a=ssrc:1111 cname:slimshady" << end_line;
  builder << "a=ssrc:2222 cname:slimshady" << end_line;
  builder << "a=ssrc:3333 cname:slimshady" << end_line;
  builder << "a=ssrc-group:SIM 1111 2222 3333" << end_line;

  SdpParseError parse_error;
  auto sd =
      CreateSessionDescription(SdpType::kOffer, builder.str(), &parse_error);
  ASSERT_TRUE(sd) << parse_error.line << parse_error.description;
  std::string error;
  EXPECT_TRUE(local->SetLocalDescription(std::move(sd), &error)) << error;
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionLegacy, 1)));
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1)));
  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionLegacy, 1)));
  // Legacy simulcast is not signaled in remote description.
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
}

// Checks that disabling simulcast is logged in the metrics.
TEST_F(PeerConnectionSimulcastMetricsTests, SimulcastDisabledIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.PeerConnection.Simulcast.Disabled"));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.PeerConnection.Simulcast.Disabled", 1));
}

// Checks that the disabled metric is not logged if simulcast is not disabled.
TEST_F(PeerConnectionSimulcastMetricsTests, SimulcastDisabledIsNotLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), layers);

  EXPECT_EQ(0, metrics::NumSamples("WebRTC.PeerConnection.Simulcast.Disabled"));
}

const int kMaxLayersInMetricsTest = 8;

// Checks that the number of send encodings is logged in a metric.
TEST_P(PeerConnectionSimulcastMetricsTests, NumberOfSendEncodingsIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto num_layers = GetParam();
  auto layers = CreateLayers(num_layers, true);
  AddTransceiver(local.get(), layers);
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.PeerConnection.Simulcast.NumberOfSendEncodings"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.PeerConnection.Simulcast.NumberOfSendEncodings",
                   num_layers));
}

INSTANTIATE_TEST_SUITE_P(NumberOfSendEncodings,
                         PeerConnectionSimulcastMetricsTests,
                         ::testing::Range(0, kMaxLayersInMetricsTest));
#endif

// Inherits some helper methods from PeerConnectionSimulcastTests but
// uses real threads and PeerConnectionTestWrapper to create fake media streams
// with flowing media and establish connections.
// TODO(https://crbug.com/webrtc/14884): Move these integration tests into a
// separate file and rename them to PeerConnectionEncodingIntegrationTests.
class PeerConnectionSimulcastWithMediaFlowTests
    : public PeerConnectionSimulcastTests {
 public:
  PeerConnectionSimulcastWithMediaFlowTests()
      : background_thread_(std::make_unique<rtc::Thread>(&pss_)) {
    RTC_CHECK(background_thread_->Start());
  }

  rtc::scoped_refptr<PeerConnectionTestWrapper> CreatePc() {
    auto pc_wrapper = rtc::make_ref_counted<PeerConnectionTestWrapper>(
        "pc", &pss_, background_thread_.get(), background_thread_.get());
    pc_wrapper->CreatePc({}, webrtc::CreateOpusAudioEncoderFactory(),
                         webrtc::CreateOpusAudioDecoderFactory());
    return pc_wrapper;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiverWithSimulcastLayers(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local,
      rtc::scoped_refptr<PeerConnectionTestWrapper> remote,
      std::vector<SimulcastLayer> init_layers) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
        local->GetUserMedia(
            /*audio=*/false, cricket::AudioOptions(), /*video=*/true,
            {.width = 1280, .height = 720});
    rtc::scoped_refptr<VideoTrackInterface> track = stream->GetVideoTracks()[0];

    RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
        transceiver_or_error = local->pc()->AddTransceiver(
            track, CreateTransceiverInit(init_layers));
    EXPECT_TRUE(transceiver_or_error.ok());
    return transceiver_or_error.value();
  }

  bool HasSenderVideoCodecCapability(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      absl::string_view codec_name) {
    std::vector<RtpCodecCapability> codecs =
        pc_wrapper->pc_factory()
            ->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO)
            .codecs;
    return std::find_if(codecs.begin(), codecs.end(),
                        [&codec_name](const RtpCodecCapability& codec) {
                          return absl::EqualsIgnoreCase(codec.name, codec_name);
                        }) != codecs.end();
  }

  std::vector<RtpCodecCapability> GetCapabilitiesAndRestrictToCodec(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      absl::string_view codec_name) {
    std::vector<RtpCodecCapability> codecs =
        pc_wrapper->pc_factory()
            ->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO)
            .codecs;
    codecs.erase(std::remove_if(codecs.begin(), codecs.end(),
                                [&codec_name](const RtpCodecCapability& codec) {
                                  return !IsReliabilityMechanism(codec) &&
                                         !absl::EqualsIgnoreCase(codec.name,
                                                                 codec_name);
                                }),
                 codecs.end());
    RTC_DCHECK(std::find_if(codecs.begin(), codecs.end(),
                            [&codec_name](const RtpCodecCapability& codec) {
                              return absl::EqualsIgnoreCase(codec.name,
                                                            codec_name);
                            }) != codecs.end());
    return codecs;
  }

  void ExchangeIceCandidates(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper,
      rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper) {
    local_pc_wrapper->SignalOnIceCandidateReady.connect(
        remote_pc_wrapper.get(), &PeerConnectionTestWrapper::AddIceCandidate);
    remote_pc_wrapper->SignalOnIceCandidateReady.connect(
        local_pc_wrapper.get(), &PeerConnectionTestWrapper::AddIceCandidate);
  }

  void NegotiateWithSimulcastTweaks(
      rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper,
      rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper,
      std::vector<SimulcastLayer> init_layers) {
    // Create and set offer for `local_pc_wrapper`.
    std::unique_ptr<SessionDescriptionInterface> offer =
        CreateOffer(local_pc_wrapper);
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> p1 =
        SetLocalDescription(local_pc_wrapper, offer.get());
    // Modify the offer before handoff because `remote_pc_wrapper` only supports
    // receiving singlecast.
    SimulcastDescription simulcast_description = RemoveSimulcast(offer.get());
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> p2 =
        SetRemoteDescription(remote_pc_wrapper, offer.get());
    EXPECT_TRUE(Await({p1, p2}));

    // Create and set answer for `remote_pc_wrapper`.
    std::unique_ptr<SessionDescriptionInterface> answer =
        CreateAnswer(remote_pc_wrapper);
    p1 = SetLocalDescription(remote_pc_wrapper, answer.get());
    // Modify the answer before handoff because `local_pc_wrapper` should still
    // send simulcast.
    cricket::MediaContentDescription* mcd_answer =
        answer->description()->contents()[0].media_description();
    mcd_answer->mutable_streams().clear();
    std::vector<SimulcastLayer> simulcast_layers =
        simulcast_description.send_layers().GetAllLayers();
    cricket::SimulcastLayerList& receive_layers =
        mcd_answer->simulcast_description().receive_layers();
    for (const auto& layer : simulcast_layers) {
      receive_layers.AddLayer(layer);
    }
    p2 = SetRemoteDescription(local_pc_wrapper, answer.get());
    EXPECT_TRUE(Await({p1, p2}));
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStats(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto callback = rtc::make_ref_counted<MockRTCStatsCollectorCallback>();
    pc_wrapper->pc()->GetStats(callback.get());
    EXPECT_TRUE_WAIT(callback->called(), kDefaultTimeout.ms());
    return callback->report();
  }

  bool HasOutboundRtpBytesSent(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      size_t num_layers) {
    return HasOutboundRtpBytesSent(pc_wrapper, num_layers, num_layers);
  }

  bool HasOutboundRtpBytesSent(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      size_t num_layers,
      size_t num_active_layers) {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStats(pc_wrapper);
    std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
        report->GetStatsOfType<RTCOutboundRtpStreamStats>();
    if (outbound_rtps.size() != num_layers) {
      return false;
    }
    size_t num_sending_layers = 0;
    for (const auto* outbound_rtp : outbound_rtps) {
      if (outbound_rtp->bytes_sent.is_defined() &&
          *outbound_rtp->bytes_sent > 0u) {
        ++num_sending_layers;
      }
    }
    return num_sending_layers == num_active_layers;
  }

  bool HasOutboundRtpWithRidAndScalabilityMode(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      absl::string_view rid,
      absl::string_view expected_scalability_mode,
      uint32_t frame_height) {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStats(pc_wrapper);
    std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
        report->GetStatsOfType<RTCOutboundRtpStreamStats>();
    auto* outbound_rtp = FindOutboundRtpByRid(outbound_rtps, rid);
    if (!outbound_rtp || !outbound_rtp->scalability_mode.is_defined() ||
        *outbound_rtp->scalability_mode != expected_scalability_mode) {
      return false;
    }
    if (outbound_rtp->frame_height.is_defined()) {
      RTC_LOG(LS_INFO) << "Waiting for target resolution (" << frame_height
                       << "p). Currently at " << *outbound_rtp->frame_height
                       << "p...";
    } else {
      RTC_LOG(LS_INFO)
          << "Waiting for target resolution. No frames encoded yet...";
    }
    if (!outbound_rtp->frame_height.is_defined() ||
        *outbound_rtp->frame_height != frame_height) {
      // Sleep to avoid log spam when this is used in EXPECT_TRUE_WAIT().
      rtc::Thread::Current()->SleepMs(1000);
      return false;
    }
    return true;
  }

  bool OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      std::vector<RidAndResolution> resolutions) {
    rtc::scoped_refptr<const RTCStatsReport> report = GetStats(pc_wrapper);
    std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
        report->GetStatsOfType<RTCOutboundRtpStreamStats>();
    for (const RidAndResolution& resolution : resolutions) {
      const RTCOutboundRtpStreamStats* outbound_rtp = nullptr;
      if (!resolution.rid.empty()) {
        outbound_rtp = FindOutboundRtpByRid(outbound_rtps, resolution.rid);
      } else if (outbound_rtps.size() == 1u) {
        outbound_rtp = outbound_rtps[0];
      }
      if (!outbound_rtp || !outbound_rtp->frame_width.is_defined() ||
          !outbound_rtp->frame_height.is_defined()) {
        // RTP not found by rid or has not encoded a frame yet.
        RTC_LOG(LS_ERROR) << "rid=" << resolution.rid << " does not have "
                          << "resolution metrics";
        return false;
      }
      if (*outbound_rtp->frame_width > resolution.width ||
          *outbound_rtp->frame_height > resolution.height) {
        RTC_LOG(LS_ERROR) << "rid=" << resolution.rid << " is "
                          << *outbound_rtp->frame_width << "x"
                          << *outbound_rtp->frame_height
                          << ", this is greater than the "
                          << "expected " << resolution.width << "x"
                          << resolution.height;
        return false;
      }
    }
    return true;
  }

 protected:
  std::unique_ptr<SessionDescriptionInterface> CreateOffer(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc_wrapper->pc()->CreateOffer(observer.get(), {});
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout.ms());
    return observer->MoveDescription();
  }

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper) {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc_wrapper->pc()->CreateAnswer(observer.get(), {});
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout.ms());
    return observer->MoveDescription();
  }

  rtc::scoped_refptr<MockSetSessionDescriptionObserver> SetLocalDescription(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      SessionDescriptionInterface* sdp) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    pc_wrapper->pc()->SetLocalDescription(
        observer.get(), CloneSessionDescription(sdp).release());
    return observer;
  }

  rtc::scoped_refptr<MockSetSessionDescriptionObserver> SetRemoteDescription(
      rtc::scoped_refptr<PeerConnectionTestWrapper> pc_wrapper,
      SessionDescriptionInterface* sdp) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    pc_wrapper->pc()->SetRemoteDescription(
        observer.get(), CloneSessionDescription(sdp).release());
    return observer;
  }

  // To avoid ICE candidates arriving before the remote endpoint has received
  // the offer it is important to SetLocalDescription() and
  // SetRemoteDescription() are kicked off without awaiting in-between. This
  // helper is used to await multiple observers.
  bool Await(std::vector<rtc::scoped_refptr<MockSetSessionDescriptionObserver>>
                 observers) {
    for (auto& observer : observers) {
      EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout.ms());
      if (!observer->result()) {
        return false;
      }
    }
    return true;
  }

  rtc::PhysicalSocketServer pss_;
  std::unique_ptr<rtc::Thread> background_thread_;
};

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingOneEncodings_VP8_DefaultsToL1T1) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers = CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP8");
  transceiver->SetCodecPreferences(codecs);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T1"));
}

// TODO(https://crbug.com/webrtc/15018): Investigate heap-use-after free during
// shutdown of the test that is flakily happening on bots. It's not only
// happening on ASAN, but it is rare enough on non-ASAN that we don't have to
// disable everywhere.
#if !defined(ADDRESS_SANITIZER)

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_VP8_Simulcast) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP8");
  transceiver->SetCodecPreferences(codecs);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing on all three layers.
  // Ramp up time is needed before all three layers are sending.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u),
                   kLongTimeoutForRampingUp.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"f", 320, 180}, {"h", 640, 360}, {"q", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[1]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[2]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[1]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[2]->scalability_mode, StrEq("L1T3"));
}

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingOneEncoding_VP8_RejectsSVCWhenNotPossibleAndDefaultsToL1T1) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers = CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  // Restricting codecs restricts what SetParameters() will accept or reject.
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP8");
  transceiver->SetCodecPreferences(codecs);
  // Attempt SVC (L3T3_KEY). This is not possible because only VP8 is up for
  // negotiation and VP8 does not support it.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  EXPECT_FALSE(sender->SetParameters(parameters).ok());
  // `scalability_mode` remains unset because SetParameters() failed.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode, Eq(absl::nullopt));

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  // When `scalability_mode` is not set, VP8 defaults to L1T1.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T1"));
  // GetParameters() confirms `scalability_mode` is still not set.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode, Eq(absl::nullopt));
}

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingOneEncoding_VP8_FallbackFromSVCResultsInL1T2) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers = CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  // Verify test assumption that VP8 is first in the list, but don't modify the
  // codec preferences because we want the sender to think SVC is a possibility.
  std::vector<RtpCodecCapability> codecs =
      local_pc_wrapper->pc_factory()
          ->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO)
          .codecs;
  EXPECT_THAT(codecs[0].name, StrCaseEq("VP8"));
  // Attempt SVC (L3T3_KEY), which is not possible with VP8, but the sender does
  // not yet know which codec we'll use so the parameters will be accepted.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  EXPECT_TRUE(sender->SetParameters(parameters).ok());
  // Verify fallback has not happened yet.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L3T3_KEY")));

  // Negotiate, this results in VP8 being picked and fallback happening.
  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();
  // `scalaiblity_mode` is assigned the fallback value "L1T2" which is different
  // than the default of absl::nullopt.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L1T2")));

  // Wait until media is flowing, no significant time needed because we only
  // have one layer.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  // GetStats() confirms "L1T2" is used which is different than the "L1T1"
  // default or the "L3T3_KEY" that was attempted.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP8"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T2"));
}

#if defined(WEBRTC_USE_H264)

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_H264_Simulcast) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "H264");
  transceiver->SetCodecPreferences(codecs);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing on all three layers.
  // Ramp up time is needed before all three layers are sending.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u),
                   kLongTimeoutForRampingUp.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"f", 320, 180}, {"h", 640, 360}, {"q", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/H264"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[1]),
              StrCaseEq("video/H264"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[2]),
              StrCaseEq("video/H264"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[1]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[2]->scalability_mode, StrEq("L1T3"));
}

#endif  // defined(WEBRTC_USE_H264)

// The legacy SVC path is triggered when VP9 us used, but `scalability_mode` has
// not been specified.
// TODO(https://crbug.com/webrtc/14889): When legacy VP9 SVC path has been
// deprecated and removed, update this test to assert that simulcast is used
// (i.e. VP9 is not treated differently than VP8).
TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_VP9_LegacySVC) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing. We only expect a single RTP stream.
  // We expect to see bytes flowing almost immediately on the lowest layer.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  // Wait until scalability mode is reported and expected resolution reached.
  // Ramp up time may be significant.
  EXPECT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(
                       local_pc_wrapper, "f", "L3T3_KEY", 720),
                   (2 * kLongTimeoutForRampingUp).ms());

  // Despite SVC being used on a single RTP stream, GetParameters() returns the
  // three encodings that we configured earlier (this is not spec-compliant but
  // it is how legacy SVC behaves).
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  std::vector<RtpEncodingParameters> encodings =
      sender->GetParameters().encodings;
  ASSERT_EQ(encodings.size(), 3u);
  // When legacy SVC is used, `scalability_mode` is not specified.
  EXPECT_FALSE(encodings[0].scalability_mode.has_value());
  EXPECT_FALSE(encodings[1].scalability_mode.has_value());
  EXPECT_FALSE(encodings[2].scalability_mode.has_value());
}

// The spec-compliant way to configure SVC for a single stream. The expected
// outcome is the same as for the legacy SVC case except that we only have one
// encoding in GetParameters().
TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingOneEncoding_VP9_StandardSVC) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers = CreateLayers({"f"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);
  // Configure SVC, a.k.a. "L3T3_KEY".
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  EXPECT_TRUE(sender->SetParameters(parameters).ok());

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Wait until media is flowing. We only expect a single RTP stream.
  // We expect to see bytes flowing almost immediately on the lowest layer.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 1u),
                   kDefaultTimeout.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(1u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP9"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L3T3_KEY"));

  // GetParameters() is consistent with what we asked for and got.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L3T3_KEY")));
}

// The {active,inactive,inactive} case is technically simulcast but since we
// only have one active stream, we're able to do SVC (multiple spatial layers
// is not supported if multiple encodings are active). The expected outcome is
// the same as above except we end up with two inactive RTP streams which are
// observable in GetStats().
TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_VP9_StandardSVC) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);
  // Configure SVC, a.k.a. "L3T3_KEY".
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  parameters.encodings[0].scalability_mode = "L3T3_KEY";
  parameters.encodings[0].scale_resolution_down_by = 1;
  parameters.encodings[1].active = false;
  parameters.encodings[2].active = false;
  EXPECT_TRUE(sender->SetParameters(parameters).ok());

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Since the standard API is configuring simulcast we get three outbound-rtps,
  // but only one is active.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u, 1u),
                   kDefaultTimeout.ms());
  // Wait until scalability mode is reported and expected resolution reached.
  // Ramp up time is significant.
  EXPECT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(
                       local_pc_wrapper, "f", "L3T3_KEY", 720),
                   (2 * kLongTimeoutForRampingUp).ms());

  // GetParameters() is consistent with what we asked for and got.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L3T3_KEY")));
  EXPECT_FALSE(parameters.encodings[1].scalability_mode.has_value());
  EXPECT_FALSE(parameters.encodings[2].scalability_mode.has_value());
}

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_VP9_Simulcast) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // Opt-in to spec-compliant simulcast by explicitly setting the
  // `scalability_mode` and `scale_resolution_down_by` parameters.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  parameters.encodings[0].scalability_mode = "L1T3";
  parameters.encodings[0].scale_resolution_down_by = 4;
  parameters.encodings[1].scalability_mode = "L1T3";
  parameters.encodings[1].scale_resolution_down_by = 2;
  parameters.encodings[2].scalability_mode = "L1T3";
  parameters.encodings[2].scale_resolution_down_by = 1;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // GetParameters() does not report any fallback.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L1T3")));
  EXPECT_THAT(parameters.encodings[1].scalability_mode,
              Optional(std::string("L1T3")));
  EXPECT_THAT(parameters.encodings[2].scalability_mode,
              Optional(std::string("L1T3")));

  // Wait until media is flowing on all three layers.
  // Ramp up time is needed before all three layers are sending.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u),
                   kLongTimeoutForRampingUp.ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"f", 320, 180}, {"h", 640, 360}, {"q", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/VP9"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[1]),
              StrCaseEq("video/VP9"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[2]),
              StrCaseEq("video/VP9"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[1]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[2]->scalability_mode, StrEq("L1T3"));
}

// Exercise common path where `scalability_mode` is not specified until after
// negotiation, requring us to recreate the stream when the number of streams
// changes from 1 (legacy SVC) to 3 (standard simulcast).
TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_VP9_FromLegacyToSingleActiveWithScalability) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "VP9");
  transceiver->SetCodecPreferences(codecs);

  // The original negotiation triggers legacy SVC because we didn't specify
  // any scalability mode.
  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // Switch to the standard mode. Despite only having a single active stream in
  // both cases, this internally reconfigures from 1 stream to 3 streams.
  // Test coverage for https://crbug.com/webrtc/15016.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  parameters.encodings[0].active = true;
  parameters.encodings[0].scalability_mode = "L2T2_KEY";
  parameters.encodings[0].scale_resolution_down_by = 2.0;
  parameters.encodings[1].active = false;
  parameters.encodings[1].scalability_mode = absl::nullopt;
  parameters.encodings[2].active = false;
  parameters.encodings[2].scalability_mode = absl::nullopt;
  sender->SetParameters(parameters);

  // Since the standard API is configuring simulcast we get three outbound-rtps,
  // but only one is active.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u, 1u),
                   kDefaultTimeout.ms());
  // Wait until scalability mode is reported and expected resolution reached.
  // Ramp up time may be significant.
  EXPECT_TRUE_WAIT(HasOutboundRtpWithRidAndScalabilityMode(
                       local_pc_wrapper, "f", "L2T2_KEY", 720 / 2),
                   (2 * kLongTimeoutForRampingUp).ms());

  // GetParameters() does not report any fallback.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L2T2_KEY")));
  EXPECT_FALSE(parameters.encodings[1].scalability_mode.has_value());
  EXPECT_FALSE(parameters.encodings[2].scalability_mode.has_value());
}

TEST_F(PeerConnectionSimulcastWithMediaFlowTests,
       SendingThreeEncodings_AV1_Simulcast) {
  rtc::scoped_refptr<PeerConnectionTestWrapper> local_pc_wrapper = CreatePc();
  // TODO(https://crbug.com/webrtc/15011): Expand testing support for AV1 or
  // allow compile time checks so that gates like this isn't needed at runtime.
  if (!HasSenderVideoCodecCapability(local_pc_wrapper, "AV1")) {
    RTC_LOG(LS_WARNING) << "\n***\nAV1 is not available, skipping test.\n***";
    return;
  }
  rtc::scoped_refptr<PeerConnectionTestWrapper> remote_pc_wrapper = CreatePc();
  ExchangeIceCandidates(local_pc_wrapper, remote_pc_wrapper);

  std::vector<SimulcastLayer> layers =
      CreateLayers({"f", "h", "q"}, /*active=*/true);
  rtc::scoped_refptr<RtpTransceiverInterface> transceiver =
      AddTransceiverWithSimulcastLayers(local_pc_wrapper, remote_pc_wrapper,
                                        layers);
  std::vector<RtpCodecCapability> codecs =
      GetCapabilitiesAndRestrictToCodec(local_pc_wrapper, "AV1");
  transceiver->SetCodecPreferences(codecs);

  // Opt-in to spec-compliant simulcast by explicitly setting the
  // `scalability_mode`.
  rtc::scoped_refptr<RtpSenderInterface> sender = transceiver->sender();
  RtpParameters parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  parameters.encodings[0].scalability_mode = "L1T3";
  parameters.encodings[0].scale_resolution_down_by = 4;
  parameters.encodings[1].scalability_mode = "L1T3";
  parameters.encodings[1].scale_resolution_down_by = 2;
  parameters.encodings[2].scalability_mode = "L1T3";
  parameters.encodings[2].scale_resolution_down_by = 1;
  sender->SetParameters(parameters);

  NegotiateWithSimulcastTweaks(local_pc_wrapper, remote_pc_wrapper, layers);
  local_pc_wrapper->WaitForConnection();
  remote_pc_wrapper->WaitForConnection();

  // GetParameters() does not report any fallback.
  parameters = sender->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 3u);
  EXPECT_THAT(parameters.encodings[0].scalability_mode,
              Optional(std::string("L1T3")));
  EXPECT_THAT(parameters.encodings[1].scalability_mode,
              Optional(std::string("L1T3")));
  EXPECT_THAT(parameters.encodings[2].scalability_mode,
              Optional(std::string("L1T3")));

  // Wait until media is flowing on all three layers.
  // Ramp up time is needed before all three layers are sending.
  //
  // This test is given 2X timeout because AV1 simulcast ramp-up time is
  // terrible compared to other codecs.
  // TODO(https://crbug.com/webrtc/15006): Improve the ramp-up time and stop
  // giving this test extra long timeout.
  EXPECT_TRUE_WAIT(HasOutboundRtpBytesSent(local_pc_wrapper, 3u),
                   (2 * kLongTimeoutForRampingUp).ms());
  EXPECT_TRUE(OutboundRtpResolutionsAreLessThanOrEqualToExpectations(
      local_pc_wrapper, {{"f", 320, 180}, {"h", 640, 360}, {"q", 1280, 720}}));
  // Verify codec and scalability mode.
  rtc::scoped_refptr<const RTCStatsReport> report = GetStats(local_pc_wrapper);
  std::vector<const RTCOutboundRtpStreamStats*> outbound_rtps =
      report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_THAT(outbound_rtps, SizeIs(3u));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[0]),
              StrCaseEq("video/AV1"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[1]),
              StrCaseEq("video/AV1"));
  EXPECT_THAT(GetCurrentCodecMimeType(report, *outbound_rtps[2]),
              StrCaseEq("video/AV1"));
  EXPECT_THAT(*outbound_rtps[0]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[1]->scalability_mode, StrEq("L1T3"));
  EXPECT_THAT(*outbound_rtps[2]->scalability_mode, StrEq("L1T3"));
}

#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace webrtc
