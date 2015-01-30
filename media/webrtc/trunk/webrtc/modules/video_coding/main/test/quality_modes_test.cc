/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/quality_modes_test.h"

#include <iostream>
#include <sstream>
#include <string>
#include <time.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_callbacks.h"
#include "webrtc/modules/video_coding/main/test/test_macros.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/data_log.h"
#include "webrtc/system_wrappers/interface/data_log.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"

using namespace webrtc;

int qualityModeTest(const CmdArgs& args)
{
  SimulatedClock clock(0);
  NullEventFactory event_factory;
  VideoCodingModule* vcm = VideoCodingModule::Create(&clock, &event_factory);
  QualityModesTest QMTest(vcm, &clock);
  QMTest.Perform(args);
  VideoCodingModule::Destroy(vcm);
  return 0;
}

QualityModesTest::QualityModesTest(VideoCodingModule* vcm,
                                   Clock* clock):
NormalTest(vcm, clock),
_vpm()
{
    //
}

QualityModesTest::~QualityModesTest()
{
    //
}

void
QualityModesTest::Setup(const CmdArgs& args)
{
  NormalTest::Setup(args);
  _inname = args.inputFile;
  _outname = args.outputFile;
  fv_outfilename_ = args.fv_outputfile;

  filename_testvideo_ =
      _inname.substr(_inname.find_last_of("\\/") + 1,_inname.length());

  _encodedName = test::OutputPath() + "encoded_qmtest.yuv";

  //NATIVE/SOURCE VALUES
  _nativeWidth = args.width;
  _nativeHeight = args.height;
  _nativeFrameRate =args.frameRate;

  //TARGET/ENCODER VALUES
  _width = args.width;
  _height = args.height;
  _frameRate = args.frameRate;

  _bitRate = args.bitRate;

  _flagSSIM = true;

  _lengthSourceFrame  = 3*_nativeWidth*_nativeHeight/2;

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
  if ((_decodedFile = fopen(_outname.c_str(),  "wb")) == NULL)
  {
    printf("Cannot write file %s.\n", _outname.c_str());
    exit(1);
  }

  DataLog::CreateLog();

  feature_table_name_ = fv_outfilename_;

  DataLog::AddTable(feature_table_name_);

  DataLog::AddColumn(feature_table_name_, "motion magnitude", 1);
  DataLog::AddColumn(feature_table_name_, "spatial prediction error", 1);
  DataLog::AddColumn(feature_table_name_, "spatial pred err horizontal", 1);
  DataLog::AddColumn(feature_table_name_, "spatial pred err vertical", 1);
  DataLog::AddColumn(feature_table_name_, "width", 1);
  DataLog::AddColumn(feature_table_name_, "height", 1);
  DataLog::AddColumn(feature_table_name_, "num pixels", 1);
  DataLog::AddColumn(feature_table_name_, "frame rate", 1);
  DataLog::AddColumn(feature_table_name_, "num frames since drop", 1);

  _log.open((test::OutputPath() + "TestLog.txt").c_str(),
            std::fstream::out | std::fstream::app);
}

void
QualityModesTest::Print()
{
  std::cout << "Quality Modes Test Completed!" << std::endl;
  (_log) << "Quality Modes Test Completed!" << std::endl;
  (_log) << "Input file: " << _inname << std::endl;
  (_log) << "Output file: " << _outname << std::endl;
  (_log) << "Total run time: " << _testTotalTime << std::endl;
  printf("Total run time: %f s \n", _testTotalTime);
  double ActualBitRate = 8.0*( _sumEncBytes / (_frameCnt / _nativeFrameRate));
  double actualBitRate = ActualBitRate / 1000.0;
  double avgEncTime = _totalEncodeTime / _frameCnt;
  double avgDecTime = _totalDecodeTime / _frameCnt;
  webrtc::test::QualityMetricsResult psnr,ssim;
  I420PSNRFromFiles(_inname.c_str(), _outname.c_str(), _nativeWidth,
                    _nativeHeight, &psnr);
  printf("Actual bitrate: %f kbps\n", actualBitRate);
  printf("Target bitrate: %f kbps\n", _bitRate);
  ( _log) << "Actual bitrate: " << actualBitRate<< " kbps\tTarget: " <<
      _bitRate << " kbps" << std::endl;
  printf("Average encode time: %f s\n", avgEncTime);
  ( _log) << "Average encode time: " << avgEncTime << " s" << std::endl;
  printf("Average decode time: %f s\n", avgDecTime);
  ( _log) << "Average decode time: " << avgDecTime << " s" << std::endl;
  printf("PSNR: %f \n", psnr.average);
  printf("**Number of frames dropped in VPM***%d \n",_numFramesDroppedVPM);
  ( _log) << "PSNR: " << psnr.average << std::endl;
  if (_flagSSIM == 1)
  {
    printf("***computing SSIM***\n");
    I420SSIMFromFiles(_inname.c_str(), _outname.c_str(), _nativeWidth,
                      _nativeHeight, &ssim);
    printf("SSIM: %f \n", ssim.average);
  }
  (_log) << std::endl;

  printf("\nVCM Quality Modes Test: \n\n%i tests completed\n", vcmMacrosTests);
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
QualityModesTest::Teardown()
{
  _log.close();
  fclose(_sourceFile);
  fclose(_decodedFile);
  fclose(_encodedFile);
  return;
}

int32_t
QualityModesTest::Perform(const CmdArgs& args)
{
  Setup(args);
  // changing bit/frame rate during the test
  const float bitRateUpdate[] = {1000};
  const float frameRateUpdate[] = {30};
  // frame num at which an update will occur
  const int updateFrameNum[] = {10000};

  uint32_t numChanges = sizeof(updateFrameNum)/sizeof(*updateFrameNum);
  uint8_t change = 0;// change counter

  _vpm = VideoProcessingModule::Create(1);
  EventWrapper* waitEvent = EventWrapper::Create();
  VideoCodec codec;//both send and receive
  _vcm->InitializeReceiver();
  _vcm->InitializeSender();
  int32_t NumberOfCodecs = _vcm->NumberOfCodecs();
  for (int i = 0; i < NumberOfCodecs; i++)
  {
    _vcm->Codec(i, &codec);
    if(strncmp(codec.plName,"VP8" , 5) == 0)
    {
      codec.startBitrate = (int)_bitRate;
      codec.maxFramerate = (uint8_t) _frameRate;
      codec.width = (uint16_t)_width;
      codec.height = (uint16_t)_height;
      codec.codecSpecific.VP8.frameDroppingOn = false;

      // Will also set and init the desired codec
      TEST(_vcm->RegisterSendCodec(&codec, 2, 1440) == VCM_OK);
      i = NumberOfCodecs;
    }
  }

  // register a decoder (same codec for decoder and encoder )
  TEST(_vcm->RegisterReceiveCodec(&codec, 2) == VCM_OK);
  /* Callback Settings */
  VCMQMDecodeCompleCallback  _decodeCallback(
      _decodedFile, _nativeFrameRate, feature_table_name_);
  _vcm->RegisterReceiveCallback(&_decodeCallback);
  VCMNTEncodeCompleteCallback   _encodeCompleteCallback(_encodedFile, *this);
  _vcm->RegisterTransportCallback(&_encodeCompleteCallback);
  // encode and decode with the same vcm
  _encodeCompleteCallback.RegisterReceiverVCM(_vcm);

  //quality modes callback
  QMTestVideoSettingsCallback QMCallback;
  QMCallback.RegisterVCM(_vcm);
  QMCallback.RegisterVPM(_vpm);
  //_vcm->RegisterVideoQMCallback(&QMCallback);

  ///////////////////////
  /// Start Test
  ///////////////////////
  _vpm->EnableTemporalDecimation(true);
  _vpm->EnableContentAnalysis(true);
  _vpm->SetInputFrameResampleMode(kFastRescaling);

  // disabling internal VCM frame dropper
  _vcm->EnableFrameDropper(false);

  I420VideoFrame sourceFrame;
  I420VideoFrame *decimatedFrame = NULL;
  uint8_t* tmpBuffer = new uint8_t[_lengthSourceFrame];
  double startTime = clock()/(double)CLOCKS_PER_SEC;
  _vcm->SetChannelParameters(static_cast<uint32_t>(1000 * _bitRate), 0, 0);

  SendStatsTest sendStats;
  sendStats.set_framerate(static_cast<uint32_t>(_frameRate));
  sendStats.set_bitrate(1000 * _bitRate);
  _vcm->RegisterSendStatisticsCallback(&sendStats);

  VideoContentMetrics* contentMetrics = NULL;
  // setting user frame rate
  // for starters: keeping native values:
  _vpm->SetTargetResolution(_width, _height,
                            (uint32_t)(_frameRate+ 0.5f));
  _decodeCallback.SetOriginalFrameDimensions(_nativeWidth, _nativeHeight);

  //tmp  - disabling VPM frame dropping
  _vpm->EnableTemporalDecimation(false);

  int32_t ret = 0;
  _numFramesDroppedVPM = 0;

  do {
    if (fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0) {
      _frameCnt++;
      int size_y = _nativeWidth * _nativeHeight;
      int size_uv = ((_nativeWidth + 1) / 2) * ((_nativeHeight  + 1) / 2);
      sourceFrame.CreateFrame(size_y, tmpBuffer,
                              size_uv, tmpBuffer + size_y,
                              size_uv, tmpBuffer + size_y + size_uv,
                              _nativeWidth, _nativeHeight,
                              _nativeWidth, (_nativeWidth + 1) / 2,
                              (_nativeWidth + 1) / 2);

      _timeStamp +=
          (uint32_t)(9e4 / static_cast<float>(codec.maxFramerate));
      sourceFrame.set_timestamp(_timeStamp);

      ret = _vpm->PreprocessFrame(sourceFrame, &decimatedFrame);
      if (ret  == 1)
      {
        printf("VD: frame drop %d \n",_frameCnt);
        _numFramesDroppedVPM += 1;
        continue; // frame drop
      }
      else if (ret < 0)
      {
        printf("Error in PreprocessFrame: %d\n", ret);
        //exit(1);
      }
      // Frame was not re-sampled => use original.
      if (decimatedFrame == NULL)
      {
        decimatedFrame = &sourceFrame;
      }
      contentMetrics = _vpm->ContentMetrics();
      if (contentMetrics == NULL)
      {
        printf("error: contentMetrics = NULL\n");
      }

      // counting only encoding time
      _encodeTimes[int(sourceFrame.timestamp())] =
          clock()/(double)CLOCKS_PER_SEC;

      int32_t ret = _vcm->AddVideoFrame(*decimatedFrame, contentMetrics);

      _totalEncodeTime += clock()/(double)CLOCKS_PER_SEC -
          _encodeTimes[int(sourceFrame.timestamp())];

      if (ret < 0)
      {
        printf("Error in AddFrame: %d\n", ret);
        //exit(1);
      }

      // Same timestamp value for encode and decode
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
      // mimicking setTargetRates - update every 1 sec
      // this will trigger QMSelect
      if (_frameCnt%((int)_frameRate) == 0)
      {
        _vcm->SetChannelParameters(static_cast<uint32_t>(1000 * _bitRate), 0,
                                   1);
      }

      // check for bit rate update
      if (change < numChanges && _frameCnt == updateFrameNum[change])
      {
        _bitRate = bitRateUpdate[change];
        _frameRate = frameRateUpdate[change];
        codec.startBitrate = (int)_bitRate;
        codec.maxFramerate = (uint8_t) _frameRate;
        // Will also set and init the desired codec
        TEST(_vcm->RegisterSendCodec(&codec, 2, 1440) == VCM_OK);
        change++;
      }

      DataLog::InsertCell(feature_table_name_, "motion magnitude",
                          contentMetrics->motion_magnitude);
      DataLog::InsertCell(feature_table_name_, "spatial prediction error",
                          contentMetrics->spatial_pred_err);
      DataLog::InsertCell(feature_table_name_, "spatial pred err horizontal",
                          contentMetrics->spatial_pred_err_h);
      DataLog::InsertCell(feature_table_name_, "spatial pred err vertical",
                          contentMetrics->spatial_pred_err_v);

      DataLog::InsertCell(feature_table_name_, "width", _nativeHeight);
      DataLog::InsertCell(feature_table_name_, "height", _nativeWidth);

      DataLog::InsertCell(feature_table_name_, "num pixels",
                          _nativeHeight * _nativeWidth);

      DataLog::InsertCell(feature_table_name_, "frame rate", _nativeFrameRate);
      DataLog::NextRow(feature_table_name_);

      static_cast<SimulatedClock*>(_clock)->AdvanceTimeMilliseconds(
          1000 / _nativeFrameRate);
  }

  } while (feof(_sourceFile) == 0);
  _decodeCallback.WriteEnd(_frameCnt);


  double endTime = clock()/(double)CLOCKS_PER_SEC;
  _testTotalTime = endTime - startTime;
  _sumEncBytes = _encodeCompleteCallback.EncodedBytes();

  delete tmpBuffer;
  delete waitEvent;
  _vpm->Reset();
  Teardown();
  Print();
  VideoProcessingModule::Destroy(_vpm);
  return 0;
}

// implementing callback to be called from
// VCM to update VPM of frame rate and size
QMTestVideoSettingsCallback::QMTestVideoSettingsCallback():
_vpm(NULL),
_vcm(NULL)
{
  //
}

void
QMTestVideoSettingsCallback::RegisterVPM(VideoProcessingModule *vpm)
{
  _vpm = vpm;
}
void
QMTestVideoSettingsCallback::RegisterVCM(VideoCodingModule *vcm)
{
  _vcm = vcm;
}

bool
QMTestVideoSettingsCallback::Updated()
{
  if (_updated)
  {
    _updated = false;
    return true;
  }
  return false;
}

int32_t
QMTestVideoSettingsCallback::SetVideoQMSettings(const uint32_t frameRate,
                                                const uint32_t width,
                                                const uint32_t height)
{
  int32_t retVal = 0;
  printf("QM updates: W = %d, H = %d, FR = %d, \n", width, height, frameRate);
  retVal = _vpm->SetTargetResolution(width, height, frameRate);
  //Initialize codec with new values - is this the best place to do it?
  if (!retVal)
  {
    // first get current settings
    VideoCodec currentCodec;
    _vcm->SendCodec(&currentCodec);
    // now set new values:
    currentCodec.height = (uint16_t)height;
    currentCodec.width = (uint16_t)width;
    currentCodec.maxFramerate = (uint8_t)frameRate;

    // re-register encoder
    retVal = _vcm->RegisterSendCodec(&currentCodec, 2, 1440);
    _updated = true;
  }

  return retVal;
}

// Decoded Frame Callback Implementation
VCMQMDecodeCompleCallback::VCMQMDecodeCompleCallback(
    FILE* decodedFile, int frame_rate, std::string feature_table_name):
_decodedFile(decodedFile),
_decodedBytes(0),
//_test(test),
_origWidth(0),
_origHeight(0),
_decWidth(0),
_decHeight(0),
//_interpolator(NULL),
_decBuffer(NULL),
_frameCnt(0),
frame_rate_(frame_rate),
frames_cnt_since_drop_(0),
feature_table_name_(feature_table_name)
{
    //
}

VCMQMDecodeCompleCallback::~VCMQMDecodeCompleCallback()
 {
//     if (_interpolator != NULL)
//     {
//         deleteInterpolator(_interpolator);
//         _interpolator = NULL;
//     }
   if (_decBuffer != NULL)
   {
     delete [] _decBuffer;
     _decBuffer = NULL;
   }
 }

int32_t
VCMQMDecodeCompleCallback::FrameToRender(I420VideoFrame& videoFrame)
{
  ++frames_cnt_since_drop_;

  // When receiving the first coded frame the last_frame variable is not set
  if (last_frame_.IsZeroSize()) {
    last_frame_.CopyFrame(videoFrame);
  }

   // Check if there were frames skipped.
  int num_frames_skipped = static_cast<int>( 0.5f +
  (videoFrame.timestamp() - (last_frame_.timestamp() + (9e4 / frame_rate_))) /
  (9e4 / frame_rate_));

  // If so...put the last frames into the encoded stream to make up for the
  // skipped frame(s)
  while (num_frames_skipped > 0) {
    PrintI420VideoFrame(last_frame_, _decodedFile);
    _frameCnt++;
    --num_frames_skipped;
    frames_cnt_since_drop_ = 1; // Reset counter

  }

  DataLog::InsertCell(
        feature_table_name_,"num frames since drop",frames_cnt_since_drop_);

  if (_origWidth == videoFrame.width() && _origHeight == videoFrame.height())
  {
    if (PrintI420VideoFrame(videoFrame, _decodedFile) < 0) {
      return -1;
    }
    _frameCnt++;
    // no need for interpolator and decBuffer
    if (_decBuffer != NULL)
    {
      delete [] _decBuffer;
      _decBuffer = NULL;
    }
    _decWidth = 0;
    _decHeight = 0;
  }
  else
  {
    // TODO(mikhal): Add support for scaling.
    return -1;
  }

  _decodedBytes += CalcBufferSize(kI420, videoFrame.width(),
                                  videoFrame.height());
  videoFrame.SwapFrame(&last_frame_);
  return VCM_OK;
}

int32_t VCMQMDecodeCompleCallback::DecodedBytes()
{
  return _decodedBytes;
}

void VCMQMDecodeCompleCallback::SetOriginalFrameDimensions(int32_t width,
                                                           int32_t height)
{
  _origWidth = width;
  _origHeight = height;
}

int32_t VCMQMDecodeCompleCallback::buildInterpolator()
{
  uint32_t decFrameLength  = _origWidth*_origHeight*3 >> 1;
  if (_decBuffer != NULL)
  {
    delete [] _decBuffer;
  }
  _decBuffer = new uint8_t[decFrameLength];
  if (_decBuffer == NULL)
  {
    return -1;
  }
  return 0;
}

// This function checks if the total number of frames processed in the encoding
// process is the same as the number of frames rendered. If not,  the last
// frame (or several consecutive frames from the end) must have been dropped. If
// this is the case, the last frame is repeated so that there are as many
// frames rendered as there are number of frames encoded.
void VCMQMDecodeCompleCallback::WriteEnd(int input_frame_count)
{
  int num_missing_frames = input_frame_count - _frameCnt;

  for (int n = num_missing_frames; n > 0; --n) {
    PrintI420VideoFrame(last_frame_, _decodedFile);
    _frameCnt++;
  }
}
