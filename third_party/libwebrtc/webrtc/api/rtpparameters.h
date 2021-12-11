/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTPPARAMETERS_H_
#define API_RTPPARAMETERS_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "api/mediatypes.h"
#include "api/optional.h"

namespace webrtc {

// These structures are intended to mirror those defined by:
// http://draft.ortc.org/#rtcrtpdictionaries*
// Contains everything specified as of 2017 Jan 24.
//
// They are used when retrieving or modifying the parameters of an
// RtpSender/RtpReceiver, or retrieving capabilities.
//
// Note on conventions: Where ORTC may use "octet", "short" and "unsigned"
// types, we typically use "int", in keeping with our style guidelines. The
// parameter's actual valid range will be enforced when the parameters are set,
// rather than when the parameters struct is built. An exception is made for
// SSRCs, since they use the full unsigned 32-bit range, and aren't expected to
// be used for any numeric comparisons/operations.
//
// Additionally, where ORTC uses strings, we may use enums for things that have
// a fixed number of supported values. However, for things that can be extended
// (such as codecs, by providing an external encoder factory), a string
// identifier is used.

enum class FecMechanism {
  RED,
  RED_AND_ULPFEC,
  FLEXFEC,
};

// Used in RtcpFeedback struct.
enum class RtcpFeedbackType {
  CCM,
  NACK,
  REMB,  // "goog-remb"
  TRANSPORT_CC,
};

// Used in RtcpFeedback struct when type is NACK or CCM.
enum class RtcpFeedbackMessageType {
  // Equivalent to {type: "nack", parameter: undefined} in ORTC.
  GENERIC_NACK,
  PLI,  // Usable with NACK.
  FIR,  // Usable with CCM.
};

enum class DtxStatus {
  DISABLED,
  ENABLED,
};

enum class DegradationPreference {
  MAINTAIN_FRAMERATE,
  MAINTAIN_RESOLUTION,
  BALANCED,
};

enum class PriorityType { VERY_LOW, LOW, MEDIUM, HIGH };

struct RtcpFeedback {
  RtcpFeedbackType type = RtcpFeedbackType::CCM;

  // Equivalent to ORTC "parameter" field with slight differences:
  // 1. It's an enum instead of a string.
  // 2. Generic NACK feedback is represented by a GENERIC_NACK message type,
  //    rather than an unset "parameter" value.
  rtc::Optional<RtcpFeedbackMessageType> message_type;

  // Constructors for convenience.
  RtcpFeedback();
  explicit RtcpFeedback(RtcpFeedbackType type);
  RtcpFeedback(RtcpFeedbackType type, RtcpFeedbackMessageType message_type);
  ~RtcpFeedback();

  bool operator==(const RtcpFeedback& o) const {
    return type == o.type && message_type == o.message_type;
  }
  bool operator!=(const RtcpFeedback& o) const { return !(*this == o); }
};

// RtpCodecCapability is to RtpCodecParameters as RtpCapabilities is to
// RtpParameters. This represents the static capabilities of an endpoint's
// implementation of a codec.
struct RtpCodecCapability {
  RtpCodecCapability();
  ~RtpCodecCapability();

  // Build MIME "type/subtype" string from |name| and |kind|.
  std::string mime_type() const { return MediaTypeToString(kind) + "/" + name; }

  // Used to identify the codec. Equivalent to MIME subtype.
  std::string name;

  // The media type of this codec. Equivalent to MIME top-level type.
  cricket::MediaType kind = cricket::MEDIA_TYPE_AUDIO;

  // Clock rate in Hertz. If unset, the codec is applicable to any clock rate.
  rtc::Optional<int> clock_rate;

  // Default payload type for this codec. Mainly needed for codecs that use
  // that have statically assigned payload types.
  rtc::Optional<int> preferred_payload_type;

  // Maximum packetization time supported by an RtpReceiver for this codec.
  // TODO(deadbeef): Not implemented.
  rtc::Optional<int> max_ptime;

  // Preferred packetization time for an RtpReceiver or RtpSender of this
  // codec.
  // TODO(deadbeef): Not implemented.
  rtc::Optional<int> ptime;

  // The number of audio channels supported. Unused for video codecs.
  rtc::Optional<int> num_channels;

  // Feedback mechanisms supported for this codec.
  std::vector<RtcpFeedback> rtcp_feedback;

  // Codec-specific parameters that must be signaled to the remote party.
  //
  // Corresponds to "a=fmtp" parameters in SDP.
  //
  // Contrary to ORTC, these parameters are named using all lowercase strings.
  // This helps make the mapping to SDP simpler, if an application is using
  // SDP. Boolean values are represented by the string "1".
  std::unordered_map<std::string, std::string> parameters;

  // Codec-specific parameters that may optionally be signaled to the remote
  // party.
  // TODO(deadbeef): Not implemented.
  std::unordered_map<std::string, std::string> options;

  // Maximum number of temporal layer extensions supported by this codec.
  // For example, a value of 1 indicates that 2 total layers are supported.
  // TODO(deadbeef): Not implemented.
  int max_temporal_layer_extensions = 0;

  // Maximum number of spatial layer extensions supported by this codec.
  // For example, a value of 1 indicates that 2 total layers are supported.
  // TODO(deadbeef): Not implemented.
  int max_spatial_layer_extensions = 0;

  // Whether the implementation can send/receive SVC layers with distinct
  // SSRCs. Always false for audio codecs. True for video codecs that support
  // scalable video coding with MRST.
  // TODO(deadbeef): Not implemented.
  bool svc_multi_stream_support = false;

  bool operator==(const RtpCodecCapability& o) const {
    return name == o.name && kind == o.kind && clock_rate == o.clock_rate &&
           preferred_payload_type == o.preferred_payload_type &&
           max_ptime == o.max_ptime && ptime == o.ptime &&
           num_channels == o.num_channels && rtcp_feedback == o.rtcp_feedback &&
           parameters == o.parameters && options == o.options &&
           max_temporal_layer_extensions == o.max_temporal_layer_extensions &&
           max_spatial_layer_extensions == o.max_spatial_layer_extensions &&
           svc_multi_stream_support == o.svc_multi_stream_support;
  }
  bool operator!=(const RtpCodecCapability& o) const { return !(*this == o); }
};

// Used in RtpCapabilities; represents the capabilities/preferences of an
// implementation for a header extension.
//
// Just called "RtpHeaderExtension" in ORTC, but the "Capability" suffix was
// added here for consistency and to avoid confusion with
// RtpHeaderExtensionParameters.
//
// Note that ORTC includes a "kind" field, but we omit this because it's
// redundant; if you call "RtpReceiver::GetCapabilities(MEDIA_TYPE_AUDIO)",
// you know you're getting audio capabilities.
struct RtpHeaderExtensionCapability {
  // URI of this extension, as defined in RFC5285.
  std::string uri;

  // Preferred value of ID that goes in the packet.
  rtc::Optional<int> preferred_id;

  // If true, it's preferred that the value in the header is encrypted.
  // TODO(deadbeef): Not implemented.
  bool preferred_encrypt = false;

  // Constructors for convenience.
  RtpHeaderExtensionCapability();
  explicit RtpHeaderExtensionCapability(const std::string& uri);
  RtpHeaderExtensionCapability(const std::string& uri, int preferred_id);
  ~RtpHeaderExtensionCapability();

  bool operator==(const RtpHeaderExtensionCapability& o) const {
    return uri == o.uri && preferred_id == o.preferred_id &&
           preferred_encrypt == o.preferred_encrypt;
  }
  bool operator!=(const RtpHeaderExtensionCapability& o) const {
    return !(*this == o);
  }
};

// RTP header extension, see RFC 5285.
struct RtpExtension {
  RtpExtension();
  RtpExtension(const std::string& uri, int id);
  RtpExtension(const std::string& uri, int id, bool encrypt);
  ~RtpExtension();
  std::string ToString() const;
  bool operator==(const RtpExtension& rhs) const {
    return uri == rhs.uri && id == rhs.id && encrypt == rhs.encrypt;
  }
  static bool IsSupportedForAudio(const std::string& uri);
  static bool IsSupportedForVideo(const std::string& uri);
  // Return "true" if the given RTP header extension URI may be encrypted.
  static bool IsEncryptionSupported(const std::string& uri);

  // Returns the named header extension if found among all extensions,
  // nullptr otherwise.
  static const RtpExtension* FindHeaderExtensionByUri(
      const std::vector<RtpExtension>& extensions,
      const std::string& uri);

  // Return a list of RTP header extensions with the non-encrypted extensions
  // removed if both the encrypted and non-encrypted extension is present for
  // the same URI.
  static std::vector<RtpExtension> FilterDuplicateNonEncrypted(
      const std::vector<RtpExtension>& extensions);

  // Header extension for audio levels, as defined in:
  // http://tools.ietf.org/html/draft-ietf-avtext-client-to-mixer-audio-level-03
  static const char kAudioLevelUri[];
  static const int kAudioLevelDefaultId;

  // Header extension for RTP timestamp offset, see RFC 5450 for details:
  // http://tools.ietf.org/html/rfc5450
  static const char kTimestampOffsetUri[];
  static const int kTimestampOffsetDefaultId;

  // Header extension for absolute send time, see url for details:
  // http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
  static const char kAbsSendTimeUri[];
  static const int kAbsSendTimeDefaultId;

  // Header extension for coordination of video orientation, see url for
  // details:
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/ts_126114v120700p.pdf
  static const char kVideoRotationUri[];
  static const int kVideoRotationDefaultId;

  // Header extension for video content type. E.g. default or screenshare.
  static const char kVideoContentTypeUri[];
  static const int kVideoContentTypeDefaultId;

  // Header extension for video timing.
  static const char kVideoTimingUri[];
  static const int kVideoTimingDefaultId;

  // Header extension for transport sequence number, see url for details:
  // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions
  static const char kTransportSequenceNumberUri[];
  static const int kTransportSequenceNumberDefaultId;

  static const char kPlayoutDelayUri[];
  static const int kPlayoutDelayDefaultId;

  // Encryption of Header Extensions, see RFC 6904 for details:
  // https://tools.ietf.org/html/rfc6904
  static const char kEncryptHeaderExtensionsUri[];

  static const char* kRtpStreamIdUri;
  static const int kRtpStreamIdDefaultId;

  static const char* kRepairedRtpStreamIdUri;
  static const int kRepairedRtpStreamIdDefaultId;

  static const char* kMIdUri;
  static const int kMIdDefaultId;

  static const char* kCsrcAudioLevelUri;
  static const int kCsrcAudioLevelDefaultId;

  // Inclusive min and max IDs for one-byte header extensions, per RFC5285.
  static const int kMinId;
  static const int kMaxId;

  std::string uri;
  int id = 0;
  bool encrypt = false;
};

// TODO(deadbeef): This is missing the "encrypt" flag, which is unimplemented.
typedef RtpExtension RtpHeaderExtensionParameters;

struct RtpFecParameters {
  // If unset, a value is chosen by the implementation.
  // Works just like RtpEncodingParameters::ssrc.
  rtc::Optional<uint32_t> ssrc;

  FecMechanism mechanism = FecMechanism::RED;

  // Constructors for convenience.
  RtpFecParameters();
  explicit RtpFecParameters(FecMechanism mechanism);
  RtpFecParameters(FecMechanism mechanism, uint32_t ssrc);
  ~RtpFecParameters();

  bool operator==(const RtpFecParameters& o) const {
    return ssrc == o.ssrc && mechanism == o.mechanism;
  }
  bool operator!=(const RtpFecParameters& o) const { return !(*this == o); }
};

struct RtpRtxParameters {
  // If unset, a value is chosen by the implementation.
  // Works just like RtpEncodingParameters::ssrc.
  rtc::Optional<uint32_t> ssrc;

  // Constructors for convenience.
  RtpRtxParameters();
  explicit RtpRtxParameters(uint32_t ssrc);
  ~RtpRtxParameters();

  bool operator==(const RtpRtxParameters& o) const { return ssrc == o.ssrc; }
  bool operator!=(const RtpRtxParameters& o) const { return !(*this == o); }
};

struct RtpEncodingParameters {
  RtpEncodingParameters();
  ~RtpEncodingParameters();

  // If unset, a value is chosen by the implementation.
  //
  // Note that the chosen value is NOT returned by GetParameters, because it
  // may change due to an SSRC conflict, in which case the conflict is handled
  // internally without any event. Another way of looking at this is that an
  // unset SSRC acts as a "wildcard" SSRC.
  rtc::Optional<uint32_t> ssrc;

  // Can be used to reference a codec in the |codecs| member of the
  // RtpParameters that contains this RtpEncodingParameters. If unset, the
  // implementation will choose the first possible codec (if a sender), or
  // prepare to receive any codec (for a receiver).
  // TODO(deadbeef): Not implemented. Implementation of RtpSender will always
  // choose the first codec from the list.
  rtc::Optional<int> codec_payload_type;

  // Specifies the FEC mechanism, if set.
  // TODO(deadbeef): Not implemented. Current implementation will use whatever
  // FEC codecs are available, including red+ulpfec.
  rtc::Optional<RtpFecParameters> fec;

  // Specifies the RTX parameters, if set.
  // TODO(deadbeef): Not implemented with PeerConnection senders/receivers.
  rtc::Optional<RtpRtxParameters> rtx;

  // Only used for audio. If set, determines whether or not discontinuous
  // transmission will be used, if an available codec supports it. If not
  // set, the implementation default setting will be used.
  // TODO(deadbeef): Not implemented. Current implementation will use a CN
  // codec as long as it's present.
  rtc::Optional<DtxStatus> dtx;

  // The relative priority of this encoding.
  // TODO(deadbeef): Not implemented.
  rtc::Optional<PriorityType> priority;

  // If set, this represents the Transport Independent Application Specific
  // maximum bandwidth defined in RFC3890. If unset, there is no maximum
  // bitrate.
  //
  // Just called "maxBitrate" in ORTC spec.
  //
  // TODO(deadbeef): With ORTC RtpSenders, this currently sets the total
  // bandwidth for the entire bandwidth estimator (audio and video). This is
  // just always how "b=AS" was handled, but it's not correct and should be
  // fixed.
  rtc::Optional<int> max_bitrate_bps;

  // TODO(deadbeef): Not implemented.
  rtc::Optional<int> max_framerate;

  // For video, scale the resolution down by this factor.
  // TODO(deadbeef): Not implemented.
  double scale_resolution_down_by = 1.0;

  // Scale the framerate down by this factor.
  // TODO(deadbeef): Not implemented.
  double scale_framerate_down_by = 1.0;

  // For an RtpSender, set to true to cause this encoding to be sent, and false
  // for it not to be sent. For an RtpReceiver, set to true to cause the
  // encoding to be decoded, and false for it to be ignored.
  // TODO(deadbeef): Not implemented for PeerConnection RtpReceivers.
  bool active = true;

  // Value to use for RID RTP header extension.
  // Called "encodingId" in ORTC.
  // TODO(deadbeef): Not implemented.
  std::string rid;

  // RIDs of encodings on which this layer depends.
  // Called "dependencyEncodingIds" in ORTC spec.
  // TODO(deadbeef): Not implemented.
  std::vector<std::string> dependency_rids;

  bool operator==(const RtpEncodingParameters& o) const {
    return ssrc == o.ssrc && codec_payload_type == o.codec_payload_type &&
           fec == o.fec && rtx == o.rtx && dtx == o.dtx &&
           priority == o.priority && max_bitrate_bps == o.max_bitrate_bps &&
           max_framerate == o.max_framerate &&
           scale_resolution_down_by == o.scale_resolution_down_by &&
           scale_framerate_down_by == o.scale_framerate_down_by &&
           active == o.active && rid == o.rid &&
           dependency_rids == o.dependency_rids;
  }
  bool operator!=(const RtpEncodingParameters& o) const {
    return !(*this == o);
  }
};

struct RtpCodecParameters {
  RtpCodecParameters();
  ~RtpCodecParameters();

  // Build MIME "type/subtype" string from |name| and |kind|.
  std::string mime_type() const { return MediaTypeToString(kind) + "/" + name; }

  // Used to identify the codec. Equivalent to MIME subtype.
  std::string name;

  // The media type of this codec. Equivalent to MIME top-level type.
  cricket::MediaType kind = cricket::MEDIA_TYPE_AUDIO;

  // Payload type used to identify this codec in RTP packets.
  // This must always be present, and must be unique across all codecs using
  // the same transport.
  int payload_type = 0;

  // If unset, the implementation default is used.
  rtc::Optional<int> clock_rate;

  // The number of audio channels used. Unset for video codecs. If unset for
  // audio, the implementation default is used.
  // TODO(deadbeef): The "implementation default" part isn't fully implemented.
  // Only defaults to 1, even though some codecs (such as opus) should really
  // default to 2.
  rtc::Optional<int> num_channels;

  // The maximum packetization time to be used by an RtpSender.
  // If |ptime| is also set, this will be ignored.
  // TODO(deadbeef): Not implemented.
  rtc::Optional<int> max_ptime;

  // The packetization time to be used by an RtpSender.
  // If unset, will use any time up to max_ptime.
  // TODO(deadbeef): Not implemented.
  rtc::Optional<int> ptime;

  // Feedback mechanisms to be used for this codec.
  // TODO(deadbeef): Not implemented with PeerConnection senders/receivers.
  std::vector<RtcpFeedback> rtcp_feedback;

  // Codec-specific parameters that must be signaled to the remote party.
  //
  // Corresponds to "a=fmtp" parameters in SDP.
  //
  // Contrary to ORTC, these parameters are named using all lowercase strings.
  // This helps make the mapping to SDP simpler, if an application is using
  // SDP. Boolean values are represented by the string "1".
  //
  // TODO(deadbeef): Not implemented with PeerConnection senders/receivers.
  std::unordered_map<std::string, std::string> parameters;

  bool operator==(const RtpCodecParameters& o) const {
    return name == o.name && kind == o.kind && payload_type == o.payload_type &&
           clock_rate == o.clock_rate && num_channels == o.num_channels &&
           max_ptime == o.max_ptime && ptime == o.ptime &&
           rtcp_feedback == o.rtcp_feedback && parameters == o.parameters;
  }
  bool operator!=(const RtpCodecParameters& o) const { return !(*this == o); }
};

// RtpCapabilities is used to represent the static capabilities of an
// endpoint. An application can use these capabilities to construct an
// RtpParameters.
struct RtpCapabilities {
  RtpCapabilities();
  ~RtpCapabilities();

  // Supported codecs.
  std::vector<RtpCodecCapability> codecs;

  // Supported RTP header extensions.
  std::vector<RtpHeaderExtensionCapability> header_extensions;

  // Supported Forward Error Correction (FEC) mechanisms. Note that the RED,
  // ulpfec and flexfec codecs used by these mechanisms will still appear in
  // |codecs|.
  std::vector<FecMechanism> fec;

  bool operator==(const RtpCapabilities& o) const {
    return codecs == o.codecs && header_extensions == o.header_extensions &&
           fec == o.fec;
  }
  bool operator!=(const RtpCapabilities& o) const { return !(*this == o); }
};

// Note that unlike in ORTC, an RtcpParameters structure is not included in
// RtpParameters, because our API includes an additional "RtpTransport"
// abstraction on which RTCP parameters are set.
struct RtpParameters {
  RtpParameters();
  ~RtpParameters();

  // Used when calling getParameters/setParameters with a PeerConnection
  // RtpSender, to ensure that outdated parameters are not unintentionally
  // applied successfully.
  // TODO(deadbeef): Not implemented.
  std::string transaction_id;

  // Value to use for MID RTP header extension.
  // Called "muxId" in ORTC.
  // TODO(deadbeef): Not implemented.
  std::string mid;

  std::vector<RtpCodecParameters> codecs;

  // TODO(deadbeef): Not implemented with PeerConnection senders/receivers.
  std::vector<RtpHeaderExtensionParameters> header_extensions;

  std::vector<RtpEncodingParameters> encodings;

  // TODO(deadbeef): Not implemented.
  DegradationPreference degradation_preference =
      DegradationPreference::BALANCED;

  bool operator==(const RtpParameters& o) const {
    return mid == o.mid && codecs == o.codecs &&
           header_extensions == o.header_extensions &&
           encodings == o.encodings &&
           degradation_preference == o.degradation_preference;
  }
  bool operator!=(const RtpParameters& o) const { return !(*this == o); }
};

}  // namespace webrtc

#endif  // API_RTPPARAMETERS_H_
