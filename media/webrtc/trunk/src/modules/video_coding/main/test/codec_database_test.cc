/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implementation of codec data base test
// testing is done via the VCM module, no specific CodecDataBase module functionality.

#include "codec_database_test.h"

#include <assert.h>
#include <stdio.h>

#include "../../../../engine_configurations.h"
#include "../source/event.h"
#include "test_callbacks.h"
#include "test_macros.h"
#include "test_util.h"
#include "testsupport/fileutils.h"
#include "testsupport/metrics/video_metrics.h"
#include "vp8.h" // for external codecs test


using namespace webrtc;

int CodecDataBaseTest::RunTest(CmdArgs& args)
{
    VideoCodingModule* vcm = VideoCodingModule::Create(1);
    CodecDataBaseTest* cdbt = new CodecDataBaseTest(vcm);
    cdbt->Perform(args);
    VideoCodingModule::Destroy(vcm);
    delete cdbt;
    return 0;

}

CodecDataBaseTest::CodecDataBaseTest(VideoCodingModule* vcm):
_vcm(vcm),
_width(0),
_height(0),
_lengthSourceFrame(0),
_timeStamp(0)
{
    //
}
CodecDataBaseTest::~CodecDataBaseTest()
{
    //
}
void
CodecDataBaseTest::Setup(CmdArgs& args)
{
    _inname= args.inputFile;
    _width = args.width;
    _height = args.height;
    _frameRate = args.frameRate;
    _lengthSourceFrame  = 3*_width*_height/2;
    if (args.outputFile.compare(""))
        _outname = test::OutputPath() + "CDBtest_decoded.yuv";
    else
        _outname = args.outputFile;
    _outname = args.outputFile;
    _encodedName = test::OutputPath() + "CDBtest_encoded.vp8";

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

    return;
}



WebRtc_Word32
CodecDataBaseTest::Perform(CmdArgs& args)
{
#ifndef VIDEOCODEC_VP8
    assert(false);
#endif
    Setup(args);
    EventWrapper* waitEvent = EventWrapper::Create();

    /**************************/
    /* General Sanity Checks */
    /************************/
    VideoCodec sendCodec, receiveCodec;
    TEST(VideoCodingModule::NumberOfCodecs() > 0);
    _vcm->InitializeReceiver();
    _vcm->InitializeSender();
    VCMDecodeCompleteCallback *_decodeCallback = new VCMDecodeCompleteCallback(_decodedFile);
    VCMEncodeCompleteCallback *_encodeCompleteCallback = new VCMEncodeCompleteCallback(_encodedFile);
    _vcm->RegisterReceiveCallback(_decodeCallback);
    _vcm->RegisterTransportCallback(_encodeCompleteCallback);
    _encodeCompleteCallback->SetFrameDimensions(_width, _height);
    // registering the callback - encode and decode with the same vcm (could be later changed)
    _encodeCompleteCallback->RegisterReceiverVCM(_vcm);
    // preparing a frame to be encoded
    VideoFrame sourceFrame;
    sourceFrame.VerifyAndAllocate(_lengthSourceFrame);
    WebRtc_UWord8* tmpBuffer = new WebRtc_UWord8[_lengthSourceFrame];
    TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
    sourceFrame.CopyFrame(_lengthSourceFrame, tmpBuffer);
    sourceFrame.SetHeight(_height);
    sourceFrame.SetWidth(_width);
    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);
    // Encoder registration
    TEST (VideoCodingModule::NumberOfCodecs() > 0);
    TEST(VideoCodingModule::Codec(-1, &sendCodec) == VCM_PARAMETER_ERROR);
    TEST(VideoCodingModule::Codec(VideoCodingModule::NumberOfCodecs() + 1, &sendCodec) == VCM_PARAMETER_ERROR);
    VideoCodingModule::Codec(1, &sendCodec);
    sendCodec.plType = 0; // random value
    TEST(_vcm->RegisterSendCodec(&sendCodec, 1, 1440) < 0);
    _vcm->InitializeReceiver();
    _vcm->InitializeSender();
    _vcm->RegisterReceiveCallback(_decodeCallback);
    _vcm->RegisterTransportCallback(_encodeCompleteCallback);
    printf(" \nNumber of Registered Codecs: %d \n\n", VideoCodingModule::NumberOfCodecs());
    printf("Registered codec names: ");
    for (int i=0; i < VideoCodingModule::NumberOfCodecs(); i++)
    {
        VideoCodingModule::Codec(i, &sendCodec);
        printf("%s   ", sendCodec.plName);
    }
    printf("\n\nVerify that all requested codecs are used\n \n \n");

    // Testing with VP8.
    VideoCodingModule::Codec(kVideoCodecVP8, &sendCodec);
    _vcm->RegisterSendCodec(&sendCodec, 1, 1440);
    _encodeCompleteCallback->SetCodecType(kRTPVideoVP8);
    _vcm->InitializeReceiver();
    TEST (_vcm->AddVideoFrame(sourceFrame) == VCM_OK );
    _vcm->InitializeSender();
    TEST (_vcm->AddVideoFrame(sourceFrame) < 0 );

    // Test changing frame size while keeping the same payload type
    VideoCodingModule::Codec(0, &sendCodec);
    sendCodec.width = 352;
    sendCodec.height = 288;
    VideoCodec currentSendCodec;
    _vcm->RegisterSendCodec(&sendCodec, 1, 1440);
    _vcm->SendCodec(&currentSendCodec);
    TEST(currentSendCodec.width == sendCodec.width &&
        currentSendCodec.height == sendCodec.height);
    sendCodec.width = 352/2;
    sendCodec.height = 288/2;
    _vcm->RegisterSendCodec(&sendCodec, 1, 1440);
    _vcm->SendCodec(&currentSendCodec);
    TEST(currentSendCodec.width == sendCodec.width &&
        currentSendCodec.height == sendCodec.height);

    delete _decodeCallback;
    _decodeCallback = NULL;
    delete _encodeCompleteCallback;
    _encodeCompleteCallback = NULL;

    VCMEncodeCompleteCallback *_encodeCallback = new VCMEncodeCompleteCallback(_encodedFile);

    /*************************/
    /* External codecs       */
    /*************************/


    _vcm->InitializeReceiver();
    VP8Decoder* decoder = VP8Decoder::Create();
    VideoCodec vp8DecSettings;
    VideoCodingModule::Codec(kVideoCodecVP8, &vp8DecSettings);
    TEST(_vcm->RegisterExternalDecoder(decoder, vp8DecSettings.plType, false) == VCM_OK);
    TEST(_vcm->RegisterReceiveCodec(&vp8DecSettings, 1, false) == VCM_OK);
    VP8Encoder* encoder = VP8Encoder::Create();
    VideoCodec vp8EncSettings;
    VideoCodingModule::Codec(kVideoCodecVP8, &vp8EncSettings);
    _vcm->RegisterTransportCallback(_encodeCallback); // encode returns error if callback uninitialized
    _encodeCallback->RegisterReceiverVCM(_vcm);
    _encodeCallback->SetCodecType(kRTPVideoVP8);
    TEST(_vcm->RegisterExternalEncoder(encoder, vp8EncSettings.plType) == VCM_OK);
    TEST(_vcm->RegisterSendCodec(&vp8EncSettings, 4, 1440) == VCM_OK);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);
    waitEvent->Wait(33);
    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);

    // De-register and try again.
    TEST(_vcm->RegisterExternalDecoder(NULL, vp8DecSettings.plType, false) == VCM_OK);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() < 0); // Expect an error since we have de-registered the decoder
    TEST(_vcm->RegisterExternalEncoder(NULL, vp8DecSettings.plType) == VCM_OK);
    TEST(_vcm->AddVideoFrame(sourceFrame) < 0); // No send codec registered

    delete decoder;
    decoder = NULL;
    delete encoder;
    encoder = NULL;

    /***************************************
     * Test the "require key frame" setting*
     ***************************************/

    TEST(_vcm->InitializeSender() == VCM_OK);
    TEST(_vcm->InitializeReceiver() == VCM_OK);
    VideoCodingModule::Codec(kVideoCodecVP8, &receiveCodec);
    receiveCodec.height = _height;
    receiveCodec.width = _width;
    TEST(_vcm->RegisterSendCodec(&receiveCodec, 4, 1440) == VCM_OK);
    TEST(_vcm->RegisterReceiveCodec(&receiveCodec, 1, true) == VCM_OK); // Require key frame
    _vcm->RegisterTransportCallback(_encodeCallback); // encode returns error if callback uninitialized
    _encodeCallback->RegisterReceiverVCM(_vcm);
    _encodeCallback->SetCodecType(kRTPVideoVP8);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);
    TEST(_vcm->ResetDecoder() == VCM_OK);
    waitEvent->Wait(33);
    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    // Try to decode a delta frame. Should get a warning since we have enabled the "require key frame" setting
    // and because no frame type request callback has been registered.
    TEST(_vcm->Decode() == VCM_MISSING_CALLBACK);
    TEST(_vcm->IntraFrameRequest() == VCM_OK);
    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);

    // Make sure we can register another codec with the same
    // payload type without crash.
    _vcm->InitializeReceiver();
    sendCodec.width = _width;
    sendCodec.height = _height;
    TEST(_vcm->RegisterReceiveCodec(&sendCodec, 1) == VCM_OK);
    TEST(_vcm->IntraFrameRequest() == VCM_OK);
    waitEvent->Wait(33);
    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);
    TEST(_vcm->RegisterReceiveCodec(&sendCodec, 1) == VCM_OK);
    waitEvent->Wait(33);
    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);
    TEST(_vcm->IntraFrameRequest() == VCM_OK);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);
    TEST(_vcm->ResetDecoder() == VCM_OK);

    delete _encodeCallback;

    /*************************/
    /* Send/Receive Control */
    /***********************/
    /*
    1. check available codecs (N)
    2. register all corresponding decoders
    3. encode 300/N frames with each encoder, and hope to properly decode
    4. encode without a matching decoder - expect an error
    */
    rewind(_sourceFile);
    _vcm->InitializeReceiver();
    _vcm->InitializeSender();
    sourceFrame.Free();
    VCMDecodeCompleteCallback* decodeCallCDT = new VCMDecodeCompleteCallback(_decodedFile);
    VCMEncodeCompleteCallback* encodeCallCDT = new VCMEncodeCompleteCallback(_encodedFile);
    _vcm->RegisterReceiveCallback(decodeCallCDT);
    _vcm->RegisterTransportCallback(encodeCallCDT);
    encodeCallCDT->RegisterReceiverVCM(_vcm);
    if (VideoCodingModule::NumberOfCodecs() > 0)
    {
        // Register all available decoders.
        int i, j;
        //double psnr;
        sourceFrame.VerifyAndAllocate(_lengthSourceFrame);
        _vcm->RegisterReceiveCallback(decodeCallCDT);
        for (i=0; i < VideoCodingModule::NumberOfCodecs(); i++)
        {
            VideoCodingModule::Codec(i, &receiveCodec);
            if (strcmp(receiveCodec.plName, "I420") == 0)
            {
                receiveCodec.height = _height;
                receiveCodec.width = _width;
            }
            _vcm->RegisterReceiveCodec(&receiveCodec, 1);
        }
        // start encoding - iterating over available encoders
        _vcm->RegisterTransportCallback(encodeCallCDT);
        encodeCallCDT->RegisterReceiverVCM(_vcm);
        encodeCallCDT->Initialize();
        int frameCnt = 0;
        for (i=0; i < VideoCodingModule::NumberOfCodecs(); i++)
        {
            encodeCallCDT->ResetByteCount();
            VideoCodingModule::Codec(i, &sendCodec);
            sendCodec.height = _height;
            sendCodec.width = _width;
            sendCodec.startBitrate = 1000;
            sendCodec.maxBitrate = 8000;
            encodeCallCDT->SetFrameDimensions(_width, _height);
            encodeCallCDT->SetCodecType(ConvertCodecType(sendCodec.plName));
            TEST(_vcm->RegisterSendCodec(&sendCodec, 1, 1440) == VCM_OK);

            // We disable the frame dropper to avoid dropping frames due to
            // bad rate control. This isn't a codec performance test, and the
            // I420 codec is expected to produce too many bits.
            _vcm->EnableFrameDropper(false);

            printf("Encoding with %s \n\n", sendCodec.plName);
            for (j=0; j < int(300/VideoCodingModule::NumberOfCodecs()); j++)// assuming 300 frames, NumberOfCodecs <= 10
            {
                frameCnt++;
                TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
                // building source frame
                sourceFrame.CopyFrame(_lengthSourceFrame, tmpBuffer);
                sourceFrame.SetHeight(_height);
                sourceFrame.SetWidth(_width);
                sourceFrame.SetLength(_lengthSourceFrame);
                _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
                sourceFrame.SetTimeStamp(_timeStamp);
                // send frame to the encoder
                TEST (_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
                waitEvent->Wait(33); // was 100

                int ret =_vcm->Decode();
                TEST(ret == 0);
                if (ret < 0)
                {
                    printf("Error #%d in frame number %d \n",ret, frameCnt);
                }
                 // verifying matching payload types:
                _vcm->SendCodec(&sendCodec);
                _vcm->ReceiveCodec(&receiveCodec);
                TEST(sendCodec.plType == receiveCodec.plType);
                if (sendCodec.plType != receiveCodec.plType)
                {
                    printf("frame number:%d\n",frameCnt);
                }
            } // end for:encode-decode
           // byte count for codec specific

            printf("Total bytes encoded: %f \n\n",(8.0/1000)*(encodeCallCDT->EncodedBytes()/((int)10/VideoCodingModule::NumberOfCodecs())));
            // decode what's left in the buffer....
            _vcm->Decode();
            _vcm->Decode();
            // Don't measure PSNR for I420 since it will be perfect.
            if (sendCodec.codecType != kVideoCodecI420) {
                webrtc::test::QualityMetricsResult psnr;
                I420PSNRFromFiles(_inname.c_str(), _outname.c_str(), _width,
                                  _height, &psnr);
                printf("\n @ %d KBPS:  ", sendCodec.startBitrate);
                printf("PSNR from encoder-decoder send-receive control test"
                       "is %f\n\n", psnr.average);
            }
        } // end: iterate codecs
        rewind(_sourceFile);
        sourceFrame.Free();
        delete [] tmpBuffer;
        delete decodeCallCDT;
        delete encodeCallCDT;
        // closing and calculating PSNR for prior encoder-decoder test
        TearDown(); // closing open files
    } // end of #codecs >1

    delete waitEvent;
    Print();
    return 0;
}
void
CodecDataBaseTest::Print()
{
    printf("\nVCM Codec DataBase Test: \n\n%i tests completed\n", vcmMacrosTests);
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
CodecDataBaseTest::TearDown()
{
    fclose(_sourceFile);
    fclose(_decodedFile);
    fclose(_encodedFile);
    return;
}
