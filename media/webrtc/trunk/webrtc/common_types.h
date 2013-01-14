/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_TYPES_H
#define WEBRTC_COMMON_TYPES_H

#include "typedefs.h"

#if defined(_MSC_VER)
// Disable "new behavior: elements of array will be default initialized"
// warning. Affects OverUseDetectorOptions.
#pragma warning(disable:4351)
#endif

#ifdef WEBRTC_EXPORT
#define WEBRTC_DLLEXPORT _declspec(dllexport)
#elif WEBRTC_DLL
#define WEBRTC_DLLEXPORT _declspec(dllimport)
#else
#define WEBRTC_DLLEXPORT
#endif

#ifndef NULL
#define NULL 0
#endif

#define RTP_PAYLOAD_NAME_SIZE 32

namespace webrtc {

class InStream
{
public:
    virtual int Read(void *buf,int len) = 0;
    virtual int Rewind() {return -1;}
    virtual ~InStream() {}
protected:
    InStream() {}
};

class OutStream
{
public:
    virtual bool Write(const void *buf,int len) = 0;
    virtual int Rewind() {return -1;}
    virtual ~OutStream() {}
protected:
    OutStream() {}
};

enum TraceModule
{
    kTraceUndefined          = 0,
    // not a module, triggered from the engine code
    kTraceVoice              = 0x0001,
    // not a module, triggered from the engine code
    kTraceVideo              = 0x0002,
    // not a module, triggered from the utility code
    kTraceUtility            = 0x0003,
    kTraceRtpRtcp            = 0x0004,
    kTraceTransport          = 0x0005,
    kTraceSrtp               = 0x0006,
    kTraceAudioCoding        = 0x0007,
    kTraceAudioMixerServer   = 0x0008,
    kTraceAudioMixerClient   = 0x0009,
    kTraceFile               = 0x000a,
    kTraceAudioProcessing    = 0x000b,
    kTraceVideoCoding        = 0x0010,
    kTraceVideoMixer         = 0x0011,
    kTraceAudioDevice        = 0x0012,
    kTraceVideoRenderer      = 0x0014,
    kTraceVideoCapture       = 0x0015,
    kTraceVideoPreocessing   = 0x0016
};

enum TraceLevel
{
    kTraceNone               = 0x0000,    // no trace
    kTraceStateInfo          = 0x0001,
    kTraceWarning            = 0x0002,
    kTraceError              = 0x0004,
    kTraceCritical           = 0x0008,
    kTraceApiCall            = 0x0010,
    kTraceDefault            = 0x00ff,

    kTraceModuleCall         = 0x0020,
    kTraceMemory             = 0x0100,   // memory info
    kTraceTimer              = 0x0200,   // timing info
    kTraceStream             = 0x0400,   // "continuous" stream of data

    // used for debug purposes
    kTraceDebug              = 0x0800,  // debug
    kTraceInfo               = 0x1000,  // debug info

    // Non-verbose level used by LS_INFO of logging.h. Do not use directly.
    kTraceTerseInfo          = 0x2000,

    kTraceAll                = 0xffff
};

// External Trace API
class TraceCallback {
 public:
  virtual void Print(TraceLevel level, const char* message, int length) = 0;

 protected:
  virtual ~TraceCallback() {}
  TraceCallback() {}
};

enum FileFormats
{
    kFileFormatWavFile        = 1,
    kFileFormatCompressedFile = 2,
    kFileFormatAviFile        = 3,
    kFileFormatPreencodedFile = 4,
    kFileFormatPcm16kHzFile   = 7,
    kFileFormatPcm8kHzFile    = 8,
    kFileFormatPcm32kHzFile   = 9
};


enum ProcessingTypes
{
    kPlaybackPerChannel = 0,
    kPlaybackAllChannelsMixed,
    kRecordingPerChannel,
    kRecordingAllChannelsMixed,
    kRecordingPreprocessing
};

// Encryption enums
enum CipherTypes
{
    kCipherNull               = 0,
    kCipherAes128CounterMode  = 1
};

enum AuthenticationTypes
{
    kAuthNull       = 0,
    kAuthHmacSha1   = 3
};

enum SecurityLevels
{
    kNoProtection                    = 0,
    kEncryption                      = 1,
    kAuthentication                  = 2,
    kEncryptionAndAuthentication     = 3
};

// Interface for encrypting and decrypting regular data and rtp/rtcp packets.
// Implement this interface if you wish to provide an encryption scheme to
// the voice or video engines.
class Encryption
{
public:
    // Encrypt the given data.
    //
    // Args:
    //   channel: The channel to encrypt data for.
    //   in_data: The data to encrypt. This data is bytes_in bytes long.
    //   out_data: The buffer to write the encrypted data to. You may write more
    //       bytes of encrypted data than what you got as input, up to a maximum
    //       of webrtc::kViEMaxMtu if you are encrypting in the video engine, or
    //       webrtc::kVoiceEngineMaxIpPacketSizeBytes for the voice engine.
    //   bytes_in: The number of bytes in the input buffer.
    //   bytes_out: The number of bytes written in out_data.
    virtual void encrypt(
        int channel,
        unsigned char* in_data,
        unsigned char* out_data,
        int bytes_in,
        int* bytes_out) = 0;

    // Decrypts the given data. This should reverse the effects of encrypt().
    //
    // Args:
    //   channel_no: The channel to decrypt data for.
    //   in_data: The data to decrypt. This data is bytes_in bytes long.
    //   out_data: The buffer to write the decrypted data to. You may write more
    //       bytes of decrypted data than what you got as input, up to a maximum
    //       of webrtc::kViEMaxMtu if you are encrypting in the video engine, or
    //       webrtc::kVoiceEngineMaxIpPacketSizeBytes for the voice engine.
    //   bytes_in: The number of bytes in the input buffer.
    //   bytes_out: The number of bytes written in out_data.
    virtual void decrypt(
        int channel,
        unsigned char* in_data,
        unsigned char* out_data,
        int bytes_in,
        int* bytes_out) = 0;

    // Encrypts a RTCP packet. Otherwise, this method has the same contract as
    // encrypt().
    virtual void encrypt_rtcp(
        int channel,
        unsigned char* in_data,
        unsigned char* out_data,
        int bytes_in,
        int* bytes_out) = 0;

    // Decrypts a RTCP packet. Otherwise, this method has the same contract as
    // decrypt().
    virtual void decrypt_rtcp(
        int channel,
        unsigned char* in_data,
        unsigned char* out_data,
        int bytes_in,
        int* bytes_out) = 0;

protected:
    virtual ~Encryption() {}
    Encryption() {}
};

// External transport callback interface
class Transport
{
public:
    virtual int SendPacket(int channel, const void *data, int len) = 0;
    virtual int SendRTCPPacket(int channel, const void *data, int len) = 0;

protected:
    virtual ~Transport() {}
    Transport() {}
};

// ==================================================================
// Voice specific types
// ==================================================================

// Each codec supported can be described by this structure.
struct CodecInst
{
    int pltype;
    char plname[RTP_PAYLOAD_NAME_SIZE];
    int plfreq;
    int pacsize;
    int channels;
    int rate;
};

enum FrameType
{
    kFrameEmpty            = 0,
    kAudioFrameSpeech      = 1,
    kAudioFrameCN          = 2,
    kVideoFrameKey         = 3,    // independent frame
    kVideoFrameDelta       = 4,    // depends on the previus frame
    kVideoFrameGolden      = 5,    // depends on a old known previus frame
    kVideoFrameAltRef      = 6
};

// RTP
enum {kRtpCsrcSize = 15}; // RFC 3550 page 13

enum RTPDirections
{
    kRtpIncoming = 0,
    kRtpOutgoing
};

enum PayloadFrequencies
{
    kFreq8000Hz = 8000,
    kFreq16000Hz = 16000,
    kFreq32000Hz = 32000
};

enum VadModes                 // degree of bandwidth reduction
{
    kVadConventional = 0,      // lowest reduction
    kVadAggressiveLow,
    kVadAggressiveMid,
    kVadAggressiveHigh         // highest reduction
};

struct NetworkStatistics           // NETEQ statistics
{
    // current jitter buffer size in ms
    WebRtc_UWord16 currentBufferSize;
    // preferred (optimal) buffer size in ms
    WebRtc_UWord16 preferredBufferSize;
    // adding extra delay due to "peaky jitter"
    bool jitterPeaksFound;
    // loss rate (network + late) in percent (in Q14)
    WebRtc_UWord16 currentPacketLossRate;
    // late loss rate in percent (in Q14)
    WebRtc_UWord16 currentDiscardRate;
    // fraction (of original stream) of synthesized speech inserted through
    // expansion (in Q14)
    WebRtc_UWord16 currentExpandRate;
    // fraction of synthesized speech inserted through pre-emptive expansion
    // (in Q14)
    WebRtc_UWord16 currentPreemptiveRate;
    // fraction of data removed through acceleration (in Q14)
    WebRtc_UWord16 currentAccelerateRate;
    // clock-drift in parts-per-million (negative or positive)
    int32_t clockDriftPPM;
    // average packet waiting time in the jitter buffer (ms)
    int meanWaitingTimeMs;
    // median packet waiting time in the jitter buffer (ms)
    int medianWaitingTimeMs;
    // min packet waiting time in the jitter buffer (ms)
    int minWaitingTimeMs;
    // max packet waiting time in the jitter buffer (ms)
    int maxWaitingTimeMs;
    // added samples in off mode due to packet loss
    int addedSamples;
};

typedef struct
{
    int min;              // minumum
    int max;              // maximum
    int average;          // average
} StatVal;

typedef struct           // All levels are reported in dBm0
{
    StatVal speech_rx;   // long-term speech levels on receiving side
    StatVal speech_tx;   // long-term speech levels on transmitting side
    StatVal noise_rx;    // long-term noise/silence levels on receiving side
    StatVal noise_tx;    // long-term noise/silence levels on transmitting side
} LevelStatistics;

typedef struct        // All levels are reported in dB
{
    StatVal erl;      // Echo Return Loss
    StatVal erle;     // Echo Return Loss Enhancement
    StatVal rerl;     // RERL = ERL + ERLE
    // Echo suppression inside EC at the point just before its NLP
    StatVal a_nlp;
} EchoStatistics;

enum TelephoneEventDetectionMethods
{
    kInBand = 0,
    kOutOfBand = 1,
    kInAndOutOfBand = 2
};

enum NsModes    // type of Noise Suppression
{
    kNsUnchanged = 0,   // previously set mode
    kNsDefault,         // platform default
    kNsConference,      // conferencing default
    kNsLowSuppression,  // lowest suppression
    kNsModerateSuppression,
    kNsHighSuppression,
    kNsVeryHighSuppression,     // highest suppression
};

enum AgcModes                  // type of Automatic Gain Control
{
    kAgcUnchanged = 0,        // previously set mode
    kAgcDefault,              // platform default
    // adaptive mode for use when analog volume control exists (e.g. for
    // PC softphone)
    kAgcAdaptiveAnalog,
    // scaling takes place in the digital domain (e.g. for conference servers
    // and embedded devices)
    kAgcAdaptiveDigital,
    // can be used on embedded devices where the capture signal level
    // is predictable
    kAgcFixedDigital
};

// EC modes
enum EcModes                   // type of Echo Control
{
    kEcUnchanged = 0,          // previously set mode
    kEcDefault,                // platform default
    kEcConference,             // conferencing default (aggressive AEC)
    kEcAec,                    // Acoustic Echo Cancellation
    kEcAecm,                   // AEC mobile
};

// AECM modes
enum AecmModes                 // mode of AECM
{
    kAecmQuietEarpieceOrHeadset = 0,
                               // Quiet earpiece or headset use
    kAecmEarpiece,             // most earpiece use
    kAecmLoudEarpiece,         // Loud earpiece or quiet speakerphone use
    kAecmSpeakerphone,         // most speakerphone use (default)
    kAecmLoudSpeakerphone      // Loud speakerphone
};

// AGC configuration
typedef struct
{
    unsigned short targetLeveldBOv;
    unsigned short digitalCompressionGaindB;
    bool           limiterEnable;
} AgcConfig;                  // AGC configuration parameters

enum StereoChannel
{
    kStereoLeft = 0,
    kStereoRight,
    kStereoBoth
};

// Audio device layers
enum AudioLayers
{
    kAudioPlatformDefault = 0,
    kAudioWindowsWave = 1,
    kAudioWindowsCore = 2,
    kAudioLinuxAlsa = 3,
    kAudioLinuxPulse = 4
};

enum NetEqModes             // NetEQ playout configurations
{
    // Optimized trade-off between low delay and jitter robustness for two-way
    // communication.
    kNetEqDefault = 0,
    // Improved jitter robustness at the cost of increased delay. Can be
    // used in one-way communication.
    kNetEqStreaming = 1,
    // Optimzed for decodability of fax signals rather than for perceived audio
    // quality.
    kNetEqFax = 2,
    // Minimal buffer management. Inserts zeros for lost packets and during
    // buffer increases.
    kNetEqOff = 3,
};

enum NetEqBgnModes          // NetEQ Background Noise (BGN) configurations
{
    // BGN is always on and will be generated when the incoming RTP stream
    // stops (default).
    kBgnOn = 0,
    // The BGN is faded to zero (complete silence) after a few seconds.
    kBgnFade = 1,
    // BGN is not used at all. Silence is produced after speech extrapolation
    // has faded.
    kBgnOff = 2,
};

enum OnHoldModes            // On Hold direction
{
    kHoldSendAndPlay = 0,    // Put both sending and playing in on-hold state.
    kHoldSendOnly,           // Put only sending in on-hold state.
    kHoldPlayOnly            // Put only playing in on-hold state.
};

enum AmrMode
{
    kRfc3267BwEfficient = 0,
    kRfc3267OctetAligned = 1,
    kRfc3267FileStorage = 2,
};

// ==================================================================
// Video specific types
// ==================================================================

// Raw video types
enum RawVideoType
{
    kVideoI420     = 0,
    kVideoYV12     = 1,
    kVideoYUY2     = 2,
    kVideoUYVY     = 3,
    kVideoIYUV     = 4,
    kVideoARGB     = 5,
    kVideoRGB24    = 6,
    kVideoRGB565   = 7,
    kVideoARGB4444 = 8,
    kVideoARGB1555 = 9,
    kVideoMJPEG    = 10,
    kVideoNV12     = 11,
    kVideoNV21     = 12,
    kVideoBGRA     = 13,
    kVideoUnknown  = 99
};

// Video codec
enum { kConfigParameterSize = 128};
enum { kPayloadNameSize = 32};
enum { kMaxSimulcastStreams = 4};
enum { kMaxTemporalStreams = 4};

enum VideoCodecComplexity
{
    kComplexityNormal = 0,
    kComplexityHigh    = 1,
    kComplexityHigher  = 2,
    kComplexityMax     = 3
};

enum VideoCodecProfile
{
    kProfileBase = 0x00,
    kProfileMain = 0x01
};

enum VP8ResilienceMode {
  kResilienceOff,    // The stream produced by the encoder requires a
                     // recovery frame (typically a key frame) to be
                     // decodable after a packet loss.
  kResilientStream,  // A stream produced by the encoder is resilient to
                     // packet losses, but packets within a frame subsequent
                     // to a loss can't be decoded.
  kResilientFrames   // Same as kResilientStream but with added resilience
                     // within a frame.
};

// VP8 specific
struct VideoCodecVP8
{
    bool                 pictureLossIndicationOn;
    bool                 feedbackModeOn;
    VideoCodecComplexity complexity;
    VP8ResilienceMode    resilience;
    unsigned char        numberOfTemporalLayers;
    bool                 denoisingOn;
    bool                 errorConcealmentOn;
    bool                 automaticResizeOn;
    bool                 frameDroppingOn;
};

// Unknown specific
struct VideoCodecGeneric
{
};

// Video codec types
enum VideoCodecType
{
    kVideoCodecVP8,
    kVideoCodecI420,
    kVideoCodecRED,
    kVideoCodecULPFEC,
    kVideoCodecUnknown
};

union VideoCodecUnion
{
    VideoCodecVP8       VP8;
    VideoCodecGeneric   Generic;
};


// Simulcast is when the same stream is encoded multiple times with different
// settings such as resolution.
struct SimulcastStream
{
    unsigned short      width;
    unsigned short      height;
    unsigned char       numberOfTemporalLayers;
    unsigned int        maxBitrate;
    unsigned int        qpMax; // minimum quality
};

// Common video codec properties
struct VideoCodec
{
    VideoCodecType      codecType;
    char                plName[kPayloadNameSize];
    unsigned char       plType;

    unsigned short      width;
    unsigned short      height;

    unsigned int        startBitrate;
    unsigned int        maxBitrate;
    unsigned int        minBitrate;
    unsigned char       maxFramerate;

    VideoCodecUnion     codecSpecific;

    unsigned int        qpMax;
    unsigned char       numberOfSimulcastStreams;
    SimulcastStream     simulcastStream[kMaxSimulcastStreams];
};

// Bandwidth over-use detector options.  These are used to drive
// experimentation with bandwidth estimation parameters.
// See modules/remote_bitrate_estimator/overuse_detector.h
struct OverUseDetectorOptions {
  OverUseDetectorOptions()
      : initial_slope(8.0/512.0),
        initial_offset(0),
        initial_e(),
        initial_process_noise(),
        initial_avg_noise(0.0),
        initial_var_noise(50),
        initial_threshold(25.0) {
    initial_e[0][0] = 100;
    initial_e[1][1] = 1e-1;
    initial_e[0][1] = initial_e[1][0] = 0;
    initial_process_noise[0] = 1e-10;
    initial_process_noise[1] = 1e-2;
  }
  double initial_slope;
  double initial_offset;
  double initial_e[2][2];
  double initial_process_noise[2];
  double initial_avg_noise;
  double initial_var_noise;
  double initial_threshold;
};
}  // namespace webrtc
#endif  // WEBRTC_COMMON_TYPES_H
