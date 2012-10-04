/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_NETEQ_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_NETEQ_H_

#include "audio_coding_module.h"
#include "audio_coding_module_typedefs.h"
#include "engine_configurations.h"
#include "module_common_types.h"
#include "typedefs.h"
#include "webrtc_neteq.h"
#include "webrtc_vad.h"

namespace webrtc {

class CriticalSectionWrapper;
class RWLockWrapper;
struct CodecInst;
enum AudioPlayoutMode;
enum ACMSpeechType;

#define MAX_NUM_SLAVE_NETEQ 1

class ACMNetEQ
{
public:
    // Constructor of the class
    ACMNetEQ();

    // Destructor of the class.
    ~ACMNetEQ();

    //
    // Init()
    // Allocates memory for NetEQ and VAD and initializes them.
    //
    // Return value              : 0 if ok.
    //                            -1 if NetEQ or VAD returned an error or
    //                            if out of memory.
    //
    WebRtc_Word32 Init();

    //
    // RecIn()
    // Gives the payload to NetEQ.
    //
    // Input:
    //   - incomingPayload       : Incoming audio payload.
    //   - payloadLength         : Length of incoming audio payload.
    //   - rtpInfo               : RTP header for the incoming payload containing
    //                             information about payload type, sequence number,
    //                             timestamp, ssrc and marker bit.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 RecIn(
        const WebRtc_UWord8*    incomingPayload,
        const WebRtc_Word32    payloadLength,
        const WebRtcRTPHeader&   rtpInfo);

    //
    // RecOut()
    // Asks NetEQ for 10 ms of decoded audio.
    //
    // Input:
    //   -audioFrame             : an audio frame were output data and
    //                             associated parameters are written to.
    //
    // Return value              : 0 if ok.
    //                            -1 if NetEQ returned an error.
    //
    WebRtc_Word32 RecOut(
        AudioFrame& audioFrame);

    //
    // AddCodec()
    // Adds a new codec to the NetEQ codec database.
    //
    // Input:
    //   - codecDef              : The codec to be added.
    //   - toMaster              : true if the codec has to be added to Master
    //                             NetEq, otherwise will be added to the Slave
    //                             NetEQ.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 AddCodec(
        WebRtcNetEQ_CodecDef *codecDef,
        bool                  toMaster = true);

    //
    // AllocatePacketBuffer()
    // Allocates the NetEQ packet buffer.
    //
    // Input:
    //   - usedCodecs            : An array of the codecs to be used by NetEQ.
    //   - noOfCodecs            : Number of codecs in usedCodecs.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 AllocatePacketBuffer(
        const WebRtcNetEQDecoder* usedCodecs,
        WebRtc_Word16    noOfCodecs);

    //
    // SetExtraDelay()
    // Sets an delayInMS milliseconds extra delay in NetEQ.
    //
    // Input:
    //   - delayInMS             : Extra delay in milliseconds.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 SetExtraDelay(
        const WebRtc_Word32 delayInMS);

    //
    // SetAVTPlayout()
    // Enable/disable playout of AVT payloads.
    //
    // Input:
    //   - enable                : Enable if true, disable if false.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 SetAVTPlayout(
        const bool enable);

    //
    // AVTPlayout()
    // Get the current AVT playout state.
    //
    // Return value              : True if AVT playout is enabled.
    //                             False if AVT playout is disabled.
    //
    bool AVTPlayout() const;

    //
    // CurrentSampFreqHz()
    // Get the current sampling frequency in Hz.
    //
    // Return value              : Sampling frequency in Hz.
    //
    WebRtc_Word32 CurrentSampFreqHz() const;

    //
    // SetPlayoutMode()
    // Sets the playout mode to voice or fax.
    //
    // Input:
    //   - mode                  : The playout mode to be used, voice,
    //                             fax, or streaming.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 SetPlayoutMode(
        const AudioPlayoutMode mode);

    //
    // PlayoutMode()
    // Get the current playout mode.
    //
    // Return value              : The current playout mode.
    //
    AudioPlayoutMode PlayoutMode() const;

    //
    // NetworkStatistics()
    // Get the current network statistics from NetEQ.
    //
    // Output:
    //   - statistics            : The current network statistics.
    //
    // Return value              : 0 if ok.
    //                            <0 if NetEQ returned an error.
    //
    WebRtc_Word32 NetworkStatistics(
        ACMNetworkStatistics* statistics) const;

    //
    // VADMode()
    // Get the current VAD Mode.
    //
    // Return value              : The current VAD mode.
    //
    ACMVADMode VADMode() const;

    //
    // SetVADMode()
    // Set the VAD mode.
    //
    // Input:
    //   - mode                  : The new VAD mode.
    //
    // Return value              : 0 if ok.
    //                            -1 if an error occurred.
    //
    WebRtc_Word16 SetVADMode(
        const ACMVADMode mode);

    //
    // DecodeLock()
    // Get the decode lock used to protect decoder instances while decoding.
    //
    // Return value              : Pointer to the decode lock.
    //
    RWLockWrapper* DecodeLock() const
    {
        return _decodeLock;
    }

    //
    // FlushBuffers()
    // Flushes the NetEQ packet and speech buffers.
    //
    // Return value              : 0 if ok.
    //                            -1 if NetEQ returned an error.
    //
    WebRtc_Word32 FlushBuffers();

    //
    // RemoveCodec()
    // Removes a codec from the NetEQ codec database.
    //
    // Input:
    //   - codecIdx              : Codec to be removed.
    //
    // Return value              : 0 if ok.
    //                            -1 if an error occurred.
    //
    WebRtc_Word16 RemoveCodec(
        WebRtcNetEQDecoder codecIdx,
        bool isStereo = false);


    //
    // SetBackgroundNoiseMode()
    // Set the mode of the background noise.
    //
    // Input:
    //   - mode                  : an enumerator specifying the mode of the
    //                             background noise.
    //
    // Return value              : 0 if succeeded,
    //                            -1 if failed to set the mode.
    //
    WebRtc_Word16 SetBackgroundNoiseMode(
        const ACMBackgroundNoiseMode mode);

    //
    // BackgroundNoiseMode()
    // return the mode of the background noise.
    //
    // Return value              : The mode of background noise.
    //
    WebRtc_Word16 BackgroundNoiseMode(
        ACMBackgroundNoiseMode& mode);

    void SetUniqueId(
        WebRtc_Word32 id);

    WebRtc_Word32 PlayoutTimestamp(
        WebRtc_UWord32& timestamp);

    void SetReceivedStereo(
        bool receivedStereo);

    WebRtc_UWord8 NumSlaves();

    enum JB {masterJB = 0, slaveJB = 1};

    // Delete all slaves.
    void RemoveSlaves();

    WebRtc_Word16 AddSlave(
        const WebRtcNetEQDecoder*    usedCodecs,
        WebRtc_Word16       noOfCodecs);

private:
    //
    // RTPPack()
    // Creates a Word16 RTP packet out of the payload data in Word16 and
    // a WebRtcRTPHeader.
    //
    // Input:
    //   - payload               : Payload to be packetized.
    //   - payloadLengthW8       : Length of the payload in bytes.
    //   - rtpInfo               : RTP header struct.
    //
    // Output:
    //   - rtpPacket             : The RTP packet.
    //
    static void RTPPack(
        WebRtc_Word16*         rtpPacket,
        const WebRtc_Word8*    payload,
        const WebRtc_Word32    payloadLengthW8,
        const WebRtcRTPHeader& rtpInfo);

    void LogError(
        const char* neteqFuncName,
        const WebRtc_Word16 idx) const;

    WebRtc_Word16 InitByIdxSafe(
        const WebRtc_Word16 idx);

    // EnableVAD()
    // Enable VAD.
    //
    // Return value              : 0 if ok.
    //                            -1 if an error occurred.
    //
    WebRtc_Word16 EnableVAD();

    WebRtc_Word16 EnableVADByIdxSafe(
        const WebRtc_Word16 idx);

    WebRtc_Word16 AllocatePacketBufferByIdxSafe(
        const WebRtcNetEQDecoder* usedCodecs,
        WebRtc_Word16       noOfCodecs,
        const WebRtc_Word16 idx);

    // Delete the NetEQ corresponding to |index|.
    void RemoveNetEQSafe(int index);

    void RemoveSlavesSafe();

    void*                   _inst[MAX_NUM_SLAVE_NETEQ + 1];
    void*                   _instMem[MAX_NUM_SLAVE_NETEQ + 1];

    WebRtc_Word16*          _netEqPacketBuffer[MAX_NUM_SLAVE_NETEQ + 1];

    WebRtc_Word32           _id;
    float                   _currentSampFreqKHz;
    bool                    _avtPlayout;
    AudioPlayoutMode        _playoutMode;
    CriticalSectionWrapper* _netEqCritSect;

    WebRtcVadInst*          _ptrVADInst[MAX_NUM_SLAVE_NETEQ + 1];

    bool                    _vadStatus;
    ACMVADMode              _vadMode;
    RWLockWrapper*          _decodeLock;
    bool                    _isInitialized[MAX_NUM_SLAVE_NETEQ + 1];
    WebRtc_UWord8           _numSlaves;
    bool                    _receivedStereo;
    void*                   _masterSlaveInfo;
    AudioFrame::VADActivity _previousAudioActivity;
    WebRtc_Word32           _extraDelay;

    CriticalSectionWrapper* _callbackCritSect;
};

} //namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_NETEQ_H_
