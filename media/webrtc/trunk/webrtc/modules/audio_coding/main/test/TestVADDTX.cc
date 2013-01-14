/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestVADDTX.h"

#include <iostream>

#include "audio_coding_module_typedefs.h"
#include "common_types.h"
#include "engine_configurations.h"
#include "testsupport/fileutils.h"
#include "trace.h"
#include "utility.h"

namespace webrtc {

TestVADDTX::TestVADDTX(int testMode):
_acmA(NULL),
_acmB(NULL),
_channelA2B(NULL),
_testResults(0)
{
    //testMode == 1 for more extensive testing
    //testMode == 0 for quick test (autotest)
   _testMode = testMode;
}

TestVADDTX::~TestVADDTX()
{
    if(_acmA != NULL)
    {
        AudioCodingModule::Destroy(_acmA);
        _acmA = NULL;
    }
    if(_acmB != NULL)
    {
        AudioCodingModule::Destroy(_acmB);
        _acmB = NULL;
    }
    if(_channelA2B != NULL)
    {
        delete _channelA2B;
        _channelA2B = NULL;
    }
}

void TestVADDTX::Perform()
{
    if(_testMode == 0)
    {
        printf("Running VAD/DTX Test");
        WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceAudioCoding, -1,
                     "---------- TestVADDTX ----------");
    }

    const std::string file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    _inFileA.Open(file_name, 32000, "rb");

    _acmA = AudioCodingModule::Create(0);
    _acmB = AudioCodingModule::Create(1);

    _acmA->InitializeReceiver();
    _acmB->InitializeReceiver();

    WebRtc_UWord8 numEncoders = _acmA->NumberOfCodecs();
    CodecInst myCodecParam;
    if(_testMode != 0)
    {
        printf("Registering codecs at receiver... \n");
    }
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++)
    {
        _acmB->Codec(n, myCodecParam);
        if(_testMode != 0)
        {
            printf("%s\n", myCodecParam.plname);
        }
        if (!strcmp(myCodecParam.plname, "opus")) {
          // Use mono decoding for Opus in the VAD/DTX test.
          myCodecParam.channels = 1;
        }
        _acmB->RegisterReceiveCodec(myCodecParam);
    }

    // Create and connect the channel
    _channelA2B = new Channel;
    _acmA->RegisterTransportCallback(_channelA2B);
    _channelA2B->RegisterReceiverACM(_acmB);

    _acmA->RegisterVADCallback(&_monitor);


    WebRtc_Word16 testCntr = 1;
    WebRtc_Word16 testResults = 0;

#ifdef WEBRTC_CODEC_ISAC
    // Open outputfile
    OpenOutFile(testCntr++);

    // Register iSAC WB as send codec
    char nameISAC[] = "ISAC";
    RegisterSendCodec('A', nameISAC, 16000);

    // Run the five test cased
    runTestCases();

    // Close file
    _outFileB.Close();

   // Open outputfile
    OpenOutFile(testCntr++);

    // Register iSAC SWB as send codec
    RegisterSendCodec('A', nameISAC, 32000);

    // Run the five test cased
    runTestCases();

    // Close file
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_ILBC
    // Open outputfile
    OpenOutFile(testCntr++);

    // Register iLBC as send codec
    char nameILBC[] = "ilbc";
    RegisterSendCodec('A', nameILBC);

    // Run the five test cased
    runTestCases();

    // Close file
    _outFileB.Close();

#endif
#ifdef WEBRTC_CODEC_OPUS
    // Open outputfile
    OpenOutFile(testCntr++);

    // Register Opus as send codec
    char nameOPUS[] = "opus";
    RegisterSendCodec('A', nameOPUS);

    // Run the five test cased
    runTestCases();

    // Close file
    _outFileB.Close();

#endif
    if(_testMode) {
        printf("Done!\n");
    }

    printf("VAD/DTX test completed with %d subtests failed\n", testResults);
    if (testResults > 0)
    {
        printf("Press return\n\n");
        getchar();
    }
}

void TestVADDTX::runTestCases()
{
    if(_testMode != 0)
    {
        CodecInst myCodecParam;
        _acmA->SendCodec(myCodecParam);
        printf("%s\n", myCodecParam.plname);
    }
    else
    {
        printf(".");
    }
    // #1 DTX = OFF, VAD = ON, VADNormal
    if(_testMode != 0)
        printf("Test #1 ");
    SetVAD(false, true, VADNormal);
    Run();
    _testResults += VerifyTest();

    // #2 DTX = OFF, VAD = ON, VADAggr
    if(_testMode != 0)
        printf("Test #2 ");
    SetVAD(false, true, VADAggr);
    Run();
    _testResults += VerifyTest();

    // #3 DTX = ON, VAD = ON, VADLowBitrate
    if(_testMode != 0)
        printf("Test #3 ");
    SetVAD(true, true, VADLowBitrate);
    Run();
    _testResults += VerifyTest();

    // #4 DTX = ON, VAD = ON, VADVeryAggr
    if(_testMode != 0)
        printf("Test #4 ");
    SetVAD(true, true, VADVeryAggr);
    Run();
    _testResults += VerifyTest();

    // #5 DTX = ON, VAD = OFF, VADNormal
    if(_testMode != 0)
        printf("Test #5 ");
    SetVAD(true, false, VADNormal);
    Run();
    _testResults += VerifyTest();

}
void TestVADDTX::runTestInternalDTX()
{
    // #6 DTX = ON, VAD = ON, VADNormal
    if(_testMode != 0)
        printf("Test #6 ");

    SetVAD(true, true, VADNormal);
    if(_acmA->ReplaceInternalDTXWithWebRtc(true) < 0) {
        printf("Was not able to replace DTX since CN was not registered\n");
     }
    Run();
    _testResults += VerifyTest();
}

void TestVADDTX::SetVAD(bool statusDTX, bool statusVAD, WebRtc_Word16 vadMode)
{
    bool dtxEnabled, vadEnabled;
    ACMVADMode vadModeSet;

    if (_acmA->SetVAD(statusDTX, statusVAD, (ACMVADMode) vadMode) < 0) {
      assert(false);
    }
    if (_acmA->VAD(dtxEnabled, vadEnabled, vadModeSet) < 0) {
      assert(false);
    }

    if(_testMode != 0)
    {
        if(statusDTX != dtxEnabled)
        {
            printf("DTX: %s not the same as requested: %s\n",
            dtxEnabled? "ON":"OFF", dtxEnabled? "OFF":"ON");
        }
        if(((statusVAD == true) && (vadEnabled == false)) ||
           ((statusVAD == false) && (vadEnabled == false) &&
               (statusDTX == true)))
        {
            printf("VAD: %s not the same as requested: %s\n",
            vadEnabled? "ON":"OFF", vadEnabled? "OFF":"ON");
        }
        if(vadModeSet != vadMode)
        {
            printf("VAD mode: %d not the same as requested: %d\n",
            (WebRtc_Word16)vadModeSet, (WebRtc_Word16)vadMode);
        }
    }

    // Requested VAD/DTX settings
    _setStruct.statusDTX = statusDTX;
    _setStruct.statusVAD = statusVAD;
    _setStruct.vadMode = (ACMVADMode) vadMode;

    // VAD settings after setting VAD in ACM
    _getStruct.statusDTX = dtxEnabled;
    _getStruct.statusVAD = vadEnabled;
    _getStruct.vadMode = vadModeSet;

}

VADDTXstruct TestVADDTX::GetVAD()
{
    VADDTXstruct retStruct;
    bool dtxEnabled, vadEnabled;
    ACMVADMode vadModeSet;

    if (_acmA->VAD(dtxEnabled, vadEnabled, vadModeSet) < 0) {
      assert(false);
    }

    retStruct.statusDTX = dtxEnabled;
    retStruct.statusVAD = vadEnabled;
    retStruct.vadMode = vadModeSet;
    return retStruct;
}

WebRtc_Word16 TestVADDTX::RegisterSendCodec(char side,
                                          char* codecName,
                                          WebRtc_Word32 samplingFreqHz,
                                          WebRtc_Word32 rateKbps)
{
    if(_testMode != 0)
    {
        printf("Registering %s for side %c\n", codecName, side);
    }
    std::cout << std::flush;
    AudioCodingModule* myACM;
    switch(side)
    {
    case 'A':
        {
            myACM = _acmA;
            break;
        }
    case 'B':
        {
            myACM = _acmB;
            break;
        }
    default:
        return -1;
    }

    if(myACM == NULL)
    {
        return -1;
    }

    CodecInst myCodecParam;
    for(WebRtc_Word16 codecCntr = 0; codecCntr < myACM->NumberOfCodecs();
        codecCntr++)
    {
        CHECK_ERROR(myACM->Codec((WebRtc_UWord8)codecCntr, myCodecParam));
        if(!STR_CASE_CMP(myCodecParam.plname, codecName))
        {
            if((samplingFreqHz == -1) || (myCodecParam.plfreq == samplingFreqHz))
            {
                if((rateKbps == -1) || (myCodecParam.rate == rateKbps))
                {
                    break;
                }
            }
        }
    }

    // We only allow VAD/DTX when sending mono.
    myCodecParam.channels = 1;
    CHECK_ERROR(myACM->RegisterSendCodec(myCodecParam));

    // initialization was succesful
    return 0;
}

void TestVADDTX::Run()
{
    AudioFrame audioFrame;

    WebRtc_UWord16 SamplesIn10MsecA = _inFileA.PayloadLength10Ms();
    WebRtc_UWord32 timestampA = 1;
    WebRtc_Word32 outFreqHzB = _outFileB.SamplingFrequency();

    while(!_inFileA.EndOfFile())
    {
        _inFileA.Read10MsData(audioFrame);
        audioFrame.timestamp_ = timestampA;
        timestampA += SamplesIn10MsecA;
        CHECK_ERROR(_acmA->Add10MsData(audioFrame));

        CHECK_ERROR(_acmA->Process());

        CHECK_ERROR(_acmB->PlayoutData10Ms(outFreqHzB, audioFrame));
        _outFileB.Write10MsData(audioFrame.data_, audioFrame.samples_per_channel_);
    }
#ifdef PRINT_STAT
    _monitor.PrintStatistics(_testMode);
#endif
    _inFileA.Rewind();
    _monitor.GetStatistics(_statCounter);
    _monitor.ResetStatistics();
}

void TestVADDTX::OpenOutFile(WebRtc_Word16 test_number) {
  std::string file_name;
  std::stringstream file_stream;
  file_stream << webrtc::test::OutputPath();
  if (_testMode == 0) {
    file_stream << "testVADDTX_autoFile_";
  } else {
    file_stream << "testVADDTX_outFile_";
  }
  file_stream << test_number << ".pcm";
  file_name = file_stream.str();
  _outFileB.Open(file_name, 16000, "wb");
}


WebRtc_Word16 TestVADDTX::VerifyTest()
{
    // Verify empty frame result
    WebRtc_UWord8 statusEF = 0;
    WebRtc_UWord8 vadPattern = 0;
    WebRtc_UWord8 emptyFramePattern[6];
    CodecInst myCodecParam;
    _acmA->SendCodec(myCodecParam);
    bool dtxInUse = true;
    bool isReplaced = false;
    if ((STR_CASE_CMP(myCodecParam.plname,"G729") == 0) ||
        (STR_CASE_CMP(myCodecParam.plname,"G723") == 0) ||
        (STR_CASE_CMP(myCodecParam.plname,"AMR") == 0) ||
        (STR_CASE_CMP(myCodecParam.plname,"AMR-wb") == 0) ||
        (STR_CASE_CMP(myCodecParam.plname,"speex") == 0))
    {
        _acmA->IsInternalDTXReplacedWithWebRtc(isReplaced);
        if (!isReplaced)
        {
            dtxInUse = false;
        }
    }

    // Check for error in VAD/DTX settings
    if (_getStruct.statusDTX != _setStruct.statusDTX){
        // DTX status doesn't match expected
        vadPattern |= 4;
    }
    if (_getStruct.statusDTX){
        if ((!_getStruct.statusVAD && dtxInUse) || (!dtxInUse && (_getStruct.statusVAD !=_setStruct.statusVAD)))
        {
            // Missmatch in VAD setting
            vadPattern |= 2;
        }
    } else {
        if (_getStruct.statusVAD != _setStruct.statusVAD){
            // VAD status doesn't match expected
            vadPattern |= 2;
        }
    }
    if (_getStruct.vadMode != _setStruct.vadMode){
        // VAD Mode doesn't match expected
        vadPattern |= 1;
    }

    // Set expected empty frame pattern
    int ii;
    for (ii = 0; ii < 6; ii++) {
        emptyFramePattern[ii] = 0;
    }
    emptyFramePattern[0] = 1; // "kNoEncoding", not important to check. Codecs with packetsize != 80 samples will get this output.
    emptyFramePattern[1] = 1; // Expect to always receive some frames labeled "kActiveNormalEncoded"
    emptyFramePattern[2] = (((!_getStruct.statusDTX && _getStruct.statusVAD) || (!dtxInUse && _getStruct.statusDTX))); // "kPassiveNormalEncoded"
    emptyFramePattern[3] = ((_getStruct.statusDTX && dtxInUse && (_acmA->SendFrequency() == 8000))); // "kPassiveDTXNB"
    emptyFramePattern[4] = ((_getStruct.statusDTX && dtxInUse && (_acmA->SendFrequency() == 16000))); // "kPassiveDTXWB"
    emptyFramePattern[5] = ((_getStruct.statusDTX && dtxInUse && (_acmA->SendFrequency() == 32000))); // "kPassiveDTXSWB"

    // Check pattern 1-5 (skip 0)
    for (int ii = 1; ii < 6; ii++)
    {
        if (emptyFramePattern[ii])
        {
            statusEF |= (_statCounter[ii] == 0);
        }
        else
        {
            statusEF |= (_statCounter[ii] > 0);
        }
    }
    if ((statusEF == 0) && (vadPattern == 0))
    {
        if(_testMode != 0)
        {
            printf(" Test OK!\n");
        }
        return 0;
    }
    else
    {
        if (statusEF)
        {
            printf("\t\t\tUnexpected empty frame result!\n");
        }
        if (vadPattern)
        {
            printf("\t\t\tUnexpected SetVAD() result!\tDTX: %d\tVAD: %d\tMode: %d\n", (vadPattern >> 2) & 1, (vadPattern >> 1) & 1, vadPattern & 1);
        }
        return 1;
    }
}

ActivityMonitor::ActivityMonitor()
{
    _counter[0] = _counter[1] = _counter[2] = _counter[3] = _counter[4] = _counter[5] = 0;
}

ActivityMonitor::~ActivityMonitor()
{
}

WebRtc_Word32 ActivityMonitor::InFrameType(WebRtc_Word16 frameType)
{
    _counter[frameType]++;
    return 0;
}

void ActivityMonitor::PrintStatistics(int testMode)
{
    if(testMode != 0)
    {
        printf("\n");
        printf("kActiveNormalEncoded  kPassiveNormalEncoded  kPassiveDTXWB  kPassiveDTXNB kPassiveDTXSWB kFrameEmpty\n");

        printf("%19u", _counter[1]);
        printf("%22u", _counter[2]);
        printf("%14u", _counter[3]);
        printf("%14u", _counter[4]);
        printf("%14u", _counter[5]);
        printf("%11u", _counter[0]);

        printf("\n\n");
    }
}

void ActivityMonitor::ResetStatistics()
{
    _counter[0] = _counter[1] = _counter[2] = _counter[3] = _counter[4] = _counter[5] = 0;
}

void ActivityMonitor::GetStatistics(WebRtc_UWord32* getCounter)
{
    for (int ii = 0; ii < 6; ii++)
    {
        getCounter[ii] = _counter[ii];
    }
}

} // namespace webrtc
