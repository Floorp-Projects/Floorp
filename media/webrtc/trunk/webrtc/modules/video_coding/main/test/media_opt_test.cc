/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implementation of Media Optimization Test
// testing is done via the VCM module, no specific Media opt functionality.

#include "webrtc/modules/video_coding/main/test/media_opt_test.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_macros.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"

using namespace webrtc;

int MediaOptTest::RunTest(int testNum, CmdArgs& args)
{
    Trace::CreateTrace();
    Trace::SetTraceFile((test::OutputPath() + "mediaOptTestTrace.txt").c_str());
    Trace::set_level_filter(webrtc::kTraceAll);
    VideoCodingModule* vcm = VideoCodingModule::Create(1);
    Clock* clock = Clock::GetRealTimeClock();
    MediaOptTest* mot = new MediaOptTest(vcm, clock);
    if (testNum == 0)
    { // regular
         mot->Setup(0, args);
         mot->GeneralSetup();
         mot->Perform();
         mot->Print(1);// print to screen
         mot->TearDown();
    }
    if (testNum == 1)
    {   // release test
        mot->Setup(0, args);
        mot->RTTest();
    }
    if (testNum == 2)
    { // release test, running from script
         mot->Setup(1, args);
         mot->GeneralSetup();
         mot->Perform();
         mot->Print(1);// print to screen
         mot->TearDown();
    }

    VideoCodingModule::Destroy(vcm);
    delete mot;
    Trace::ReturnTrace();
    return 0;

}


MediaOptTest::MediaOptTest(VideoCodingModule* vcm, Clock* clock)
    : _vcm(vcm),
      _rtp(NULL),
      _outgoingTransport(NULL),
      _dataCallback(NULL),
      _clock(clock),
      _width(0),
      _height(0),
      _lengthSourceFrame(0),
      _timeStamp(0),
      _frameRate(30.0f),
      _nackEnabled(false),
      _fecEnabled(false),
      _rttMS(0),
      _bitRate(300.0f),
      _lossRate(0.0f),
      _renderDelayMs(0),
      _frameCnt(0),
      _sumEncBytes(0),
      _numFramesDropped(0),
      _numberOfCores(4) {
}

MediaOptTest::~MediaOptTest() {
  delete _rtp;
}

void MediaOptTest::Setup(int testType, CmdArgs& args) {
    /*TEST USER SETTINGS*/
    // test parameters
    _inname = args.inputFile;
    if (args.outputFile == "")
        _outname = test::OutputPath() + "MOTest_out.vp8";
    else
        _outname = args.outputFile;
    // actual source after frame dropping
    _actualSourcename = test::OutputPath() + "MOTestSource.yuv";
    _codecName = args.codecName;
    _sendCodecType = args.codecType;
    _width = args.width;
    _height = args.height;
    _frameRate = args.frameRate;
    _bitRate = args.bitRate;
    _numberOfCores = 4;

    // error resilience
    _nackEnabled = false;
    _fecEnabled = true;
    _nackFecEnabled = false;

    _rttMS = 100;
    _lossRate = 0.00*255; // no packet loss

    _testType = testType;

    //For multiple runs with script
    if (_testType == 1)
    {
        float rateTest,lossTest;
        int numRuns;
        _fpinp = fopen("dat_inp","rb");
        _fpout = fopen("test_runs/dat_out","ab");
        _fpout2 = fopen("test_runs/dat_out2","ab");
        TEST(fscanf(_fpinp,"%f %f %d \n",&rateTest,&lossTest,&numRuns) > 0);
        _bitRate = rateTest;
        _lossRate = lossTest;
        _testNum = 0;

        // for bit rates: 500, 1000, 2000, 3000,4000
        // for loss rates: 0, 1, 3, 5, 10%
        _numParRuns = 25;

        _testNum = numRuns + 1;
        if (rateTest == 0.0) _lossRate = 0.0;
        else
        {
            if (rateTest == 4000)  //final bit rate
            {
                if (lossTest == 0.1*255) _lossRate = 0.0;  //start at 1%
                else
                    if (lossTest == 0.05*255) _lossRate = 0.1*255;  //final loss rate
                    else
                        if (lossTest == 0.0) _lossRate = 0.01*255;
                        else _lossRate = lossTest + 0.02*255;
            }
        }

        if (rateTest == 0.0 || rateTest == 4000) _bitRate = 500; //starting bit rate
        else
            if (rateTest == 500) _bitRate = 1000;
                else _bitRate = rateTest +  1000;
    }
   //

    _renderDelayMs = 0;
    /* test settings end*/

   _lengthSourceFrame  = 3*_width*_height/2;
    _log.open((test::OutputPath() + "VCM_MediaOptLog.txt").c_str(),
              std::fstream::out | std::fstream::app);
}

void
MediaOptTest::GeneralSetup()
{
    uint32_t minPlayoutDelayMs = 0;

    if ((_sourceFile = fopen(_inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", _inname.c_str());
        exit(1);
    }

    if ((_decodedFile = fopen(_outname.c_str(), "wb")) == NULL)
    {
        printf("Cannot read file %s.\n", _outname.c_str());
        exit(1);
    }

    if ((_actualSourceFile = fopen(_actualSourcename.c_str(), "wb")) == NULL)
    {
        printf("Cannot read file %s.\n", _actualSourcename.c_str());
        exit(1);
    }
    if (_vcm->InitializeReceiver() < 0)
    {
        exit(1);
    }
    if (_vcm->InitializeSender())
    {
        exit(1);
    }
    _outgoingTransport = new RTPSendCompleteCallback(_clock);
    _dataCallback = new RtpDataCallback(_vcm);

    RtpRtcp::Configuration configuration;
    configuration.id = 1;
    configuration.audio = false;
    configuration.outgoing_transport = _outgoingTransport;
    _rtp = RtpRtcp::CreateRtpRtcp(configuration);

    _outgoingTransport->SetRtpModule(_rtp);

    // Registering codecs for the RTP module

    // Register receive and send payload
    VideoCodec video_codec;
    strncpy(video_codec.plName, "VP8", 32);
    video_codec.plType = VCM_VP8_PAYLOAD_TYPE;
    rtp_receiver_->RegisterReceivePayload(video_codec.plName,
                                          video_codec.plType,
                                          90000,
                                          0,
                                          video_codec.maxBitrate);
    _rtp->RegisterSendPayload(video_codec);

    strncpy(video_codec.plName, "ULPFEC", 32);
    video_codec.plType = VCM_ULPFEC_PAYLOAD_TYPE;
    rtp_receiver_->RegisterReceivePayload(video_codec.plName,
                                          video_codec.plType,
                                          90000,
                                          0,
                                          video_codec.maxBitrate);
    _rtp->RegisterSendPayload(video_codec);

    strncpy(video_codec.plName, "RED", 32);
    video_codec.plType = VCM_RED_PAYLOAD_TYPE;
    rtp_receiver_->RegisterReceivePayload(video_codec.plName,
                                          video_codec.plType,
                                          90000,
                                          0,
                                          video_codec.maxBitrate);
    _rtp->RegisterSendPayload(video_codec);

    if (_nackFecEnabled == 1)
        _rtp->SetGenericFECStatus(_nackFecEnabled, VCM_RED_PAYLOAD_TYPE,
                VCM_ULPFEC_PAYLOAD_TYPE);
    else
        _rtp->SetGenericFECStatus(_fecEnabled, VCM_RED_PAYLOAD_TYPE,
                VCM_ULPFEC_PAYLOAD_TYPE);

    // VCM: Registering codecs
    VideoCodec sendCodec;
    _vcm->InitializeSender();
    _vcm->InitializeReceiver();
    int32_t numberOfCodecs = _vcm->NumberOfCodecs();
    if (numberOfCodecs < 1)
    {
        exit(1);
    }

    if (_vcm->Codec(_sendCodecType, &sendCodec) != 0)
    {
        printf("Unknown codec\n");
        exit(1);
    }
    // register codec
    sendCodec.startBitrate = (int) _bitRate;
    sendCodec.height = _height;
    sendCodec.width = _width;
    sendCodec.maxFramerate = (uint8_t)_frameRate;
    _vcm->RegisterSendCodec(&sendCodec, _numberOfCores, 1440);
    _vcm->RegisterReceiveCodec(&sendCodec, _numberOfCores); // same settings for encode and decode

    _vcm->SetRenderDelay(_renderDelayMs);
    _vcm->SetMinimumPlayoutDelay(minPlayoutDelayMs);
}
// The following test shall be conducted under release tests



int32_t
MediaOptTest::Perform()
{
    VCMDecodeCompleteCallback receiveCallback(_decodedFile);

    VCMRTPEncodeCompleteCallback* encodeCompleteCallback = new VCMRTPEncodeCompleteCallback(_rtp);
    _vcm->RegisterTransportCallback(encodeCompleteCallback);
    encodeCompleteCallback->SetCodecType(ConvertCodecType(_codecName.c_str()));
    encodeCompleteCallback->SetFrameDimensions(_width, _height);

    // callback settings
    VideoProtectionCallback  protectionCallback;
    protectionCallback.RegisterRtpModule(_rtp);
    _vcm->RegisterProtectionCallback(&protectionCallback);

    // set error resilience / test parameters:
    _outgoingTransport->SetLossPct(_lossRate);
    if (_nackFecEnabled == 1) {
        _vcm->SetVideoProtection(kProtectionNackFEC, _nackFecEnabled);
    } else {
        _vcm->SetVideoProtection(kProtectionNack, _nackEnabled);
        _vcm->SetVideoProtection(kProtectionFEC, _fecEnabled);
    }

    // START TEST
    I420VideoFrame sourceFrame;
    uint8_t* tmpBuffer = new uint8_t[_lengthSourceFrame];
    _vcm->SetChannelParameters(static_cast<uint32_t>(1000 * _bitRate),
                               (uint8_t)_lossRate, _rttMS);
    _vcm->RegisterReceiveCallback(&receiveCallback);

    _frameCnt  = 0;
    _sumEncBytes = 0.0;
    _numFramesDropped = 0;
    int half_width = (_width + 1) / 2;
    int half_height = (_height + 1) / 2;
    int size_y = _width * _height;
    int size_uv = half_width * half_height;

    while (feof(_sourceFile)== 0)
    {
        TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
        _frameCnt++;
        sourceFrame.CreateFrame(size_y, tmpBuffer,
                                size_uv, tmpBuffer + size_y,
                                size_uv, tmpBuffer + size_y + size_uv,
                                _width, _height,
                                _width, half_width, half_width);
        _timeStamp += (uint32_t)(9e4 / static_cast<float>(_frameRate));
        sourceFrame.set_timestamp(_timeStamp);
        TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
        // inform RTP Module of error resilience features
        //_rtp->SetFECCodeRate(protectionCallback.FECKeyRate(),protectionCallback.FECDeltaRate());
        //_rtp->SetNACKStatus(protectionCallback.NACKMethod());

        int32_t ret = _vcm->Decode();
        if (ret < 0 )
        {
            TEST(ret == 0);
            printf ("Decode error in frame # %d",_frameCnt);
        }

        float encBytes = encodeCompleteCallback->EncodedBytes();
        if (encBytes == 0)
        {
            _numFramesDropped += 1;
            //printf("frame #%d dropped \n", _frameCnt );
        }
        else
        {
          // write frame to file
          if (PrintI420VideoFrame(sourceFrame, _actualSourceFile) < 0) {
            return -1;
          }
        }

        _sumEncBytes += encBytes;
    }

    //END TEST
    delete encodeCompleteCallback;
    delete tmpBuffer;

return 0;

}

void
MediaOptTest::RTTest()
{
    // will only calculate PSNR - not create output files for all
    // SET UP
    // Set bit rates
    const float bitRateVec[] = {500, 1000, 2000,3000, 4000};
    //const float bitRateVec[] = {1000};
    // Set Packet loss values ([0,255])
    const double lossPctVec[]     = {0.0*255, 0.0*255, 0.01*255, 0.01*255, 0.03*255, 0.03*255, 0.05*255, 0.05*255, 0.1*255, 0.1*255};
    const bool  nackEnabledVec[] = {false  , false, false, false, false, false, false, false , false, false};
    const bool  fecEnabledVec[]  = {false  , true,  false, true , false, true , false, true , false, true};
    // fec and nack are set according to the packet loss values

    const float nBitrates = sizeof(bitRateVec)/sizeof(*bitRateVec);
    const float nlossPct = sizeof(lossPctVec)/sizeof(*lossPctVec);

    std::vector<const VideoSource*> sources;
    std::vector<const VideoSource*>::iterator it;

    sources.push_back(new const VideoSource(_inname, _width, _height));
    int numOfSrc = 1;

    // constant settings (valid for entire run time)
    _rttMS = 20;
    _renderDelayMs = 0;

    // same out name for all
    _outname = test::OutputPath() + "RTMOTest_out.yuv";
    // actual source after frame dropping
    _actualSourcename = test::OutputPath() + "RTMOTestSource.yuv";

    _codecName = "VP8";  // for now just this one - later iterate over all codec types
    _log.open((test::OutputPath() + "/VCM_RTMediaOptLog.txt").c_str(),
              std::fstream::out | std::fstream::app);
    _outputRes=fopen((test::OutputPath() + "VCM_MediaOptResults.txt").c_str(),
                     "ab");

    //char filename[128];
    /* test settings end*/

    // START TEST
    // iterate over test sequences
    printf("\n****START TEST OVER ALL RUNS ****\n");
    int runCnt = 0;
    for (it = sources.begin() ; it < sources.end(); it++)
    {

        // test set up
        _inname = (*it)->GetFileName();
        _width  = (*it)->GetWidth();
        _height = (*it)->GetHeight();
        _lengthSourceFrame  = 3*_width*_height/2;
        _frameRate = (*it)->GetFrameRate();
         //GeneralSetup();


        // iterate over all bit rates
        for (int i = 0; i < nBitrates; i++)
        {
           _bitRate = static_cast<float>(bitRateVec[i]);
            // iterate over all packet loss values
            for (int j = 0; j < nlossPct; j++)
            {
                 _lossRate = static_cast<float>(lossPctVec[j]);
                 _nackEnabled = static_cast<bool>(nackEnabledVec[j]);
                 _fecEnabled = static_cast<bool>(fecEnabledVec[j]);

                 runCnt++;
                 printf("run #%d out of %d \n", runCnt,(int)(nlossPct*nBitrates*numOfSrc));

                //printf("**FOR RUN: **%d %d %d %d \n",_nackEnabled,_fecEnabled,int(lossPctVec[j]),int(_bitRate));

                 /*
                 int ch = sprintf(filename,"../test_mediaOpt/RTMOTest_%d_%d_%d_%d.yuv",_nackEnabled,_fecEnabled,int(lossPctVec[j]),int(_bitRate));
                _outname = filename;

                printf("**FOR RUN: **%d %d %d %d \n",_nackEnabled,_fecEnabled,int(lossPctVec[j]),int(_bitRate));
               */
                 GeneralSetup();
                 Perform();
                 Print(1);
                 TearDown();

                 printf("\n");
                  //printf("**DONE WITH RUN: **%d %d %f %d \n",_nackEnabled,_fecEnabled,lossPctVec[j],int(_bitRate));
                 //

            }// end of packet loss loop
        }// end of bit rate loop
        delete *it;
    }// end of video sequence loop
    // at end of sequence
    fclose(_outputRes);
    printf("\nVCM Media Optimization Test: \n\n%i tests completed\n", vcmMacrosTests);
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
MediaOptTest::Print(int mode)
{
    double ActualBitRate =  8.0 *( _sumEncBytes / (_frameCnt / _frameRate));
    double actualBitRate = ActualBitRate / 1000.0;
    webrtc::test::QualityMetricsResult psnr;
    I420PSNRFromFiles(_actualSourcename.c_str(), _outname.c_str(), _width,
                      _height, &psnr);

    (_log) << "VCM: Media Optimization Test Cycle Completed!" << std::endl;
    (_log) << "Input file: " << _inname << std::endl;
    (_log) << "Output file:" << _outname << std::endl;
    ( _log) << "Actual bitrate: " << actualBitRate<< " kbps\tTarget: " << _bitRate << " kbps" << std::endl;
    (_log) << "Error Reslience: NACK:" << _nackEnabled << "; FEC: " << _fecEnabled << std::endl;
    (_log) << "Packet Loss applied= %f " << _lossRate << std::endl;
    (_log) << _numFramesDropped << " FRames were dropped" << std::endl;
     ( _log) << "PSNR: " << psnr.average << std::endl;
    (_log) << std::endl;

    if (_testType == 2)
    {
        fprintf(_outputRes,"************\n");
        fprintf(_outputRes,"\n\n\n");
        fprintf(_outputRes,"Actual bitrate: %f kbps\n", actualBitRate);
        fprintf(_outputRes,"Target bitrate: %f kbps\n", _bitRate);
        fprintf(_outputRes,"NACK: %s  ",(_nackEnabled)?"true":"false");
        fprintf(_outputRes,"FEC: %s \n ",(_fecEnabled)?"true":"false");
        fprintf(_outputRes,"Packet loss applied = %f\n", _lossRate);
        fprintf(_outputRes,"%d frames were dropped, and total number of frames processed %d  \n",_numFramesDropped,_frameCnt);
        fprintf(_outputRes,"PSNR: %f \n", psnr.average);
        fprintf(_outputRes,"************\n");
    }


    //
    if (_testType == 1)
    {
        fprintf(_fpout,"************\n");
        fprintf(_fpout,"\n\n\n");
        fprintf(_fpout,"Actual bitrate: %f kbps\n", actualBitRate);
        fprintf(_fpout,"Target bitrate: %f kbps\n", _bitRate);
        fprintf(_fpout,"NACK: %s  ",(_nackEnabled)?"true":"false");
        fprintf(_fpout,"FEC: %s \n ",(_fecEnabled)?"true":"false");
        fprintf(_fpout,"Packet loss applied = %f\n", _lossRate);
        fprintf(_fpout,"%d frames were dropped, and total number of frames processed %d  \n",_numFramesDropped,_frameCnt);
        fprintf(_fpout,"PSNR: %f \n", psnr.average);
        fprintf(_fpout,"************\n");

        int testNum1 = _testNum/(_numParRuns +1) + 1;
        int testNum2 = _testNum%_numParRuns;
        if (testNum2 == 0) testNum2 = _numParRuns;
        fprintf(_fpout2,"%d %d %f %f %f %f \n",testNum1,testNum2,_bitRate,actualBitRate,_lossRate,psnr.average);
        fclose(_fpinp);
        _fpinp = fopen("dat_inp","wb");
        fprintf(_fpinp,"%f %f %d \n",_bitRate,_lossRate,_testNum);
    }
    //


    if (mode == 1)
    {
        // print to screen
        printf("\n\n\n");
        printf("Actual bitrate: %f kbps\n", actualBitRate);
        printf("Target bitrate: %f kbps\n", _bitRate);
        printf("NACK: %s  ",(_nackEnabled)?"true":"false");
        printf("FEC: %s \n",(_fecEnabled)?"true":"false");
        printf("Packet loss applied = %f\n", _lossRate);
        printf("%d frames were dropped, and total number of frames processed %d  \n",_numFramesDropped,_frameCnt);
        printf("PSNR: %f \n", psnr.average);
    }
    TEST(psnr.average > 10); // low becuase of possible frame dropping (need to verify that OK for all packet loss values/ rates)
}

void MediaOptTest::TearDown() {
  delete _rtp;
  _rtp = NULL;
  delete _outgoingTransport;
  _outgoingTransport = NULL;
  delete _dataCallback;
  _dataCallback = NULL;
  _log.close();
  fclose(_sourceFile);
  fclose(_decodedFile);
  fclose(_actualSourceFile);
}
