/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_NORMAL_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_NORMAL_TEST_H_

#include "video_coding.h"
#include "test_util.h"

#include <fstream>
#include <map>

class NormalTest;

//Send Side - Packetization callback -
// will create and send a packet to the VCMReceiver
class VCMNTEncodeCompleteCallback : public webrtc::VCMPacketizationCallback
{
 public:
  // constructor input: file in which encoded data will be written
  VCMNTEncodeCompleteCallback(FILE* encodedFile, NormalTest& test);
  virtual ~VCMNTEncodeCompleteCallback();
  // Register transport callback
  void RegisterTransportCallback(webrtc::VCMPacketizationCallback* transport);
  // process encoded data received from the encoder,
  // pass stream to the VCMReceiver module
  int32_t
  SendData(const webrtc::FrameType frameType,
           const uint8_t payloadType,
           const uint32_t timeStamp,
           int64_t capture_time_ms,
           const uint8_t* payloadData,
           const uint32_t payloadSize,
           const webrtc::RTPFragmentationHeader& fragmentationHeader,
           const webrtc::RTPVideoHeader* videoHdr);

  // Register exisitng VCM.
  // Currently - encode and decode with the same vcm module.
  void RegisterReceiverVCM(webrtc::VideoCodingModule *vcm);
  // Return sum of encoded data (all frames in the sequence)
  int32_t EncodedBytes();
  // return number of encoder-skipped frames
  uint32_t SkipCnt();;
  // conversion function for payload type (needed for the callback function)
//    RTPVideoVideoCodecTypes ConvertPayloadType(uint8_t payloadType);

 private:
  FILE*                       _encodedFile;
  uint32_t              _encodedBytes;
  uint32_t              _skipCnt;
  webrtc::VideoCodingModule*  _VCMReceiver;
  webrtc::FrameType           _frameType;
  uint16_t              _seqNo;
  NormalTest&                 _test;
}; // end of VCMEncodeCompleteCallback

class VCMNTDecodeCompleCallback: public webrtc::VCMReceiveCallback
{
public:
    VCMNTDecodeCompleCallback(std::string outname): // or should it get a name?
        _decodedFile(NULL),
        _outname(outname),
        _decodedBytes(0),
        _currentWidth(0),
        _currentHeight(0) {}
    virtual ~VCMNTDecodeCompleCallback();
    void SetUserReceiveCallback(webrtc::VCMReceiveCallback* receiveCallback);
    // will write decoded frame into file
    int32_t FrameToRender(webrtc::I420VideoFrame& videoFrame);
    int32_t DecodedBytes();
private:
    FILE*             _decodedFile;
    std::string       _outname;
    int               _decodedBytes;
    int               _currentWidth;
    int               _currentHeight;
}; // end of VCMDecodeCompleCallback class

class NormalTest
{
public:
    NormalTest(webrtc::VideoCodingModule* vcm,
               webrtc::Clock* clock);
    ~NormalTest();
    static int RunTest(const CmdArgs& args);
    int32_t    Perform(const CmdArgs& args);
    // option:: turn into private and call from perform
    int   Width() const { return _width; };
    int   Height() const { return _height; };
    webrtc::VideoCodecType VideoType() const { return _videoType; };


protected:
    // test setup - open files, general initializations
    void            Setup(const CmdArgs& args);
   // close open files, delete used memory
    void            Teardown();
    // print results to std output and to log file
    void            Print();
    // calculating pipeline delay, and encoding time
    void            FrameEncoded(uint32_t timeStamp);
    // calculating pipeline delay, and decoding time
    void            FrameDecoded(uint32_t timeStamp);

    webrtc::Clock*                   _clock;
    webrtc::VideoCodingModule*       _vcm;
    webrtc::VideoCodec               _sendCodec;
    webrtc::VideoCodec               _receiveCodec;
    std::string                      _inname;
    std::string                      _outname;
    std::string                      _encodedName;
    int32_t                    _sumEncBytes;
    FILE*                            _sourceFile;
    FILE*                            _decodedFile;
    FILE*                            _encodedFile;
    std::fstream                     _log;
    int                              _width;
    int                              _height;
    float                            _frameRate;
    float                            _bitRate;
    uint32_t                   _lengthSourceFrame;
    uint32_t                   _timeStamp;
    webrtc::VideoCodecType           _videoType;
    double                           _totalEncodeTime;
    double                           _totalDecodeTime;
    double                           _decodeCompleteTime;
    double                           _encodeCompleteTime;
    double                           _totalEncodePipeTime;
    double                           _totalDecodePipeTime;
    double                           _testTotalTime;
    std::map<int, double>            _encodeTimes;
    std::map<int, double>            _decodeTimes;
    int32_t                    _frameCnt;
    int32_t                    _encFrameCnt;
    int32_t                    _decFrameCnt;

}; // end of VCMNormalTestClass

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_NORMAL_TEST_H_
