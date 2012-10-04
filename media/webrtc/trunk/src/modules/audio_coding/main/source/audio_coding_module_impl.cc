/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_coding_module_impl.h"

#include <assert.h>
#include <stdlib.h>
#ifdef ACM_QA_TEST
#   include <stdio.h>
#endif

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_dtmf_detection.h"
#include "acm_generic_codec.h"
#include "acm_resampler.h"
#include "critical_section_wrapper.h"
#include "engine_configurations.h"
#include "rw_lock_wrapper.h"
#include "trace.h"

namespace webrtc {

enum {
  kACMToneEnd = 999
};

// Maximum number of bytes in one packet (PCM16B, 20 ms packets, stereo).
enum {
  kMaxPacketSize = 2560
};

AudioCodingModuleImpl::AudioCodingModuleImpl(const WebRtc_Word32 id)
    : _packetizationCallback(NULL),
      _id(id),
      _lastTimestamp(0),
      _lastInTimestamp(0),
      _cng_nb_pltype(255),
      _cng_wb_pltype(255),
      _cng_swb_pltype(255),
      _red_pltype(255),
      _vadEnabled(false),
      _dtxEnabled(false),
      _vadMode(VADNormal),
      _stereoReceiveRegistered(false),
      _stereoSend(false),
      _prev_received_channel(0),
      _expected_channels(1),
      _currentSendCodecIdx(-1),
      _current_receive_codec_idx(-1),
      _sendCodecRegistered(false),
      _acmCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _vadCallback(NULL),
      _lastRecvAudioCodecPlType(255),
      _isFirstRED(true),
      _fecEnabled(false),
      _fragmentation(NULL),
      _lastFECTimestamp(0),
      _receiveREDPayloadType(255),
      _previousPayloadType(255),
      _dummyRTPHeader(NULL),
      _recvPlFrameSizeSmpls(0),
      _receiverInitialized(false),
      _dtmfDetector(NULL),
      _dtmfCallback(NULL),
      _lastDetectedTone(kACMToneEnd),
      _callbackCritSect(CriticalSectionWrapper::CreateCriticalSection()) {
  _lastTimestamp = 0xD87F3F9F;
  _lastInTimestamp = 0xD87F3F9F;

  // Nullify send codec memory, set payload type and set codec name to
  // invalid values.
  memset(&_sendCodecInst, 0, sizeof(CodecInst));
  strncpy(_sendCodecInst.plname, "noCodecRegistered", 31);
  _sendCodecInst.pltype = -1;

  for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
    _codecs[i] = NULL;
    _registeredPlTypes[i] = -1;
    _stereoReceive[i] = false;
    _slaveCodecs[i] = NULL;
    _mirrorCodecIdx[i] = -1;
  }

  _netEq.SetUniqueId(_id);

  // Allocate memory for RED.
  _redBuffer = new WebRtc_UWord8[MAX_PAYLOAD_SIZE_BYTE];
  _fragmentation = new RTPFragmentationHeader;
  _fragmentation->fragmentationVectorSize = 2;
  _fragmentation->fragmentationOffset = new WebRtc_UWord32[2];
  _fragmentation->fragmentationLength = new WebRtc_UWord32[2];
  _fragmentation->fragmentationTimeDiff = new WebRtc_UWord16[2];
  _fragmentation->fragmentationPlType = new WebRtc_UWord8[2];

  // Register the default payload type for RED and for CNG for the three
  // frequencies 8, 16 and 32 kHz.
  for (int i = (ACMCodecDB::kNumCodecs - 1); i >= 0; i--) {
    if (IsCodecRED(i)) {
      _red_pltype = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
    } else if (IsCodecCN(i)) {
      if (ACMCodecDB::database_[i].plfreq == 8000) {
        _cng_nb_pltype = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 16000) {
        _cng_wb_pltype = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 32000) {
        _cng_swb_pltype = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      }
    }
  }

  if (InitializeReceiverSafe() < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Cannot initialize reciever");
  }
#ifdef ACM_QA_TEST
  char file_name[500];
  sprintf(file_name, "ACM_QA_incomingPL_%03d_%d%d%d%d%d%d.dat", _id,
          rand() % 10, rand() % 10, rand() % 10, rand() % 10, rand() % 10,
          rand() % 10);
  _incomingPL = fopen(file_name, "wb");
  sprintf(file_name, "ACM_QA_outgoingPL_%03d_%d%d%d%d%d%d.dat", _id,
          rand() % 10, rand() % 10, rand() % 10, rand() % 10, rand() % 10,
          rand() % 10);
  _outgoingPL = fopen(file_name, "wb");
#endif

  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, id, "Created");
}

AudioCodingModuleImpl::~AudioCodingModuleImpl() {
  {
    CriticalSectionScoped lock(_acmCritSect);
    _currentSendCodecIdx = -1;

    for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
      if (_codecs[i] != NULL) {
        // True stereo codecs share the same memory for master and
        // slave, so slave codec need to be nullified here, since the
        // memory will be deleted.
        if (_slaveCodecs[i] == _codecs[i]) {
          _slaveCodecs[i] = NULL;
        }

        // Mirror index holds the address of the codec memory.
        assert(_mirrorCodecIdx[i] > -1);
        if (_codecs[_mirrorCodecIdx[i]] != NULL) {
          delete _codecs[_mirrorCodecIdx[i]];
          _codecs[_mirrorCodecIdx[i]] = NULL;
        }

        _codecs[i] = NULL;
      }

      if (_slaveCodecs[i] != NULL) {
        // Delete memory for stereo usage of mono codecs.
        assert(_mirrorCodecIdx[i] > -1);
        if (_slaveCodecs[_mirrorCodecIdx[i]] != NULL) {
          delete _slaveCodecs[_mirrorCodecIdx[i]];
          _slaveCodecs[_mirrorCodecIdx[i]] = NULL;
        }
        _slaveCodecs[i] = NULL;
      }
    }

    if (_dtmfDetector != NULL) {
      delete _dtmfDetector;
      _dtmfDetector = NULL;
    }
    if (_dummyRTPHeader != NULL) {
      delete _dummyRTPHeader;
      _dummyRTPHeader = NULL;
    }
    if (_redBuffer != NULL) {
      delete[] _redBuffer;
      _redBuffer = NULL;
    }
    if (_fragmentation != NULL) {
      // Only need to delete fragmentation header, it will clean
      // up it's own memory.
      delete _fragmentation;
      _fragmentation = NULL;
    }
  }

#ifdef ACM_QA_TEST
  if(_incomingPL != NULL) {
    fclose(_incomingPL);
  }

  if(_outgoingPL != NULL) {
    fclose(_outgoingPL);
  }
#endif

  delete _callbackCritSect;
  _callbackCritSect = NULL;

  delete _acmCritSect;
  _acmCritSect = NULL;
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, _id,
               "Destroyed");
}

WebRtc_Word32 AudioCodingModuleImpl::ChangeUniqueId(const WebRtc_Word32 id) {
  {
    CriticalSectionScoped lock(_acmCritSect);
    _id = id;

#ifdef ACM_QA_TEST
    if (_incomingPL != NULL) {
      fclose (_incomingPL);
    }
    if (_outgoingPL != NULL) {
      fclose (_outgoingPL);
    }
    char fileName[500];
    sprintf(fileName, "ACM_QA_incomingPL_%03d_%d%d%d%d%d%d.dat", _id,
            rand() % 10, rand() % 10, rand() % 10, rand() % 10, rand() % 10,
            rand() % 10);
    _incomingPL = fopen(fileName, "wb");
    sprintf(fileName, "ACM_QA_outgoingPL_%03d_%d%d%d%d%d%d.dat", _id,
            rand() % 10, rand() % 10, rand() % 10, rand() % 10, rand() % 10,
            rand() % 10);
    _outgoingPL = fopen(fileName, "wb");
#endif

    for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
      if (_codecs[i] != NULL) {
        _codecs[i]->SetUniqueID(id);
      }
    }
  }

  _netEq.SetUniqueId(_id);
  return 0;
}

// Returns the number of milliseconds until the module want a
// worker thread to call Process.
WebRtc_Word32 AudioCodingModuleImpl::TimeUntilNextProcess() {
  CriticalSectionScoped lock(_acmCritSect);

  if (!HaveValidEncoder("TimeUntilNextProcess")) {
    return -1;
  }
  return _codecs[_currentSendCodecIdx]->SamplesLeftToEncode() /
      (_sendCodecInst.plfreq / 1000);
}

// Process any pending tasks such as timeouts.
WebRtc_Word32 AudioCodingModuleImpl::Process() {
  // Make room for 1 RED payload.
  WebRtc_UWord8 stream[2 * MAX_PAYLOAD_SIZE_BYTE];
  WebRtc_Word16 length_bytes = 2 * MAX_PAYLOAD_SIZE_BYTE;
  WebRtc_Word16 red_length_bytes = length_bytes;
  WebRtc_UWord32 rtp_timestamp;
  WebRtc_Word16 status;
  WebRtcACMEncodingType encoding_type;
  FrameType frame_type = kAudioFrameSpeech;
  WebRtc_UWord8 current_payload_type = 0;
  bool has_data_to_send = false;
  bool fec_active = false;

  // Keep the scope of the ACM critical section limited.
  {
    CriticalSectionScoped lock(_acmCritSect);
    if (!HaveValidEncoder("Process")) {
      return -1;
    }

    status = _codecs[_currentSendCodecIdx]->Encode(stream, &length_bytes,
                                                   &rtp_timestamp,
                                                   &encoding_type);
    if (status < 0) {
      // Encode failed.
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Process(): Encoding Failed");
      length_bytes = 0;
      return -1;
    } else if (status == 0) {
      // Not enough data.
      return 0;
    } else {
      switch (encoding_type) {
        case kNoEncoding: {
          current_payload_type = _previousPayloadType;
          frame_type = kFrameEmpty;
          length_bytes = 0;
          break;
        }
        case kActiveNormalEncoded:
        case kPassiveNormalEncoded: {
          current_payload_type = (WebRtc_UWord8) _sendCodecInst.pltype;
          frame_type = kAudioFrameSpeech;
          break;
        }
        case kPassiveDTXNB: {
          current_payload_type = _cng_nb_pltype;
          frame_type = kAudioFrameCN;
          _isFirstRED = true;
          break;
        }
        case kPassiveDTXWB: {
          current_payload_type = _cng_wb_pltype;
          frame_type = kAudioFrameCN;
          _isFirstRED = true;
          break;
        }
        case kPassiveDTXSWB: {
          current_payload_type = _cng_swb_pltype;
          frame_type = kAudioFrameCN;
          _isFirstRED = true;
          break;
        }
      }
      has_data_to_send = true;
      _previousPayloadType = current_payload_type;

      // Redundancy encode is done here. The two bitstreams packetized into
      // one RTP packet and the fragmentation points are set.
      // Only apply RED on speech data.
      if ((_fecEnabled) &&
          ((encoding_type == kActiveNormalEncoded) ||
              (encoding_type == kPassiveNormalEncoded))) {
        // FEC is enabled within this scope.
        //
        // Note that, a special solution exists for iSAC since it is the only
        // codec for which getRedPayload has a non-empty implementation.
        //
        // Summary of the FEC scheme below (use iSAC as example):
        //
        //  1st (_firstRED is true) encoded iSAC frame (primary #1) =>
        //      - call getRedPayload() and store redundancy for packet #1 in
        //        second fragment of RED buffer (old data)
        //      - drop the primary iSAC frame
        //      - don't call SendData
        //  2nd (_firstRED is false) encoded iSAC frame (primary #2) =>
        //      - store primary #2 in 1st fragment of RED buffer and send the
        //        combined packet
        //      - the transmitted packet contains primary #2 (new) and
        //        reduncancy for packet #1 (old)
        //      - call getRedPayload() and store redundancy for packet #2 in
        //        second fragment of RED buffer
        //
        //  ...
        //
        //  Nth encoded iSAC frame (primary #N) =>
        //      - store primary #N in 1st fragment of RED buffer and send the
        //        combined packet
        //      - the transmitted packet contains primary #N (new) and
        //        reduncancy for packet #(N-1) (old)
        //      - call getRedPayload() and store redundancy for packet #N in
        //        second fragment of RED buffer
        //
        //  For all other codecs, getRedPayload does nothing and returns -1 =>
        //  redundant data is only a copy.
        //
        //  First combined packet contains : #2 (new) and #1 (old)
        //  Second combined packet contains: #3 (new) and #2 (old)
        //  Third combined packet contains : #4 (new) and #3 (old)
        //
        //  Hence, even if every second packet is dropped, perfect
        //  reconstruction is possible.
        fec_active = true;

        has_data_to_send = false;
        // Skip the following part for the first packet in a RED session.
        if (!_isFirstRED) {
          // Rearrange stream such that FEC packets are included.
          // Replace stream now that we have stored current stream.
          memcpy(stream + _fragmentation->fragmentationOffset[1], _redBuffer,
                 _fragmentation->fragmentationLength[1]);
          // Update the fragmentation time difference vector, in number of
          // timestamps.
          WebRtc_UWord16 time_since_last = WebRtc_UWord16(
              rtp_timestamp - _lastFECTimestamp);

          // Update fragmentation vectors.
          _fragmentation->fragmentationPlType[1] = _fragmentation
              ->fragmentationPlType[0];
          _fragmentation->fragmentationTimeDiff[1] = time_since_last;
          has_data_to_send = true;
        }

        // Insert new packet length.
        _fragmentation->fragmentationLength[0] = length_bytes;

        // Insert new packet payload type.
        _fragmentation->fragmentationPlType[0] = current_payload_type;
        _lastFECTimestamp = rtp_timestamp;

        // Can be modified by the GetRedPayload() call if iSAC is utilized.
        red_length_bytes = length_bytes;

        // A fragmentation header is provided => packetization according to
        // RFC 2198 (RTP Payload for Redundant Audio Data) will be used.
        // First fragment is the current data (new).
        // Second fragment is the previous data (old).
        length_bytes = static_cast<WebRtc_Word16>(
            _fragmentation->fragmentationLength[0] +
            _fragmentation->fragmentationLength[1]);

        // Get, and store, redundant data from the encoder based on the recently
        // encoded frame.
        // NOTE - only iSAC contains an implementation; all other codecs does
        // nothing and returns -1.
        if (_codecs[_currentSendCodecIdx]->GetRedPayload(
            _redBuffer,
            &red_length_bytes) == -1) {
          // The codec was not iSAC => use current encoder output as redundant
          // data instead (trivial FEC scheme).
          memcpy(_redBuffer, stream, red_length_bytes);
        }

        _isFirstRED = false;
        // Update payload type with RED payload type.
        current_payload_type = _red_pltype;
      }
    }
  }

  if (has_data_to_send) {
    CriticalSectionScoped lock(_callbackCritSect);
#ifdef ACM_QA_TEST
    if(_outgoingPL != NULL) {
      if (fwrite(&rtp_timestamp, sizeof(WebRtc_UWord32), 1, _outgoingPL) != 1) {
        return -1;
      }
      if (fwrite(&current_payload_type, sizeof(WebRtc_UWord8),
                 1, _outgoingPL) != 1) {
        return -1;
      }
      if (fwrite(&length_bytes, sizeof(WebRtc_Word16), 1, _outgoingPL) != 1) {
        return -1;
      }
    }
#endif

    if (_packetizationCallback != NULL) {
      if (fec_active) {
        // Callback with payload data, including redundant data (FEC/RED).
        _packetizationCallback->SendData(frame_type, current_payload_type,
                                         rtp_timestamp, stream, length_bytes,
                                         _fragmentation);
      } else {
        // Callback with payload data.
        _packetizationCallback->SendData(frame_type, current_payload_type,
                                         rtp_timestamp, stream, length_bytes,
                                         NULL);
      }
    }

    if (_vadCallback != NULL) {
      // Callback with VAD decision.
      _vadCallback->InFrameType(((WebRtc_Word16) encoding_type));
    }
  }
  if (fec_active) {
    // Store RED length in bytes.
    _fragmentation->fragmentationLength[1] = red_length_bytes;
  }
  return length_bytes;
}

/////////////////////////////////////////
//   Sender
//

// Initialize send codec.
WebRtc_Word32 AudioCodingModuleImpl::InitializeSender() {
  CriticalSectionScoped lock(_acmCritSect);

  // Start with invalid values.
  _sendCodecRegistered = false;
  _currentSendCodecIdx = -1;
  _sendCodecInst.plname[0] = '\0';

  // Delete all encoders to start fresh.
  for (int id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if (_codecs[id] != NULL) {
      _codecs[id]->DestructEncoder();
    }
  }

  // Initialize FEC/RED.
  _isFirstRED = true;
  if (_fecEnabled) {
    if (_redBuffer != NULL) {
      memset(_redBuffer, 0, MAX_PAYLOAD_SIZE_BYTE);
    }
    if (_fragmentation != NULL) {
      _fragmentation->fragmentationVectorSize = 2;
      _fragmentation->fragmentationOffset[0] = 0;
      _fragmentation->fragmentationOffset[0] = MAX_PAYLOAD_SIZE_BYTE;
      memset(_fragmentation->fragmentationLength, 0,
             sizeof(WebRtc_UWord32) * 2);
      memset(_fragmentation->fragmentationTimeDiff, 0,
             sizeof(WebRtc_UWord16) * 2);
      memset(_fragmentation->fragmentationPlType, 0, sizeof(WebRtc_UWord8) * 2);
    }
  }

  return 0;
}

WebRtc_Word32 AudioCodingModuleImpl::ResetEncoder() {
  CriticalSectionScoped lock(_acmCritSect);
  if (!HaveValidEncoder("ResetEncoder")) {
    return -1;
  }
  return _codecs[_currentSendCodecIdx]->ResetEncoder();
}

void AudioCodingModuleImpl::UnregisterSendCodec() {
  CriticalSectionScoped lock(_acmCritSect);
  _sendCodecRegistered = false;
  _currentSendCodecIdx = -1;
  return;
}

ACMGenericCodec* AudioCodingModuleImpl::CreateCodec(const CodecInst& codec) {
  ACMGenericCodec* my_codec = NULL;

  my_codec = ACMCodecDB::CreateCodecInstance(&codec);
  if (my_codec == NULL) {
    // Error, could not create the codec.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "ACMCodecDB::CreateCodecInstance() failed in CreateCodec()");
    return my_codec;
  }
  my_codec->SetUniqueID(_id);
  my_codec->SetNetEqDecodeLock(_netEq.DecodeLock());

  return my_codec;
}

// Can be called multiple times for Codec, CNG, RED.
WebRtc_Word32 AudioCodingModuleImpl::RegisterSendCodec(
    const CodecInst& send_codec) {
  if ((send_codec.channels != 1) && (send_codec.channels != 2)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Registering Send codec failed due to wrong number of "
                 "channels, %d. Only mono codecs are supported, i.e. "
                 "channels=1.", send_codec.channels);
    return -1;
  }

  char error_message[500];
  int mirror_id;
  int codec_id = ACMCodecDB::CodecNumber(&send_codec, &mirror_id, error_message,
                                        sizeof(error_message));
  CriticalSectionScoped lock(_acmCritSect);

  // Check for reported errors from function CodecNumber().
  if (codec_id < 0) {
    if (!_sendCodecRegistered) {
      // This values has to be NULL if there is no codec registered.
      _currentSendCodecIdx = -1;
    }
    // Failed to register Send Codec.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 error_message);
    return -1;
  }

  // Telephone-event cannot be a send codec.
  if (!STR_CASE_CMP(send_codec.plname, "telephone-event")) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "telephone-event cannot be registered as send codec");
    return -1;
  }

  // RED can be registered with other payload type. If not registered a default
  // payload type is used.
  if (IsCodecRED(&send_codec)) {
    // TODO(tlegrand): Remove this check. Already taken care of in
    // ACMCodecDB::CodecNumber().
    // Check if the payload-type is valid
    if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Invalid payload-type %d for %s.", send_codec.pltype,
                   send_codec.plname);
      return -1;
    }
    // Set RED payload type.
    _red_pltype = static_cast<uint8_t>(send_codec.pltype);
    return 0;
  }

  // CNG can be registered with other payload type. If not registered the
  // default payload types from codec database will be used.
  if (IsCodecCN(&send_codec)) {
    // CNG is registered.
    switch (send_codec.plfreq) {
      case 8000: {
        _cng_nb_pltype = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 16000: {
        _cng_wb_pltype = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 32000: {
        _cng_swb_pltype = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      default: {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "RegisterSendCodec() failed, invalid frequency for CNG "
                     "registration");
        return -1;
      }
    }

    return 0;
  }

  // TODO(tlegrand): Remove this check. Already taken care of in
  // ACMCodecDB::CodecNumber().
  // Check if the payload-type is valid
  if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Invalid payload-type %d for %s.", send_codec.pltype,
                 send_codec.plname);
    return -1;
  }

  // Check if codec supports the number of channels.
  if (ACMCodecDB::codec_settings_[codec_id].channel_support
      < send_codec.channels) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "%d number of channels not supportedn for %s.",
                 send_codec.channels, send_codec.plname);
    return -1;
  }

  // Set Stereo, and make sure VAD and DTX is turned off.
  if (send_codec.channels == 2) {
    _stereoSend = true;
    if (_vadEnabled || _dtxEnabled) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
                   "VAD/DTX is turned off, not supported when sending stereo.");
    }
    _vadEnabled = false;
    _dtxEnabled = false;
  } else {
    _stereoSend = false;
  }

  // Check if the codec is already registered as send codec.
  bool is_send_codec;
  if (_sendCodecRegistered) {
    int send_codec_mirror_id;
    int send_codec_id = ACMCodecDB::CodecNumber(&_sendCodecInst,
                                              &send_codec_mirror_id);
    assert(send_codec_id >= 0);
    is_send_codec = (send_codec_id == codec_id) ||
        (mirror_id == send_codec_mirror_id);
  } else {
    is_send_codec = false;
  }

  // If new codec, or new settings, register.
  if (!is_send_codec) {
    if (_codecs[mirror_id] == NULL) {

      _codecs[mirror_id] = CreateCodec(send_codec);
      if (_codecs[mirror_id] == NULL) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Cannot Create the codec");
        return -1;
      }
      _mirrorCodecIdx[mirror_id] = mirror_id;
    }

    if (mirror_id != codec_id) {
      _codecs[codec_id] = _codecs[mirror_id];
      _mirrorCodecIdx[codec_id] = mirror_id;
    }

    ACMGenericCodec* codec_ptr = _codecs[codec_id];
    WebRtc_Word16 status;
    WebRtcACMCodecParams codec_params;

    memcpy(&(codec_params.codecInstant), &send_codec, sizeof(CodecInst));
    codec_params.enableVAD = _vadEnabled;
    codec_params.enableDTX = _dtxEnabled;
    codec_params.vadMode = _vadMode;
    // Force initialization.
    status = codec_ptr->InitEncoder(&codec_params, true);

    // Check if VAD was turned on, or if error is reported.
    if (status == 1) {
      _vadEnabled = true;
    } else if (status < 0) {
      // Could not initialize the encoder.

      // Check if already have a registered codec.
      // Depending on that different messages are logged.
      if (!_sendCodecRegistered) {
        _currentSendCodecIdx = -1;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Cannot Initialize the encoder No Encoder is registered");
      } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Cannot Initialize the encoder, continue encoding with "
                     "the previously registered codec");
      }
      return -1;
    }

    // Everything is fine so we can replace the previous codec with this one.
    if (_sendCodecRegistered) {
      // If we change codec we start fresh with FEC.
      // This is not strictly required by the standard.
      _isFirstRED = true;

      if (codec_ptr->SetVAD(_dtxEnabled, _vadEnabled, _vadMode) < 0) {
        // SetVAD failed.
        _vadEnabled = false;
        _dtxEnabled = false;
      }
    }

    _currentSendCodecIdx = codec_id;
    _sendCodecRegistered = true;
    memcpy(&_sendCodecInst, &send_codec, sizeof(CodecInst));
    _previousPayloadType = _sendCodecInst.pltype;
    return 0;
  } else {
    // If codec is the same as already registered check if any parameters
    // has changed compared to the current values.
    // If any parameter is valid then apply it and record.
    bool force_init = false;

    if (mirror_id != codec_id) {
      _codecs[codec_id] = _codecs[mirror_id];
      _mirrorCodecIdx[codec_id] = mirror_id;
    }

    // Check the payload type.
    if (send_codec.pltype != _sendCodecInst.pltype) {
      // At this point check if the given payload type is valid.
      // Record it later when the sampling frequency is changed
      // successfully.
      if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Out of range payload type");
        return -1;
      }
    }

    // If there is a codec that ONE instance of codec supports multiple
    // sampling frequencies, then we need to take care of it here.
    // one such a codec is iSAC. Both WB and SWB are encoded and decoded
    // with one iSAC instance. Therefore, we need to update the encoder
    // frequency if required.
    if (_sendCodecInst.plfreq != send_codec.plfreq) {
      force_init = true;

      // If sampling frequency is changed we have to start fresh with RED.
      _isFirstRED = true;
    }

    // If packet size or number of channels has changed, we need to
    // re-initialize the encoder.
    if (_sendCodecInst.pacsize != send_codec.pacsize) {
      force_init = true;
    }
    if (_sendCodecInst.channels != send_codec.channels) {
      force_init = true;
    }

    if (force_init) {
      WebRtcACMCodecParams codec_params;

      memcpy(&(codec_params.codecInstant), &send_codec, sizeof(CodecInst));
      codec_params.enableVAD = _vadEnabled;
      codec_params.enableDTX = _dtxEnabled;
      codec_params.vadMode = _vadMode;

      // Force initialization.
      if (_codecs[_currentSendCodecIdx]->InitEncoder(&codec_params, true) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Could not change the codec packet-size.");
        return -1;
      }

      _sendCodecInst.plfreq = send_codec.plfreq;
      _sendCodecInst.pacsize = send_codec.pacsize;
      _sendCodecInst.channels = send_codec.channels;
    }

    // If the change of sampling frequency has been successful then
    // we store the payload-type.
    _sendCodecInst.pltype = send_codec.pltype;

    // Check if a change in Rate is required.
    if (send_codec.rate != _sendCodecInst.rate) {
      if (_codecs[codec_id]->SetBitRate(send_codec.rate) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Could not change the codec rate.");
        return -1;
      }
      _sendCodecInst.rate = send_codec.rate;
    }
    _previousPayloadType = _sendCodecInst.pltype;

    return 0;
  }
}

// Get current send codec.
WebRtc_Word32 AudioCodingModuleImpl::SendCodec(
    CodecInst& current_codec) const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
               "SendCodec()");
  CriticalSectionScoped lock(_acmCritSect);

  if (!_sendCodecRegistered) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
                 "SendCodec Failed, no codec is registered");

    return -1;
  }
  WebRtcACMCodecParams encoder_param;
  _codecs[_currentSendCodecIdx]->EncoderParams(&encoder_param);
  encoder_param.codecInstant.pltype = _sendCodecInst.pltype;
  memcpy(&current_codec, &(encoder_param.codecInstant), sizeof(CodecInst));

  return 0;
}

// Get current send frequency.
WebRtc_Word32 AudioCodingModuleImpl::SendFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
               "SendFrequency()");
  CriticalSectionScoped lock(_acmCritSect);

  if (!_sendCodecRegistered) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
                 "SendFrequency Failed, no codec is registered");

    return -1;
  }

  return _sendCodecInst.plfreq;
}

// Get encode bitrate.
// Adaptive rate codecs return their current encode target rate, while other
// codecs return there longterm avarage or their fixed rate.
WebRtc_Word32 AudioCodingModuleImpl::SendBitrate() const {
  CriticalSectionScoped lock(_acmCritSect);

  if (!_sendCodecRegistered) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
                 "SendBitrate Failed, no codec is registered");

    return -1;
  }

  WebRtcACMCodecParams encoder_param;
  _codecs[_currentSendCodecIdx]->EncoderParams(&encoder_param);

  return encoder_param.codecInstant.rate;
}

// Set available bandwidth, inform the encoder about the estimated bandwidth
// received from the remote party.
WebRtc_Word32 AudioCodingModuleImpl::SetReceivedEstimatedBandwidth(
    const WebRtc_Word32 bw) {
  return _codecs[_currentSendCodecIdx]->SetEstimatedBandwidth(bw);
}

// Register a transport callback which will be called to deliver
// the encoded buffers.
WebRtc_Word32 AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport) {
  CriticalSectionScoped lock(_callbackCritSect);
  _packetizationCallback = transport;
  return 0;
}

// Used by the module to deliver messages to the codec module/application
// AVT(DTMF).
WebRtc_Word32 AudioCodingModuleImpl::RegisterIncomingMessagesCallback(
#ifndef WEBRTC_DTMF_DETECTION
    AudioCodingFeedback* /* incoming_message */,
    const ACMCountries /* cpt */) {
  return -1;
#else
    AudioCodingFeedback* incoming_message,
    const ACMCountries cpt) {
  WebRtc_Word16 status = 0;

  // Enter the critical section for callback.
  {
    CriticalSectionScoped lock(_callbackCritSect);
    _dtmfCallback = incoming_message;
  }
  // Enter the ACM critical section to set up the DTMF class.
  {
    CriticalSectionScoped lock(_acmCritSect);
    // Check if the call is to disable or enable the callback.
    if (incoming_message == NULL) {
      // Callback is disabled, delete DTMF-detector class.
      if (_dtmfDetector != NULL) {
        delete _dtmfDetector;
        _dtmfDetector = NULL;
      }
      status = 0;
    } else {
      status = 0;
      if (_dtmfDetector == NULL) {
        _dtmfDetector = new (ACMDTMFDetection);
        if (_dtmfDetector == NULL) {
          status = -1;
        }
      }
      if (status >= 0) {
        status = _dtmfDetector->Enable(cpt);
        if (status < 0) {
          // Failed to initialize if DTMF-detection was not enabled before,
          // delete the class, and set the callback to NULL and return -1.
          delete _dtmfDetector;
          _dtmfDetector = NULL;
        }
      }
    }
  }
  // Check if we failed in setting up the DTMF-detector class.
  if ((status < 0)) {
    // We failed, we cannot have the callback.
    CriticalSectionScoped lock(_callbackCritSect);
    _dtmfCallback = NULL;
  }

  return status;
#endif
}

// Add 10MS of raw (PCM) audio data to the encoder.
WebRtc_Word32 AudioCodingModuleImpl::Add10MsData(
    const AudioFrame& audio_frame) {
  // Do we have a codec registered?
  CriticalSectionScoped lock(_acmCritSect);
  if (!HaveValidEncoder("Add10MsData")) {
    return -1;
  }

  if (audio_frame.samples_per_channel_ == 0) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Cannot Add 10 ms audio, payload length is zero");
    return -1;
  }
  // Allow for 8, 16, 32 and 48kHz input audio.
  if ((audio_frame.sample_rate_hz_ != 8000)
      && (audio_frame.sample_rate_hz_ != 16000)
      && (audio_frame.sample_rate_hz_ != 32000)
      && (audio_frame.sample_rate_hz_ != 48000)) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Cannot Add 10 ms audio, input frequency not valid");
    return -1;
  }

  // If the length and frequency matches. We currently just support raw PCM.
  if ((audio_frame.sample_rate_hz_ / 100)
      != audio_frame.samples_per_channel_) {
    WEBRTC_TRACE(
        webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
        "Cannot Add 10 ms audio, input frequency and length doesn't match");
    return -1;
  }

  // Calculate the timestamp that should be pushed to codec.
  // This might be different from the timestamp of the frame
  // due to re-sampling.
  bool resample = ((WebRtc_Word32) audio_frame.sample_rate_hz_
      != _sendCodecInst.plfreq);

  // If number of channels in audio doesn't match codec mode, we need
  // either mono-to-stereo or stereo-to-mono conversion.
  WebRtc_Word16 audio[WEBRTC_10MS_PCM_AUDIO];
  int audio_channels = _sendCodecInst.channels;
  // TODO(andrew): reuse RemixAndResample here? The upmixing should be done
  // after resampling. (Would require moving it somewhere common).
  if (audio_frame.num_channels_ != audio_channels) {
    if (audio_channels == 2) {
      // Do mono-to-stereo conversion by copying each sample.
      for (int k = 0; k < audio_frame.samples_per_channel_; k++) {
        audio[k * 2] = audio_frame.data_[k];
        audio[(k * 2) + 1] = audio_frame.data_[k];
      }
    } else if (audio_channels == 1) {
      // Do stereo-to-mono conversion by creating the average of the stereo
      // samples.
      for (int k = 0; k < audio_frame.samples_per_channel_; k++) {
        audio[k] = (audio_frame.data_[k * 2]
            + audio_frame.data_[(k * 2) + 1]) >> 1;
      }
    }
  } else {
    // Copy payload data for future use.
    size_t length = static_cast<size_t>(audio_frame.samples_per_channel_
        * audio_channels);
    memcpy(audio, audio_frame.data_, length * sizeof(WebRtc_UWord16));
  }

  WebRtc_UWord32 current_timestamp;
  WebRtc_Word32 status;
  // If it is required, we have to do a resampling.
  if (resample) {
    WebRtc_Word16 resampled_audio[WEBRTC_10MS_PCM_AUDIO];
    WebRtc_Word32 send_freq = _sendCodecInst.plfreq;
    WebRtc_UWord32 timestamp_diff;
    WebRtc_Word16 new_length;

    // Calculate the timestamp of this frame.
    if (_lastInTimestamp > audio_frame.timestamp_) {
      // A wrap around has happened.
      timestamp_diff = ((WebRtc_UWord32) 0xFFFFFFFF - _lastInTimestamp)
          + audio_frame.timestamp_;
    } else {
      timestamp_diff = audio_frame.timestamp_ - _lastInTimestamp;
    }
    current_timestamp = _lastTimestamp + (WebRtc_UWord32)(timestamp_diff *
        ((double) _sendCodecInst.plfreq / (double) audio_frame.sample_rate_hz_));

    new_length = _inputResampler.Resample10Msec(audio,
                                                audio_frame.sample_rate_hz_,
                                                resampled_audio, send_freq,
                                                audio_channels);

    if (new_length < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Cannot add 10 ms audio, resmapling failed");
      return -1;
    }
    status = _codecs[_currentSendCodecIdx]->Add10MsData(current_timestamp,
                                                        resampled_audio,
                                                        new_length,
                                                        audio_channels);
  } else {
    current_timestamp = audio_frame.timestamp_;

    status = _codecs[_currentSendCodecIdx]->Add10MsData(
        current_timestamp, audio, audio_frame.samples_per_channel_,
        audio_channels);
  }
  _lastInTimestamp = audio_frame.timestamp_;
  _lastTimestamp = current_timestamp;
  return status;
}

/////////////////////////////////////////
//   (FEC) Forward Error Correction
//

bool AudioCodingModuleImpl::FECStatus() const {
  CriticalSectionScoped lock(_acmCritSect);
  return _fecEnabled;
}

// Configure FEC status i.e on/off.
WebRtc_Word32
AudioCodingModuleImpl::SetFECStatus(
#ifdef WEBRTC_CODEC_RED
    const bool enable_fec) {
  CriticalSectionScoped lock(_acmCritSect);

  if (_fecEnabled != enable_fec) {
    // Reset the RED buffer.
    memset(_redBuffer, 0, MAX_PAYLOAD_SIZE_BYTE);

    // Reset fragmentation buffers.
    _fragmentation->fragmentationVectorSize = 2;
    _fragmentation->fragmentationOffset[0] = 0;
    _fragmentation->fragmentationOffset[1] = MAX_PAYLOAD_SIZE_BYTE;
    memset(_fragmentation->fragmentationLength, 0, sizeof(WebRtc_UWord32) * 2);
    memset(_fragmentation->fragmentationTimeDiff, 0,
           sizeof(WebRtc_UWord16) * 2);
    memset(_fragmentation->fragmentationPlType, 0, sizeof(WebRtc_UWord8) * 2);

    // Set _fecEnabled.
    _fecEnabled = enable_fec;
  }
  _isFirstRED = true;  // Make sure we restart FEC.
  return 0;
#else
    const bool /* enable_fec */) {
  _fecEnabled = false;
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
               "  WEBRTC_CODEC_RED is undefined => _fecEnabled = %d",
               _fecEnabled);
  return -1;
#endif
}

/////////////////////////////////////////
//   (VAD) Voice Activity Detection
//

WebRtc_Word32 AudioCodingModuleImpl::SetVAD(const bool enable_dtx,
                                            const bool enable_vad,
                                            const ACMVADMode mode) {
  CriticalSectionScoped lock(_acmCritSect);

  // Sanity check of the mode.
  if ((mode != VADNormal) && (mode != VADLowBitrate)
      && (mode != VADAggr) && (mode != VADVeryAggr)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Invalid VAD Mode %d, no change is made to VAD/DTX status",
                 (int) mode);
    return -1;
  }

  // Check that the send codec is mono. We don't support VAD/DTX for stereo
  // sending.
  if ((enable_dtx || enable_vad) && _stereoSend) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "VAD/DTX not supported for stereo sending");
    return -1;
  }

  // If a send codec is registered, set VAD/DTX for the codec.
  if (HaveValidEncoder("SetVAD")) {
    WebRtc_Word16 status = _codecs[_currentSendCodecIdx]->SetVAD(enable_dtx,
                                                                 enable_vad,
                                                                 mode);
    if (status == 1) {
      // Vad was enabled.
      _vadEnabled = true;
      _dtxEnabled = enable_dtx;
      _vadMode = mode;

      return 0;
    } else if (status < 0) {
      // SetVAD failed.
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "SetVAD failed");

      _vadEnabled = false;
      _dtxEnabled = false;

      return -1;
    }
  }

  _vadEnabled = enable_vad;
  _dtxEnabled = enable_dtx;
  _vadMode = mode;

  return 0;
}

// Get VAD/DTX settings.
WebRtc_Word32 AudioCodingModuleImpl::VAD(bool& dtx_enabled, bool& vad_enabled,
                                         ACMVADMode& mode) const {
  CriticalSectionScoped lock(_acmCritSect);

  dtx_enabled = _dtxEnabled;
  vad_enabled = _vadEnabled;
  mode = _vadMode;

  return 0;
}

/////////////////////////////////////////
//   Receiver
//

WebRtc_Word32 AudioCodingModuleImpl::InitializeReceiver() {
  CriticalSectionScoped lock(_acmCritSect);
  return InitializeReceiverSafe();
}

// Initialize receiver, resets codec database etc.
WebRtc_Word32 AudioCodingModuleImpl::InitializeReceiverSafe() {
  // If the receiver is already initialized then we want to destroy any
  // existing decoders. After a call to this function, we should have a clean
  // start-up.
  if (_receiverInitialized) {
    for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
      if (UnregisterReceiveCodecSafe(i) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "InitializeReceiver() failed, Could not unregister codec");
        return -1;
      }
    }
  }
  if (_netEq.Init() != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "InitializeReceiver() failed, Could not initialize NetEQ");
    return -1;
  }
  _netEq.SetUniqueId(_id);
  if (_netEq.AllocatePacketBuffer(ACMCodecDB::NetEQDecoders(),
                                  ACMCodecDB::kNumCodecs) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "NetEQ cannot allocatePacket Buffer");
    return -1;
  }

  // Register RED and CN.
  for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
    if (IsCodecRED(i) || IsCodecCN(i)) {
      if (RegisterRecCodecMSSafe(ACMCodecDB::database_[i], i, i,
                                 ACMNetEQ::masterJB) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Cannot register master codec.");
        return -1;
      }
      _registeredPlTypes[i] = ACMCodecDB::database_[i].pltype;
    }
  }

  _receiverInitialized = true;
  return 0;
}

// Reset the decoder state.
WebRtc_Word32 AudioCodingModuleImpl::ResetDecoder() {
  CriticalSectionScoped lock(_acmCritSect);

  for (int id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if ((_codecs[id] != NULL) && (_registeredPlTypes[id] != -1)) {
      if (_codecs[id]->ResetDecoder(_registeredPlTypes[id]) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "ResetDecoder failed:");
        return -1;
      }
    }
  }
  return _netEq.FlushBuffers();
}

// Get current receive frequency.
WebRtc_Word32 AudioCodingModuleImpl::ReceiveFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
               "ReceiveFrequency()");
  WebRtcACMCodecParams codec_params;

  CriticalSectionScoped lock(_acmCritSect);
  if (DecoderParamByPlType(_lastRecvAudioCodecPlType, codec_params) < 0) {
    return _netEq.CurrentSampFreqHz();
  } else {
    return codec_params.codecInstant.plfreq;
  }
}

// Get current playout frequency.
WebRtc_Word32 AudioCodingModuleImpl::PlayoutFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
               "PlayoutFrequency()");

  CriticalSectionScoped lock(_acmCritSect);

  return _netEq.CurrentSampFreqHz();
}

// Register possible reveive codecs, can be called multiple times,
// for codecs, CNG (NB, WB and SWB), DTMF, RED.
WebRtc_Word32 AudioCodingModuleImpl::RegisterReceiveCodec(
    const CodecInst& receive_codec) {
  CriticalSectionScoped lock(_acmCritSect);

  if (receive_codec.channels > 2) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "More than 2 audio channel is not supported.");
    return -1;
  }

  int mirror_id;
  int codec_id = ACMCodecDB::ReceiverCodecNumber(&receive_codec, &mirror_id);

  if (codec_id < 0 || codec_id >= ACMCodecDB::kNumCodecs) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Wrong codec params to be registered as receive codec");
    return -1;
  }
  // Check if the payload-type is valid.
  if (!ACMCodecDB::ValidPayloadType(receive_codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Invalid payload-type %d for %s.", receive_codec.pltype,
                 receive_codec.plname);
    return -1;
  }

  if (!_receiverInitialized) {
    if (InitializeReceiverSafe() < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Cannot initialize reciver, so failed registering a codec.");
      return -1;
    }
  }

  // If codec already registered, unregister. Except for CN where we only
  // unregister if payload type is changing.
  if ((_registeredPlTypes[codec_id] == receive_codec.pltype)
      && IsCodecCN(&receive_codec)) {
    // Codec already registered as receiver with this payload type. Nothing
    // to be done.
    return 0;
  } else if (_registeredPlTypes[codec_id] != -1) {
    if (UnregisterReceiveCodecSafe(codec_id) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Cannot register master codec.");
      return -1;
    }
  }

  if (RegisterRecCodecMSSafe(receive_codec, codec_id, mirror_id,
                             ACMNetEQ::masterJB) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Cannot register master codec.");
    return -1;
  }

  // TODO(andrew): Refactor how the slave is initialized. Can we instead
  // always start up a slave and pre-register CN and RED? We should be able
  // to get rid of _stereoReceiveRegistered.
  // http://code.google.com/p/webrtc/issues/detail?id=453

  // Register stereo codecs with the slave, or, if we've had already seen a
  // stereo codec, register CN or RED as a special case.
  if (receive_codec.channels == 2 ||
      (_stereoReceiveRegistered && (IsCodecCN(&receive_codec) ||
          IsCodecRED(&receive_codec)))) {
    // TODO(andrew): refactor this block to combine with InitStereoSlave().

    if (!_stereoReceiveRegistered) {
      // This is the first time a stereo codec has been registered. Make
      // some stereo preparations.

      // Add a stereo slave.
      assert(_netEq.NumSlaves() == 0);
      if (_netEq.AddSlave(ACMCodecDB::NetEQDecoders(),
                          ACMCodecDB::kNumCodecs) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Cannot add slave jitter buffer to NetEQ.");
        return -1;
      }

      // Register any existing CN or RED codecs with the slave and as stereo.
      for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
        if (_registeredPlTypes[i] != -1 && (IsCodecRED(i) || IsCodecCN(i))) {
          _stereoReceive[i] = true;

          CodecInst codec;
          memcpy(&codec, &ACMCodecDB::database_[i], sizeof(CodecInst));
          codec.pltype = _registeredPlTypes[i];
          if (RegisterRecCodecMSSafe(codec, i, i, ACMNetEQ::slaveJB) < 0) {
            WEBRTC_TRACE(kTraceError, kTraceAudioCoding, _id,
                         "Cannot register slave codec.");
            return -1;
          }
        }
      }
    }

    if (RegisterRecCodecMSSafe(receive_codec, codec_id, mirror_id,
                               ACMNetEQ::slaveJB) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Cannot register slave codec.");
      return -1;
    }

    if (!_stereoReceive[codec_id]
        && (_lastRecvAudioCodecPlType == receive_codec.pltype)) {
      // The last received payload type is the same as the current one, but
      // was marked as mono. Reset to avoid problems.
      _lastRecvAudioCodecPlType = -1;
    }

    _stereoReceive[codec_id] = true;
    _stereoReceiveRegistered = true;
  } else {
    _stereoReceive[codec_id] = false;
  }

  _registeredPlTypes[codec_id] = receive_codec.pltype;

  if (IsCodecRED(&receive_codec)) {
    _receiveREDPayloadType = receive_codec.pltype;
  }
  return 0;
}

WebRtc_Word32 AudioCodingModuleImpl::RegisterRecCodecMSSafe(
    const CodecInst& receive_codec, WebRtc_Word16 codec_id,
    WebRtc_Word16 mirror_id, ACMNetEQ::JB jitter_buffer) {
  ACMGenericCodec** codecs;
  if (jitter_buffer == ACMNetEQ::masterJB) {
    codecs = &_codecs[0];
  } else if (jitter_buffer == ACMNetEQ::slaveJB) {
    codecs = &_slaveCodecs[0];
    if (_codecs[codec_id]->IsTrueStereoCodec()) {
      // True stereo codecs need to use the same codec memory
      // for both master and slave.
      _slaveCodecs[mirror_id] = _codecs[mirror_id];
      _mirrorCodecIdx[mirror_id] = mirror_id;
    }
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "RegisterReceiveCodecMSSafe failed, jitter_buffer is neither "
                 "master or slave ");
    return -1;
  }

  if (codecs[mirror_id] == NULL) {
    codecs[mirror_id] = CreateCodec(receive_codec);
    if (codecs[mirror_id] == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "Cannot create codec to register as receive codec");
      return -1;
    }
    _mirrorCodecIdx[mirror_id] = mirror_id;
  }
  if (mirror_id != codec_id) {
    codecs[codec_id] = codecs[mirror_id];
    _mirrorCodecIdx[codec_id] = mirror_id;
  }

  codecs[codec_id]->SetIsMaster(jitter_buffer == ACMNetEQ::masterJB);

  WebRtc_Word16 status = 0;
  WebRtcACMCodecParams codec_params;
  memcpy(&(codec_params.codecInstant), &receive_codec, sizeof(CodecInst));
  codec_params.enableVAD = false;
  codec_params.enableDTX = false;
  codec_params.vadMode = VADNormal;
  if (!codecs[codec_id]->DecoderInitialized()) {
    // Force initialization.
    status = codecs[codec_id]->InitDecoder(&codec_params, true);
    if (status < 0) {
      // Could not initialize the decoder, we don't want to
      // continue if we could not initialize properly.
      WEBRTC_TRACE(
          webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
          "could not initialize the receive codec, codec not registered");

      return -1;
    }
  } else if (mirror_id != codec_id) {
    // Currently this only happens for iSAC.
    // We have to store the decoder parameters.
    codecs[codec_id]->SaveDecoderParam(&codec_params);
  }

  if (codecs[codec_id]->RegisterInNetEq(&_netEq, receive_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Receive codec could not be registered in NetEQ");
      return -1;
  }
  // Guarantee that the same payload-type that is
  // registered in NetEQ is stored in the codec.
  codecs[codec_id]->SaveDecoderParam(&codec_params);

  return status;
}

// Get current received codec.
WebRtc_Word32 AudioCodingModuleImpl::ReceiveCodec(
    CodecInst& current_codec) const {
  WebRtcACMCodecParams decoderParam;
  CriticalSectionScoped lock(_acmCritSect);

  for (int id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if (_codecs[id] != NULL) {
      if (_codecs[id]->DecoderInitialized()) {
        if (_codecs[id]->DecoderParams(&decoderParam,
                                       _lastRecvAudioCodecPlType)) {
          memcpy(&current_codec, &decoderParam.codecInstant, sizeof(CodecInst));
          return 0;
        }
      }
    }
  }

  // If we are here then we haven't found any codec. Set codec pltype to -1 to
  // indicate that the structure is invalid and return -1.
  current_codec.pltype = -1;
  return -1;
}

// Incoming packet from network parsed and ready for decode.
WebRtc_Word32 AudioCodingModuleImpl::IncomingPacket(
    const WebRtc_UWord8* incoming_payload,
    const WebRtc_Word32 payload_length,
    const WebRtcRTPHeader& rtp_info) {
  WebRtcRTPHeader rtp_header;

  memcpy(&rtp_header, &rtp_info, sizeof(WebRtcRTPHeader));

  if (payload_length < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "IncomingPacket() Error, payload-length cannot be negative");
    return -1;
  }
  {
    // Store the payload Type. This will be used to retrieve "received codec"
    // and "received frequency."
    CriticalSectionScoped lock(_acmCritSect);
#ifdef ACM_QA_TEST
    if(_incomingPL != NULL) {
      if (fwrite(&rtp_info.header.timestamp, sizeof(WebRtc_UWord32),
                 1, _incomingPL) != 1) {
        return -1;
      }
      if (fwrite(&rtp_info.header.payloadType, sizeof(WebRtc_UWord8),
                 1, _incomingPL) != 1) {
        return -1;
      }
      if (fwrite(&payload_length, sizeof(WebRtc_Word16),
                 1, _incomingPL) != 1) {
        return -1;
      }
    }
#endif

    WebRtc_UWord8 myPayloadType;

    // Check if this is an RED payload.
    if (rtp_info.header.payloadType == _receiveREDPayloadType) {
      // Get the primary payload-type.
      myPayloadType = incoming_payload[0] & 0x7F;
    } else {
      myPayloadType = rtp_info.header.payloadType;
    }

    // If payload is audio, check if received payload is different from
    // previous.
    if (!rtp_info.type.Audio.isCNG) {
      // This is Audio not CNG.

      if (myPayloadType != _lastRecvAudioCodecPlType) {
        // We detect a change in payload type. It is necessary for iSAC
        // we are going to use ONE iSAC instance for decoding both WB and
        // SWB payloads. If payload is changed there might be a need to reset
        // sampling rate of decoder. depending what we have received "now".
        for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
          if (_registeredPlTypes[i] == myPayloadType) {
            if (UpdateUponReceivingCodec(i) != 0)
              return -1;
            break;
          }
        }
      }
      _lastRecvAudioCodecPlType = myPayloadType;
    }
  }

  // Split the payload for stereo packets, so that first half of payload
  // vector holds left channel, and second half holds right channel.
  if (_expected_channels == 2) {
    if (!rtp_info.type.Audio.isCNG) {
      // Create a new vector for the payload, maximum payload size.
      WebRtc_Word32 length = payload_length;
      WebRtc_UWord8 payload[kMaxPacketSize];
      assert(payload_length <= kMaxPacketSize);
      memcpy(payload, incoming_payload, payload_length);
      _codecs[_current_receive_codec_idx]->SplitStereoPacket(payload, &length);
      rtp_header.type.Audio.channel = 2;
      // Insert packet into NetEQ.
      return _netEq.RecIn(payload, length, rtp_header);
    } else {
      // If we receive a CNG packet while expecting stereo, we ignore the packet
      // and continue. CNG is not supported for stereo.
      return 0;
    }
  } else {
    return _netEq.RecIn(incoming_payload, payload_length, rtp_header);
  }
}

int AudioCodingModuleImpl::UpdateUponReceivingCodec(int index) {
  if (_codecs[index] == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceAudioCoding, _id,
        "IncomingPacket() error: payload type found but corresponding codec "
        "is NULL");
    return -1;
  }
  _codecs[index]->UpdateDecoderSampFreq(index);
  _netEq.SetReceivedStereo(_stereoReceive[index]);
  _current_receive_codec_idx = index;

  // If we have a change in the expected number of channels, flush packet
  // buffers in NetEQ.
  if ((_stereoReceive[index] && (_expected_channels == 1)) ||
      (!_stereoReceive[index] && (_expected_channels == 2))) {
    _netEq.FlushBuffers();
    _codecs[index]->ResetDecoder(_registeredPlTypes[index]);
  }

  if (_stereoReceive[index] && (_expected_channels == 1)) {
    // When switching from a mono to stereo codec reset the slave.
    if (InitStereoSlave() != 0)
      return -1;
  }

  // Store number of channels we expect to receive for the current payload type.
  if (_stereoReceive[index]) {
    _expected_channels = 2;
  } else {
    _expected_channels = 1;
  }

  // Reset previous received channel.
  _prev_received_channel = 0;
  return 0;
}

bool AudioCodingModuleImpl::IsCodecForSlave(int index) const {
  return (_registeredPlTypes[index] != -1 && _stereoReceive[index]);
}

bool AudioCodingModuleImpl::IsCodecRED(int index) const {
  return (IsCodecRED(&ACMCodecDB::database_[index]));
}

bool AudioCodingModuleImpl::IsCodecRED(const CodecInst* codec) const {
  return (STR_CASE_CMP(codec->plname, "RED") == 0);
}

bool AudioCodingModuleImpl::IsCodecCN(int index) const {
  return (IsCodecCN(&ACMCodecDB::database_[index]));
}

bool AudioCodingModuleImpl::IsCodecCN(const CodecInst* codec) const {
  return (STR_CASE_CMP(codec->plname, "CN") == 0);
}

int AudioCodingModuleImpl::InitStereoSlave() {
  _netEq.RemoveSlaves();

  if (_netEq.AddSlave(ACMCodecDB::NetEQDecoders(),
                      ACMCodecDB::kNumCodecs) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Cannot add slave jitter buffer to NetEQ.");
    return -1;
  }

  // Register all needed codecs with slave.
  for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
    if (_codecs[i] != NULL && IsCodecForSlave(i)) {
      WebRtcACMCodecParams decoder_params;
      if (_codecs[i]->DecoderParams(&decoder_params, _registeredPlTypes[i])) {
        if (RegisterRecCodecMSSafe(decoder_params.codecInstant,
                                   i, ACMCodecDB::MirrorID(i),
                                   ACMNetEQ::slaveJB) < 0) {
            WEBRTC_TRACE(kTraceError, kTraceAudioCoding, _id,
                         "Cannot register slave codec.");
            return -1;
        }
      }
    }
  }
  return 0;
}

// Minimum playout delay (Used for lip-sync).
WebRtc_Word32 AudioCodingModuleImpl::SetMinimumPlayoutDelay(
    const WebRtc_Word32 time_ms) {
  if ((time_ms < 0) || (time_ms > 1000)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Delay must be in the range of 0-1000 milliseconds.");
    return -1;
  }
  return _netEq.SetExtraDelay(time_ms);
}

// Get Dtmf playout status.
bool AudioCodingModuleImpl::DtmfPlayoutStatus() const {
#ifndef WEBRTC_CODEC_AVT
  return false;
#else
  return _netEq.AVTPlayout();
#endif
}

// Configure Dtmf playout status i.e on/off playout the incoming outband
// Dtmf tone.
WebRtc_Word32 AudioCodingModuleImpl::SetDtmfPlayoutStatus(
#ifndef WEBRTC_CODEC_AVT
    const bool /* enable */) {
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
               "SetDtmfPlayoutStatus() failed: AVT is not supported.");
  return -1;
#else
    const bool enable) {
  return _netEq.SetAVTPlayout(enable);
#endif
}

// Estimate the Bandwidth based on the incoming stream, needed for one way
// audio where the RTCP send the BW estimate.
// This is also done in the RTP module.
WebRtc_Word32 AudioCodingModuleImpl::DecoderEstimatedBandwidth() const {
  CodecInst codec;
  WebRtc_Word16 codec_id = -1;
  int payloadtype_wb;
  int payloadtype_swb;

  // Get iSAC settings.
  for (int id = 0; id < ACMCodecDB::kNumCodecs; id++) {
    // Store codec settings for codec number "codeCntr" in the output struct.
    ACMCodecDB::Codec(id, &codec);

    if (!STR_CASE_CMP(codec.plname, "isac")) {
      codec_id = 1;
      payloadtype_wb = codec.pltype;

      ACMCodecDB::Codec(id + 1, &codec);
      payloadtype_swb = codec.pltype;

      break;
    }
  }

  if (codec_id < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "DecoderEstimatedBandwidth failed");
    return -1;
  }

  if ((_lastRecvAudioCodecPlType == payloadtype_wb) ||
      (_lastRecvAudioCodecPlType == payloadtype_swb)) {
    return _codecs[codec_id]->GetEstimatedBandwidth();
  } else {
    return -1;
  }
}

// Set playout mode for: voice, fax, or streaming.
WebRtc_Word32 AudioCodingModuleImpl::SetPlayoutMode(
    const AudioPlayoutMode mode) {
  if ((mode != voice) && (mode != fax) && (mode != streaming)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Invalid playout mode.");
    return -1;
  }
  return _netEq.SetPlayoutMode(mode);
}

// Get playout mode voice, fax.
AudioPlayoutMode AudioCodingModuleImpl::PlayoutMode() const {
  return _netEq.PlayoutMode();
}

// Get 10 milliseconds of raw audio data to play out.
// Automatic resample to the requested frequency.
WebRtc_Word32 AudioCodingModuleImpl::PlayoutData10Ms(
    const WebRtc_Word32 desired_freq_hz, AudioFrame& audio_frame) {
  bool stereo_mode;

  // RecOut always returns 10 ms.
  if (_netEq.RecOut(_audioFrame) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "PlayoutData failed, RecOut Failed");
    return -1;
  }

  audio_frame.num_channels_ = _audioFrame.num_channels_;
  audio_frame.vad_activity_ = _audioFrame.vad_activity_;
  audio_frame.speech_type_ = _audioFrame.speech_type_;

  stereo_mode = (_audioFrame.num_channels_ > 1);
  // For stereo playout:
  // Master and Slave samples are interleaved starting with Master.

  const WebRtc_UWord16 receive_freq =
      static_cast<WebRtc_UWord16>(_audioFrame.sample_rate_hz_);
  bool tone_detected = false;
  WebRtc_Word16 last_detected_tone;
  WebRtc_Word16 tone;

  // Limit the scope of ACM Critical section.
  {
    CriticalSectionScoped lock(_acmCritSect);

    if ((receive_freq != desired_freq_hz) && (desired_freq_hz != -1)) {
      // Resample payloadData.
      WebRtc_Word16 temp_len = _outputResampler.Resample10Msec(
          _audioFrame.data_, receive_freq, audio_frame.data_,
          desired_freq_hz, _audioFrame.num_channels_);

      if (temp_len < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "PlayoutData failed, resampler failed");
        return -1;
      }

      // Set the payload data length from the resampler.
      audio_frame.samples_per_channel_ = (WebRtc_UWord16) temp_len;
      // Set the sampling frequency.
      audio_frame.sample_rate_hz_ = desired_freq_hz;
    } else {
      memcpy(audio_frame.data_, _audioFrame.data_,
             _audioFrame.samples_per_channel_ * audio_frame.num_channels_
             * sizeof(WebRtc_Word16));
      // Set the payload length.
      audio_frame.samples_per_channel_ =
          _audioFrame.samples_per_channel_;
      // Set the sampling frequency.
      audio_frame.sample_rate_hz_ = receive_freq;
    }

    // Tone detection done for master channel.
    if (_dtmfDetector != NULL) {
      // Dtmf Detection.
      if (audio_frame.sample_rate_hz_ == 8000) {
        // Use audio_frame.data_ then Dtmf detector doesn't
        // need resampling.
        if (!stereo_mode) {
          _dtmfDetector->Detect(audio_frame.data_,
                                audio_frame.samples_per_channel_,
                                audio_frame.sample_rate_hz_, tone_detected,
                                tone);
        } else {
          // We are in 8 kHz so the master channel needs only 80 samples.
          WebRtc_Word16 master_channel[80];
          for (int n = 0; n < 80; n++) {
            master_channel[n] = audio_frame.data_[n << 1];
          }
          _dtmfDetector->Detect(master_channel,
                                audio_frame.samples_per_channel_,
                                audio_frame.sample_rate_hz_, tone_detected,
                                tone);
        }
      } else {
        // Do the detection on the audio that we got from NetEQ (_audioFrame).
        if (!stereo_mode) {
          _dtmfDetector->Detect(_audioFrame.data_,
                                _audioFrame.samples_per_channel_,
                                receive_freq, tone_detected, tone);
        } else {
          WebRtc_Word16 master_channel[WEBRTC_10MS_PCM_AUDIO];
          for (int n = 0; n < _audioFrame.samples_per_channel_; n++) {
            master_channel[n] = _audioFrame.data_[n << 1];
          }
          _dtmfDetector->Detect(master_channel,
                                _audioFrame.samples_per_channel_,
                                receive_freq, tone_detected, tone);
        }
      }
    }

    // We want to do this while we are in _acmCritSect.
    // (Doesn't really need to initialize the following
    // variable but Linux complains if we don't.)
    last_detected_tone = kACMToneEnd;
    if (tone_detected) {
      last_detected_tone = _lastDetectedTone;
      _lastDetectedTone = tone;
    }
  }

  if (tone_detected) {
    // We will deal with callback here, so enter callback critical section.
    CriticalSectionScoped lock(_callbackCritSect);

    if (_dtmfCallback != NULL) {
      if (tone != kACMToneEnd) {
        // just a tone
        _dtmfCallback->IncomingDtmf((WebRtc_UWord8) tone, false);
      } else if ((tone == kACMToneEnd) && (last_detected_tone != kACMToneEnd)) {
        // The tone is "END" and the previously detected tone is
        // not "END," so call fir an end.
        _dtmfCallback->IncomingDtmf((WebRtc_UWord8) last_detected_tone, true);
      }
    }
  }

  audio_frame.id_ = _id;
  audio_frame.energy_ = -1;
  audio_frame.timestamp_ = 0;

  return 0;
}

/////////////////////////////////////////
//   (CNG) Comfort Noise Generation
//   Generate comfort noise when receiving DTX packets
//

// Get VAD aggressiveness on the incoming stream
ACMVADMode AudioCodingModuleImpl::ReceiveVADMode() const {
  return _netEq.VADMode();
}

// Configure VAD aggressiveness on the incoming stream.
WebRtc_Word16 AudioCodingModuleImpl::SetReceiveVADMode(const ACMVADMode mode) {
  return _netEq.SetVADMode(mode);
}

/////////////////////////////////////////
//   Statistics
//

WebRtc_Word32 AudioCodingModuleImpl::NetworkStatistics(
    ACMNetworkStatistics& statistics) const {
  WebRtc_Word32 status;
  status = _netEq.NetworkStatistics(&statistics);
  return status;
}


void AudioCodingModuleImpl::DestructEncoderInst(void* inst) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
               "DestructEncoderInst()");
  if (!HaveValidEncoder("DestructEncoderInst")) {
    return;
  }

  _codecs[_currentSendCodecIdx]->DestructEncoderInst(inst);
}

WebRtc_Word16 AudioCodingModuleImpl::AudioBuffer(
    WebRtcACMAudioBuff& buffer) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
               "AudioBuffer()");
  if (!HaveValidEncoder("AudioBuffer")) {
    return -1;
  }
  buffer.lastInTimestamp = _lastInTimestamp;
  return _codecs[_currentSendCodecIdx]->AudioBuffer(buffer);
}

WebRtc_Word16 AudioCodingModuleImpl::SetAudioBuffer(
    WebRtcACMAudioBuff& buffer) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
               "SetAudioBuffer()");
  if (!HaveValidEncoder("SetAudioBuffer")) {
    return -1;
  }
  return _codecs[_currentSendCodecIdx]->SetAudioBuffer(buffer);
}

WebRtc_UWord32 AudioCodingModuleImpl::EarliestTimestamp() const {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
               "EarliestTimestamp()");
  if (!HaveValidEncoder("EarliestTimestamp")) {
    return -1;
  }
  return _codecs[_currentSendCodecIdx]->EarliestTimestamp();
}

WebRtc_Word32 AudioCodingModuleImpl::RegisterVADCallback(
    ACMVADCallback* vad_callback) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
               "RegisterVADCallback()");
  CriticalSectionScoped lock(_callbackCritSect);
  _vadCallback = vad_callback;
  return 0;
}

// TODO(tlegrand): Modify this function to work for stereo, and add tests.
WebRtc_Word32 AudioCodingModuleImpl::IncomingPayload(
    const WebRtc_UWord8* incoming_payload, const WebRtc_Word32 payload_length,
    const WebRtc_UWord8 payload_type, const WebRtc_UWord32 timestamp) {
  if (payload_length < 0) {
    // Log error in trace file.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "IncomingPacket() Error, payload-length cannot be negative");
    return -1;
  }

  if (_dummyRTPHeader == NULL) {
    // This is the first time that we are using _dummyRTPHeader
    // so we have to create it.
    WebRtcACMCodecParams codec_params;
    _dummyRTPHeader = new WebRtcRTPHeader;
    if (_dummyRTPHeader == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                   "IncomingPacket() Error, out of memory");
      return -1;
    }
    _dummyRTPHeader->header.payloadType = payload_type;
    // Don't matter in this case.
    _dummyRTPHeader->header.ssrc = 0;
    _dummyRTPHeader->header.markerBit = false;
    // Start with random numbers.
    _dummyRTPHeader->header.sequenceNumber = rand();
    _dummyRTPHeader->header.timestamp = (((WebRtc_UWord32) rand()) << 16)
        + (WebRtc_UWord32) rand();
    _dummyRTPHeader->type.Audio.channel = 1;

    if (DecoderParamByPlType(payload_type, codec_params) < 0) {
      // We didn't find a codec with the given payload.
      // Something is wrong we exit, but we delete _dummyRTPHeader
      // and set it to NULL to start clean next time.
      delete _dummyRTPHeader;
      _dummyRTPHeader = NULL;
      return -1;
    }
    _recvPlFrameSizeSmpls = codec_params.codecInstant.pacsize;
  }

  if (payload_type != _dummyRTPHeader->header.payloadType) {
    // Payload type has changed since the last time we might need to
    // update the frame-size.
    WebRtcACMCodecParams codec_params;
    if (DecoderParamByPlType(payload_type, codec_params) < 0) {
      // We didn't find a codec with the given payload.
      return -1;
    }
    _recvPlFrameSizeSmpls = codec_params.codecInstant.pacsize;
    _dummyRTPHeader->header.payloadType = payload_type;
  }

  if (timestamp > 0) {
    _dummyRTPHeader->header.timestamp = timestamp;
  }

  // Store the payload Type. this will be used to retrieve "received codec"
  // and "received frequency."
  _lastRecvAudioCodecPlType = payload_type;

  // Insert in NetEQ.
  if (_netEq.RecIn(incoming_payload, payload_length, (*_dummyRTPHeader)) < 0) {
    return -1;
  }

  // Get ready for the next payload.
  _dummyRTPHeader->header.sequenceNumber++;
  _dummyRTPHeader->header.timestamp += _recvPlFrameSizeSmpls;
  return 0;
}

WebRtc_Word16 AudioCodingModuleImpl::DecoderParamByPlType(
    const WebRtc_UWord8 payload_type,
    WebRtcACMCodecParams& codec_params) const {
  CriticalSectionScoped lock(_acmCritSect);
  for (WebRtc_Word16 id = 0; id < ACMCodecDB::kMaxNumCodecs;
      id++) {
    if (_codecs[id] != NULL) {
      if (_codecs[id]->DecoderInitialized()) {
        if (_codecs[id]->DecoderParams(&codec_params, payload_type)) {
          return 0;
        }
      }
    }
  }
  // If we are here it means that we could not find a
  // codec with that payload type. reset the values to
  // not acceptable values and return -1.
  codec_params.codecInstant.plname[0] = '\0';
  codec_params.codecInstant.pacsize = 0;
  codec_params.codecInstant.rate = 0;
  codec_params.codecInstant.pltype = -1;
  return -1;
}

WebRtc_Word16 AudioCodingModuleImpl::DecoderListIDByPlName(
    const char* name, const WebRtc_UWord16 frequency) const {
  WebRtcACMCodecParams codec_params;
  CriticalSectionScoped lock(_acmCritSect);
  for (WebRtc_Word16 id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if ((_codecs[id] != NULL)) {
      if (_codecs[id]->DecoderInitialized()) {
        assert(_registeredPlTypes[id] >= 0);
        assert(_registeredPlTypes[id] <= 255);
        _codecs[id]->DecoderParams(
            &codec_params, (WebRtc_UWord8) _registeredPlTypes[id]);
        if (!STR_CASE_CMP(codec_params.codecInstant.plname, name)) {
          // Check if the given sampling frequency matches.
          // A zero sampling frequency means we matching the names
          // is sufficient and we don't need to check for the
          // frequencies.
          // Currently it is only iSAC which has one name but two
          // sampling frequencies.
          if ((frequency == 0)||
              (codec_params.codecInstant.plfreq == frequency)) {
            return id;
          }
        }
      }
    }
  }
  // If we are here it means that we could not find a
  // codec with that payload type. return -1.
  return -1;
}

WebRtc_Word32 AudioCodingModuleImpl::LastEncodedTimestamp(
    WebRtc_UWord32& timestamp) const {
  CriticalSectionScoped lock(_acmCritSect);
  if (!HaveValidEncoder("LastEncodedTimestamp")) {
    return -1;
  }
  timestamp = _codecs[_currentSendCodecIdx]->LastEncodedTimestamp();
  return 0;
}

WebRtc_Word32 AudioCodingModuleImpl::ReplaceInternalDTXWithWebRtc(
    bool use_webrtc_dtx) {
  CriticalSectionScoped lock(_acmCritSect);

  if (!HaveValidEncoder("ReplaceInternalDTXWithWebRtc")) {
    WEBRTC_TRACE(
        webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
        "Cannot replace codec internal DTX when no send codec is registered.");
    return -1;
  }

  WebRtc_Word32 res = _codecs[_currentSendCodecIdx]->ReplaceInternalDTX(
      use_webrtc_dtx);
  // Check if VAD is turned on, or if there is any error.
  if (res == 1) {
    _vadEnabled = true;
  } else if (res < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "Failed to set ReplaceInternalDTXWithWebRtc(%d)",
                 use_webrtc_dtx);
    return res;
  }

  return 0;
}

WebRtc_Word32 AudioCodingModuleImpl::IsInternalDTXReplacedWithWebRtc(
    bool& uses_webrtc_dtx) {
  CriticalSectionScoped lock(_acmCritSect);

  if (!HaveValidEncoder("IsInternalDTXReplacedWithWebRtc")) {
    return -1;
  }
  if (_codecs[_currentSendCodecIdx]->IsInternalDTXReplaced(&uses_webrtc_dtx)
      < 0) {
    return -1;
  }
  return 0;
}

WebRtc_Word32 AudioCodingModuleImpl::SetISACMaxRate(
    const WebRtc_UWord32 max_bit_per_sec) {
  CriticalSectionScoped lock(_acmCritSect);

  if (!HaveValidEncoder("SetISACMaxRate")) {
    return -1;
  }

  return _codecs[_currentSendCodecIdx]->SetISACMaxRate(max_bit_per_sec);
}

WebRtc_Word32 AudioCodingModuleImpl::SetISACMaxPayloadSize(
    const WebRtc_UWord16 max_size_bytes) {
  CriticalSectionScoped lock(_acmCritSect);

  if (!HaveValidEncoder("SetISACMaxPayloadSize")) {
    return -1;
  }

  return _codecs[_currentSendCodecIdx]->SetISACMaxPayloadSize(
      max_size_bytes);
}

WebRtc_Word32 AudioCodingModuleImpl::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8 frame_size_ms,
    const WebRtc_UWord16 rate_bit_per_sec,
    const bool enforce_frame_size) {
  CriticalSectionScoped lock(_acmCritSect);

  if (!HaveValidEncoder("ConfigISACBandwidthEstimator")) {
    return -1;
  }

  return _codecs[_currentSendCodecIdx]->ConfigISACBandwidthEstimator(
      frame_size_ms, rate_bit_per_sec, enforce_frame_size);
}

WebRtc_Word32 AudioCodingModuleImpl::SetBackgroundNoiseMode(
    const ACMBackgroundNoiseMode mode) {
  if ((mode < On) || (mode > Off)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "The specified background noise is out of range.\n");
    return -1;
  }
  return _netEq.SetBackgroundNoiseMode(mode);
}

WebRtc_Word32 AudioCodingModuleImpl::BackgroundNoiseMode(
    ACMBackgroundNoiseMode& mode) {
  return _netEq.BackgroundNoiseMode(mode);
}

WebRtc_Word32 AudioCodingModuleImpl::PlayoutTimestamp(
    WebRtc_UWord32& timestamp) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
               "PlayoutTimestamp()");
  return _netEq.PlayoutTimestamp(timestamp);
}

bool AudioCodingModuleImpl::HaveValidEncoder(const char* caller_name) const {
  if ((!_sendCodecRegistered) || (_currentSendCodecIdx < 0) ||
      (_currentSendCodecIdx >= ACMCodecDB::kNumCodecs)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "%s failed: No send codec is registered.", caller_name);
    return false;
  }
  if ((_currentSendCodecIdx < 0) ||
      (_currentSendCodecIdx >= ACMCodecDB::kNumCodecs)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "%s failed: Send codec index out of range.", caller_name);
    return false;
  }
  if (_codecs[_currentSendCodecIdx] == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                 "%s failed: Send codec is NULL pointer.", caller_name);
    return false;
  }
  return true;
}

WebRtc_Word32 AudioCodingModuleImpl::UnregisterReceiveCodec(
    const WebRtc_Word16 payload_type) {
  CriticalSectionScoped lock(_acmCritSect);
  int id;

  // Search through the list of registered payload types.
  for (id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if (_registeredPlTypes[id] == payload_type) {
      // We have found the id registered with the payload type.
      break;
    }
  }

  if (id >= ACMCodecDB::kNumCodecs) {
    // Payload type was not registered. No need to unregister.
    return 0;
  }

  // Unregister the codec with the given payload type.
  return UnregisterReceiveCodecSafe(id);
}

WebRtc_Word32 AudioCodingModuleImpl::UnregisterReceiveCodecSafe(
    const WebRtc_Word16 codec_id) {
  const WebRtcNetEQDecoder *neteq_decoder = ACMCodecDB::NetEQDecoders();
  WebRtc_Word16 mirror_id = ACMCodecDB::MirrorID(codec_id);
  bool stereo_receiver = false;

  if (_codecs[codec_id] != NULL) {
    if (_registeredPlTypes[codec_id] != -1) {
      // Store stereo information for future use.
      stereo_receiver = _stereoReceive[codec_id];

      // Before deleting the decoder instance unregister from NetEQ.
      if (_netEq.RemoveCodec(neteq_decoder[codec_id],
                             _stereoReceive[codec_id])  < 0) {
        CodecInst codec;
        ACMCodecDB::Codec(codec_id, &codec);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "Unregistering %s-%d from NetEQ failed.", codec.plname,
                     codec.plfreq);
        return -1;
      }

      // CN is a special case for NetEQ, all three sampling frequencies
      // are unregistered if one is deleted.
      if (IsCodecCN(codec_id)) {
        for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
          if (IsCodecCN(i)) {
            _stereoReceive[i] = false;
            _registeredPlTypes[i] = -1;
          }
        }
      } else {
        if (codec_id == mirror_id) {
          _codecs[codec_id]->DestructDecoder();
          if (_stereoReceive[codec_id]) {
            _slaveCodecs[codec_id]->DestructDecoder();
            _stereoReceive[codec_id] = false;

          }
        }
      }

      // Check if this is the last registered stereo receive codec.
      if (stereo_receiver) {
        bool no_stereo = true;

        for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
          if (_stereoReceive[i]) {
            // We still have stereo codecs registered.
            no_stereo = false;
            break;
          }
        }

        // If we don't have any stereo codecs left, change status.
        if (no_stereo) {
          _netEq.RemoveSlaves();  // No longer need the slave.
          _stereoReceiveRegistered = false;
        }
      }
    }
  }

  if (_registeredPlTypes[codec_id] == _receiveREDPayloadType) {
    // RED is going to be unregistered, set to an invalid value.
    _receiveREDPayloadType = 255;
  }
  _registeredPlTypes[codec_id] = -1;

  return 0;
}

WebRtc_Word32 AudioCodingModuleImpl::REDPayloadISAC(
    const WebRtc_Word32 isac_rate, const WebRtc_Word16 isac_bw_estimate,
    WebRtc_UWord8* payload, WebRtc_Word16* length_bytes) {
  if (!HaveValidEncoder("EncodeData")) {
    return -1;
  }
  WebRtc_Word16 status;
  status = _codecs[_currentSendCodecIdx]->REDPayloadISAC(isac_rate,
                                                         isac_bw_estimate,
                                                         payload,
                                                         length_bytes);
  return status;
}

}  // namespace webrtc
