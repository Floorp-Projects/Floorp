/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/normal_test.h"

#include <assert.h>
#include <iostream>
#include <sstream>
#include <time.h>

#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_callbacks.h"
#include "webrtc/modules/video_coding/main/test/test_macros.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"

using namespace webrtc;

int NormalTest::RunTest(const CmdArgs& args)
{
    SimulatedClock sim_clock(0);
    SimulatedClock* clock = &sim_clock;
    NullEventFactory event_factory;
    Trace::CreateTrace();
    Trace::SetTraceFile(
        (test::OutputPath() + "VCMNormalTestTrace.txt").c_str());
    Trace::SetLevelFilter(webrtc::kTraceAll);
    VideoCodingModule* vcm = VideoCodingModule::Create(1, clock,
                                                       &event_factory);
    NormalTest VCMNTest(vcm, clock);
    VCMNTest.Perform(args);
    VideoCodingModule::Destroy(vcm);
    Trace::ReturnTrace();
    return 0;
}

////////////////
// Callback Implementation
//////////////

VCMNTEncodeCompleteCallback::VCMNTEncodeCompleteCallback(FILE* encodedFile,
                                                         NormalTest& test):
    _encodedFile(encodedFile),
    _encodedBytes(0),
    _skipCnt(0),
    _VCMReceiver(NULL),
    _seqNo(0),
    _test(test)
{
    //
}
VCMNTEncodeCompleteCallback::~VCMNTEncodeCompleteCallback()
{
}

void VCMNTEncodeCompleteCallback::RegisterTransportCallback(
    VCMPacketizationCallback* transport)
{
}

int32_t
VCMNTEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const uint8_t  payloadType,
        const uint32_t timeStamp,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        const uint32_t payloadSize,
        const RTPFragmentationHeader& /*fragmentationHeader*/,
        const webrtc::RTPVideoHeader* videoHdr)

{
  // will call the VCMReceiver input packet
  _frameType = frameType;
  // writing encodedData into file
  if (fwrite(payloadData, 1, payloadSize, _encodedFile) !=  payloadSize) {
    return -1;
  }
  WebRtcRTPHeader rtpInfo;
  rtpInfo.header.markerBit = true;
  rtpInfo.type.Video.width = 0;
  rtpInfo.type.Video.height = 0;
  switch (_test.VideoType())
  {
  case kVideoCodecVP8:
    rtpInfo.type.Video.codec = kRTPVideoVP8;
    rtpInfo.type.Video.codecHeader.VP8.InitRTPVideoHeaderVP8();
    rtpInfo.type.Video.codecHeader.VP8.nonReference =
        videoHdr->codecHeader.VP8.nonReference;
    rtpInfo.type.Video.codecHeader.VP8.pictureId =
        videoHdr->codecHeader.VP8.pictureId;
    break;
  case kVideoCodecI420:
    rtpInfo.type.Video.codec = kRTPVideoI420;
    break;
  default:
    assert(false);
    return -1;
  }
  rtpInfo.header.payloadType = payloadType;
  rtpInfo.header.sequenceNumber = _seqNo++;
  rtpInfo.header.ssrc = 0;
  rtpInfo.header.timestamp = timeStamp;
  rtpInfo.frameType = frameType;
  rtpInfo.type.Video.isFirstPacket = true;
  // Size should also be received from that table, since the payload type
  // defines the size.

  _encodedBytes += payloadSize;
  if (payloadSize < 20)
  {
      _skipCnt++;
  }
  _VCMReceiver->IncomingPacket(payloadData, payloadSize, rtpInfo);
  return 0;
}
void
VCMNTEncodeCompleteCallback::RegisterReceiverVCM(VideoCodingModule *vcm)
{
  _VCMReceiver = vcm;
  return;
}
 int32_t
VCMNTEncodeCompleteCallback::EncodedBytes()
{
  return _encodedBytes;
}

uint32_t
VCMNTEncodeCompleteCallback::SkipCnt()
{
  return _skipCnt;
}

// Decoded Frame Callback Implementation
VCMNTDecodeCompleCallback::~VCMNTDecodeCompleCallback()
{
  if (_decodedFile)
  fclose(_decodedFile);
}
 int32_t
VCMNTDecodeCompleCallback::FrameToRender(webrtc::I420VideoFrame& videoFrame)
{
    if (videoFrame.width() != _currentWidth ||
        videoFrame.height() != _currentHeight)
    {
        _currentWidth = videoFrame.width();
        _currentHeight = videoFrame.height();
        if (_decodedFile != NULL)
        {
            fclose(_decodedFile);
            _decodedFile = NULL;
        }
        _decodedFile = fopen(_outname.c_str(), "wb");
    }
    if (PrintI420VideoFrame(videoFrame, _decodedFile) < 0) {
      return -1;
    }
    _decodedBytes+= webrtc::CalcBufferSize(webrtc::kI420,
                                   videoFrame.width(), videoFrame.height());
    return VCM_OK;
}

 int32_t
VCMNTDecodeCompleCallback::DecodedBytes()
{
  return _decodedBytes;
}

 //VCM Normal Test Class implementation

NormalTest::NormalTest(VideoCodingModule* vcm, Clock* clock)
:
_clock(clock),
_vcm(vcm),
_sumEncBytes(0),
_timeStamp(0),
_totalEncodeTime(0),
_totalDecodeTime(0),
_decodeCompleteTime(0),
_encodeCompleteTime(0),
_totalEncodePipeTime(0),
_totalDecodePipeTime(0),
_frameCnt(0),
_encFrameCnt(0),
_decFrameCnt(0)
{
    //
}

NormalTest::~NormalTest()
{
    //
}
void
NormalTest::Setup(const CmdArgs& args)
{
  _inname = args.inputFile;
  _encodedName = test::OutputPath() + "encoded_normaltest.yuv";
  _width = args.width;
  _height = args.height;
  _frameRate = args.frameRate;
  _bitRate = args.bitRate;
  if (args.outputFile == "")
  {
      std::ostringstream filename;
      filename << test::OutputPath() << "NormalTest_" <<
          _width << "x" << _height << "_" << _frameRate << "Hz_P420.yuv";
      _outname = filename.str();
  }
  else
  {
      _outname = args.outputFile;
  }
  _lengthSourceFrame  = 3*_width*_height/2;
  _videoType = args.codecType;

  if ((_sourceFile = fopen(_inname.c_str(), "rb")) == NULL)
  {
      printf("Cannot read file %s.\n", _inname.c_str());
      exit(1);
  }
  if ((_encodedFile = fopen(_encodedName.c_str(), "wb")) == NULL)
  {
      printf("Cannot write encoded file.\n");
      exit(1);
  }

  _log.open((test::OutputPath() + "TestLog.txt").c_str(),
            std::fstream::out | std::fstream::app);
}

int32_t
NormalTest::Perform(const CmdArgs& args)
{
  Setup(args);
  EventWrapper* waitEvent = EventWrapper::Create();
  VideoCodec _sendCodec;
  _vcm->InitializeReceiver();
  _vcm->InitializeSender();
  TEST(VideoCodingModule::Codec(_videoType, &_sendCodec) == VCM_OK);
  // should be later on changed via the API
  _sendCodec.startBitrate = (int)_bitRate;
  _sendCodec.width = static_cast<uint16_t>(_width);
  _sendCodec.height = static_cast<uint16_t>(_height);
  _sendCodec.maxFramerate = _frameRate;
  // will also set and init the desired codec
  TEST(_vcm->RegisterSendCodec(&_sendCodec, 4, 1400) == VCM_OK);
  // register a decoder (same codec for decoder and encoder )
  TEST(_vcm->RegisterReceiveCodec(&_sendCodec, 1) == VCM_OK);
  /* Callback Settings */
  VCMNTDecodeCompleCallback _decodeCallback(_outname);
  _vcm->RegisterReceiveCallback(&_decodeCallback);
  VCMNTEncodeCompleteCallback _encodeCompleteCallback(_encodedFile, *this);
  _vcm->RegisterTransportCallback(&_encodeCompleteCallback);
  // encode and decode with the same vcm
  _encodeCompleteCallback.RegisterReceiverVCM(_vcm);
  ///////////////////////
  /// Start Test
  ///////////////////////
  I420VideoFrame sourceFrame;
  int size_y = _width * _height;
  int half_width = (_width + 1) / 2;
  int half_height = (_height + 1) / 2;
  int size_uv = half_width * half_height;
  sourceFrame.CreateEmptyFrame(_width, _height,
                               _width, half_width, half_width);
  uint8_t* tmpBuffer = new uint8_t[_lengthSourceFrame];
  double startTime = clock()/(double)CLOCKS_PER_SEC;
  _vcm->SetChannelParameters(static_cast<uint32_t>(1000 * _bitRate), 0, 0);

  SendStatsTest sendStats;
  sendStats.set_framerate(static_cast<uint32_t>(_frameRate));
  sendStats.set_bitrate(1000 * _bitRate);
  _vcm->RegisterSendStatisticsCallback(&sendStats);

  while (feof(_sourceFile) == 0) {
    TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0 ||
         feof(_sourceFile));
    _frameCnt++;
    sourceFrame.CreateFrame(size_y, tmpBuffer,
                            size_uv, tmpBuffer + size_y,
                            size_uv, tmpBuffer + size_y + size_uv,
                            _width, _height,
                            _width, half_width, half_width);
    _timeStamp +=
        (uint32_t)(9e4 / static_cast<float>(_sendCodec.maxFramerate));
    sourceFrame.set_timestamp(_timeStamp);
    _encodeTimes[int(sourceFrame.timestamp())] =
        clock()/(double)CLOCKS_PER_SEC;
    int32_t ret = _vcm->AddVideoFrame(sourceFrame);
    double encodeTime = clock()/(double)CLOCKS_PER_SEC -
                        _encodeTimes[int(sourceFrame.timestamp())];
    _totalEncodeTime += encodeTime;
    if (ret < 0)
    {
        printf("Error in AddFrame: %d\n", ret);
        //exit(1);
    }
    _decodeTimes[int(sourceFrame.timestamp())] =
        clock()/(double)CLOCKS_PER_SEC;
    ret = _vcm->Decode();
    _totalDecodeTime += clock()/(double)CLOCKS_PER_SEC -
                        _decodeTimes[int(sourceFrame.timestamp())];
    if (ret < 0)
    {
        printf("Error in Decode: %d\n", ret);
        //exit(1);
    }
    if (_vcm->TimeUntilNextProcess() <= 0)
    {
        _vcm->Process();
    }
    uint32_t framePeriod =
        static_cast<uint32_t>(
            1000.0f / static_cast<float>(_sendCodec.maxFramerate) + 0.5f);
    static_cast<SimulatedClock*>(_clock)->AdvanceTimeMilliseconds(framePeriod);
  }
  double endTime = clock()/(double)CLOCKS_PER_SEC;
  _testTotalTime = endTime - startTime;
  _sumEncBytes = _encodeCompleteCallback.EncodedBytes();

  delete [] tmpBuffer;
  delete waitEvent;
  Teardown();
  Print();
  return 0;
}

void
NormalTest::FrameEncoded(uint32_t timeStamp)
{
  _encodeCompleteTime = clock()/(double)CLOCKS_PER_SEC;
  _encFrameCnt++;
  _totalEncodePipeTime += _encodeCompleteTime - _encodeTimes[int(timeStamp)];

}

void
NormalTest::FrameDecoded(uint32_t timeStamp)
{
  _decodeCompleteTime = clock()/(double)CLOCKS_PER_SEC;
  _decFrameCnt++;
  _totalDecodePipeTime += _decodeCompleteTime - _decodeTimes[timeStamp];
}

void
NormalTest::Print()
{
  std::cout << "Normal Test Completed!" << std::endl;
  (_log) << "Normal Test Completed!" << std::endl;
  (_log) << "Input file: " << _inname << std::endl;
  (_log) << "Output file: " << _outname << std::endl;
  (_log) << "Total run time: " << _testTotalTime << std::endl;
  printf("Total run time: %f s \n", _testTotalTime);
  double ActualBitRate =  8.0 *( _sumEncBytes / (_frameCnt / _frameRate));
  double actualBitRate = ActualBitRate / 1000.0;
  double avgEncTime = _totalEncodeTime / _frameCnt;
  double avgDecTime = _totalDecodeTime / _frameCnt;
  webrtc::test::QualityMetricsResult psnr, ssim;
  I420PSNRFromFiles(_inname.c_str(), _outname.c_str(), _width, _height,
                    &psnr);
  I420SSIMFromFiles(_inname.c_str(), _outname.c_str(), _width, _height,
                    &ssim);
  printf("Actual bitrate: %f kbps\n", actualBitRate);
  printf("Target bitrate: %f kbps\n", _bitRate);
  ( _log) << "Actual bitrate: " << actualBitRate <<
      " kbps\tTarget: " << _bitRate << " kbps" << std::endl;
  printf("Average encode time: %f s\n", avgEncTime);
  ( _log) << "Average encode time: " << avgEncTime << " s" << std::endl;
  printf("Average decode time: %f s\n", avgDecTime);
  ( _log) << "Average decode time: " << avgDecTime << " s" << std::endl;
  printf("PSNR: %f \n", psnr.average);
  ( _log) << "PSNR: " << psnr.average << std::endl;
  printf("SSIM: %f \n", ssim.average);
  ( _log) << "SSIM: " << ssim.average << std::endl;
  (_log) << std::endl;

  printf("\nVCM Normal Test: \n\n%i tests completed\n", vcmMacrosTests);
  if (vcmMacrosErrors > 0)
  {
      printf("%i FAILED\n\n", vcmMacrosErrors);
  }
  else
  {
      printf("ALL PASSED\n\n");
  }
}
void
NormalTest::Teardown()
{
  //_log.close();
  fclose(_sourceFile);
  fclose(_encodedFile);
  return;
}
