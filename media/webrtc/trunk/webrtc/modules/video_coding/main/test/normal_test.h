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
  WebRtc_Word32
  SendData(const webrtc::FrameType frameType,
           const WebRtc_UWord8 payloadType,
           const WebRtc_UWord32 timeStamp,
           int64_t capture_time_ms,
           const WebRtc_UWord8* payloadData,
           const WebRtc_UWord32 payloadSize,
           const webrtc::RTPFragmentationHeader& fragmentationHeader,
           const webrtc::RTPVideoHeader* videoHdr);

  // Register exisitng VCM.
  // Currently - encode and decode with the same vcm module.
  void RegisterReceiverVCM(webrtc::VideoCodingModule *vcm);
  // Return sum of encoded data (all frames in the sequence)
  WebRtc_Word32 EncodedBytes();
  // return number of encoder-skipped frames
  WebRtc_UWord32 SkipCnt();;
  // conversion function for payload type (needed for the callback function)
//    RTPVideoVideoCodecTypes ConvertPayloadType(WebRtc_UWord8 payloadType);

 private:
  FILE*                       _encodedFile;
  WebRtc_UWord32              _encodedBytes;
  WebRtc_UWord32              _skipCnt;
  webrtc::VideoCodingModule*  _VCMReceiver;
  webrtc::FrameType           _frameType;
  WebRtc_UWord16              _seqNo;
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
    WebRtc_Word32 FrameToRender(webrtc::I420VideoFrame& videoFrame);
    WebRtc_Word32 DecodedBytes();
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
               webrtc::TickTimeBase* clock);
    ~NormalTest();
    static int RunTest(const CmdArgs& args);
    WebRtc_Word32    Perform(const CmdArgs& args);
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
    void            FrameEncoded(WebRtc_UWord32 timeStamp);
    // calculating pipeline delay, and decoding time
    void            FrameDecoded(WebRtc_UWord32 timeStamp);

    webrtc::TickTimeBase*            _clock;
    webrtc::VideoCodingModule*       _vcm;
    webrtc::VideoCodec               _sendCodec;
    webrtc::VideoCodec               _receiveCodec;
    std::string                      _inname;
    std::string                      _outname;
    std::string                      _encodedName;
    WebRtc_Word32                    _sumEncBytes;
    FILE*                            _sourceFile;
    FILE*                            _decodedFile;
    FILE*                            _encodedFile;
    std::fstream                     _log;
    int                              _width;
    int                              _height;
    float                            _frameRate;
    float                            _bitRate;
    WebRtc_UWord32                   _lengthSourceFrame;
    WebRtc_UWord32                   _timeStamp;
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
    WebRtc_Word32                    _frameCnt;
    WebRtc_Word32                    _encFrameCnt;
    WebRtc_Word32                    _decFrameCnt;

}; // end of VCMNormalTestClass

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_NORMAL_TEST_H_
