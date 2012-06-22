/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "benchmark.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
#if defined(_WIN32)
    #include <windows.h>
#endif

#include "common_video/libyuv/include/libyuv.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "modules/video_coding/codecs/test_framework/video_source.h"
#include "testsupport/fileutils.h"
#include "testsupport/metrics/video_metrics.h"

#define SSIM_CALC 0 // by default, don't compute SSIM

using namespace webrtc;

Benchmark::Benchmark()
:
NormalAsyncTest("Benchmark", "Codec benchmark over a range of test cases", 6),
_resultsFileName(webrtc::test::OutputPath() + "benchmark.txt"),
_codecName("Default")
{
}

Benchmark::Benchmark(std::string name, std::string description)
:
NormalAsyncTest(name, description, 6),
_resultsFileName(webrtc::test::OutputPath() + "benchmark.txt"),
_codecName("Default")
{
}

Benchmark::Benchmark(std::string name, std::string description, std::string resultsFileName, std::string codecName)
:
NormalAsyncTest(name, description, 6),
_resultsFileName(resultsFileName),
_codecName(codecName)
{
}

void
Benchmark::Perform()
{
    std::vector<const VideoSource*> sources;
    std::vector<const VideoSource*>::iterator it;

    // Configuration --------------------------
    sources.push_back(new const VideoSource(webrtc::test::ProjectRootPath() +
                                            "resources/foreman_cif.yuv", kCIF));
//    sources.push_back(new const VideoSource(webrtc::test::ProjectRootPath() +
//                                            "resources/akiyo_cif.yuv", kCIF));

    const VideoSize size[] = {kQCIF, kCIF};
    const int frameRate[] = {10, 15, 30};
    // Specifies the framerates for which to perform a speed test.
    const bool speedTestMask[] = {false, false, false};
    const int bitRate[] = {50, 100, 200, 300, 400, 500, 600, 1000};
    // Determines the number of iterations to perform to arrive at the speed result.
    enum { kSpeedTestIterations = 10 };
    // ----------------------------------------

    const int nFrameRates = sizeof(frameRate)/sizeof(*frameRate);
    assert(sizeof(speedTestMask)/sizeof(*speedTestMask) == nFrameRates);
    const int nBitrates = sizeof(bitRate)/sizeof(*bitRate);
    int testIterations = 10;

    webrtc::test::QualityMetricsResult psnr[nBitrates];
    webrtc::test::QualityMetricsResult ssim[nBitrates];
    double fps[nBitrates];
    double totalEncodeTime[nBitrates];
    double totalDecodeTime[nBitrates];

    _results.open(_resultsFileName.c_str(), std::fstream::out);
    _results << GetMagicStr() << std::endl;
    _results << _codecName << std::endl;

    for (it = sources.begin() ; it < sources.end(); it++)
    {
        for (int i = 0; i < static_cast<int>(sizeof(size)/sizeof(*size)); i++)
        {
            for (int j = 0; j < nFrameRates; j++)
            {
                std::stringstream ss;
                std::string strFrameRate;
                std::string outFileName;
                ss << frameRate[j];
                ss >> strFrameRate;
                outFileName = (*it)->GetFilePath() + "/" + (*it)->GetName() + "_" +
                    VideoSource::GetSizeString(size[i]) + "_" + strFrameRate + ".yuv";

                _target = new const VideoSource(outFileName, size[i], frameRate[j]);
                (*it)->Convert(*_target);
                if (VideoSource::FileExists(outFileName.c_str()))
                {
                    _inname = outFileName;
                }
                else
                {
                    _inname = (*it)->GetFileName();
                }

                std::cout << (*it)->GetName() << ", " << VideoSource::GetSizeString(size[i])
                    << ", " << frameRate[j] << " fps" << std::endl << "Bitrate [kbps]:";
                _results << (*it)->GetName() << "," << VideoSource::GetSizeString(size[i])
                    << "," << frameRate[j] << " fps" << std::endl << "Bitrate [kbps]";

                if (speedTestMask[j])
                {
                    testIterations = kSpeedTestIterations;
                }
                else
                {
                    testIterations = 1;
                }

                for (int k = 0; k < nBitrates; k++)
                {
                    _bitRate = (bitRate[k]);
                    double avgFps = 0.0;
                    totalEncodeTime[k] = 0;
                    totalDecodeTime[k] = 0;

                    for (int l = 0; l < testIterations; l++)
                    {
                        PerformNormalTest();
                        _appendNext = false;

                        avgFps += _framecnt / (_totalEncodeTime + _totalDecodeTime);
                        totalEncodeTime[k] += _totalEncodeTime;
                        totalDecodeTime[k] += _totalDecodeTime;

                    }
                    avgFps /= testIterations;
                    totalEncodeTime[k] /= testIterations;
                    totalDecodeTime[k] /= testIterations;

                    double actualBitRate = ActualBitRate(_framecnt) / 1000.0;
                    std::cout << " " << actualBitRate;
                    _results << "," << actualBitRate;
                    webrtc::test::QualityMetricsResult psnr_result;
                    I420PSNRFromFiles(_inname.c_str(), _outname.c_str(),
                                      _inst.width, _inst.height, &psnr[k]);
                    if (SSIM_CALC)
                    {
                        webrtc::test::QualityMetricsResult ssim_result;
                        I420SSIMFromFiles(_inname.c_str(), _outname.c_str(),
                                          _inst.width, _inst.height, &ssim[k]);

                    }
                    fps[k] = avgFps;
                }
                std::cout << std::endl << "Y-PSNR [dB]:";
                _results << std::endl << "Y-PSNR [dB]";
                for (int k = 0; k < nBitrates; k++)
                {
                    std::cout << " " << psnr[k].average;
                    _results << "," << psnr[k].average;

                }
                if (SSIM_CALC)
                {
                    std::cout << std::endl << "SSIM: ";
                    _results << std::endl << "SSIM ";
                    for (int k = 0; k < nBitrates; k++)
                    {
                        std::cout << " " << ssim[k].average;
                        _results << "," << ssim[k].average;
                    }

                }

                std::cout << std::endl << "Encode Time[ms]:";
                _results << std::endl << "Encode Time[ms]";
                for (int k = 0; k < nBitrates; k++)
                {
                    std::cout << " " << totalEncodeTime[k];
                    _results << "," << totalEncodeTime[k];

                }

                std::cout << std::endl << "Decode Time[ms]:";
                _results << std::endl << "Decode Time[ms]";
                for (int k = 0; k < nBitrates; k++)
                {
                    std::cout << " " << totalDecodeTime[k];
                    _results << "," << totalDecodeTime[k];

                }

                if (speedTestMask[j])
                {
                    std::cout << std::endl << "Speed [fps]:";
                    _results << std::endl << "Speed [fps]";
                    for (int k = 0; k < nBitrates; k++)
                    {
                        std::cout << " " << static_cast<int>(fps[k] + 0.5);
                        _results << "," << static_cast<int>(fps[k] + 0.5);
                    }
                }
                std::cout << std::endl << std::endl;
                _results << std::endl << std::endl;

                delete _target;
            }
        }
        delete *it;
    }
    _results.close();
}

void
Benchmark::PerformNormalTest()
{
    _encoder = GetNewEncoder();
    _decoder = GetNewDecoder();
    CodecSettings(_target->GetWidth(), _target->GetHeight(), _target->GetFrameRate(), _bitRate);
    Setup();
    EventWrapper* waitEvent = EventWrapper::Create();

    _inputVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);
    _decodedVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);
    _encoder->InitEncode(&_inst, 4, 1440);
    CodecSpecific_InitBitrate();
    _decoder->InitDecode(&_inst,1);

    FrameQueue frameQueue;
    VideoEncodeCompleteCallback encCallback(_encodedFile, &frameQueue, *this);
    VideoDecodeCompleteCallback decCallback(_decodedFile, *this);
    _encoder->RegisterEncodeCompleteCallback(&encCallback);
    _decoder->RegisterDecodeCompleteCallback(&decCallback);

    SetCodecSpecificParameters();

    _totalEncodeTime = _totalDecodeTime = 0;
    _totalEncodePipeTime = _totalDecodePipeTime = 0;
    bool complete = false;
    _framecnt = 0;
    _encFrameCnt = 0;
    _sumEncBytes = 0;
    _lengthEncFrame = 0;
    while (!complete)
    {
        complete = Encode();
        if (!frameQueue.Empty() || complete)
        {
            while (!frameQueue.Empty())
            {
                _frameToDecode = static_cast<FrameQueueTuple *>(frameQueue.PopFrame());
                DoPacketLoss();
                int ret = Decode();
                delete _frameToDecode;
                _frameToDecode = NULL;
                if (ret < 0)
                {
                    fprintf(stderr,"\n\nError in decoder: %d\n\n", ret);
                    exit(EXIT_FAILURE);
                }
                else if (ret == 0)
                {
                    _framecnt++;
                }
                else
                {
                    fprintf(stderr, "\n\nPositive return value from decode!\n\n");
                }
            }
        }
        waitEvent->Wait(5);
    }

    _inputVideoBuffer.Free();
    //_encodedVideoBuffer.Reset(); ?
    _encodedVideoBuffer.Free();
    _decodedVideoBuffer.Free();

    _encoder->Release();
    _decoder->Release();
    delete waitEvent;
    delete _encoder;
    delete _decoder;
    Teardown();
}

void
Benchmark::CodecSpecific_InitBitrate()
{
    if (_bitRate == 0)
    {
        _encoder->SetRates(600, _inst.maxFramerate);
    }
    else
    {
        _encoder->SetRates(_bitRate, _inst.maxFramerate);
    }
}

