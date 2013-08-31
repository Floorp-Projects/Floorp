/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/voe_unit_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <conio.h>
#endif

#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fakes/fake_media_process.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_defines.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

using namespace webrtc;
using namespace test;

namespace voetest {

#define CHECK(expr)                                             \
    if (expr)                                                   \
    {                                                           \
        printf("Error at line: %i, %s \n", __LINE__, #expr);    \
        printf("Error code: %i \n", base->LastError());  \
		PAUSE												    \
        return -1;                                              \
    }

// ----------------------------------------------------------------------------
//                       >>>  R E A D M E  F I R S T <<<
// ----------------------------------------------------------------------------

// 1) The user must ensure that the following codecs are included in VoE:
//
// - L16
// - G.729
// - G.722.1C

// 2) It is also possible to modify the simulation time for each individual test
//
const int dTBetweenEachTest = 4000;

// ----------------------------------------------------------------------------
//                                  Encrypt
// ----------------------------------------------------------------------------

void VoEUnitTest::encrypt(int channel_no, unsigned char * in_data,
                          unsigned char * out_data, int bytes_in,
                          int * bytes_out) {
  int i;

  if (!_extOnOff) {
    // no stereo emulation <=> pure bypass
    for (i = 0; i < bytes_in; i++)
      out_data[i] = in_data[i];
    *bytes_out = bytes_in;
  } else if (_extOnOff && (_extBitsPerSample == 16)) {
    // stereo emulation (sample based, 2 bytes per sample)

    const int nBytesPayload = bytes_in - 12;

    // RTP header (first 12 bytes)
    memcpy(out_data, in_data, 12);

    // skip RTP header
    short* ptrIn = (short*) &in_data[12];
    short* ptrOut = (short*) &out_data[12];

    // network byte order
    for (i = 0; i < nBytesPayload / 2; i++) {
      // produce two output samples for each input sample
      *ptrOut++ = *ptrIn; // left sample
      *ptrOut++ = *ptrIn; // right sample
      ptrIn++;
    }

    *bytes_out = 12 + 2 * nBytesPayload;
  } else if (_extOnOff && (_extBitsPerSample == 8)) {
    // stereo emulation (sample based, 1 bytes per sample)

    const int nBytesPayload = bytes_in - 12;

    // RTP header (first 12 bytes)
    memcpy(out_data, in_data, 12);

    // skip RTP header
    unsigned char* ptrIn = (unsigned char*) &in_data[12];
    unsigned char* ptrOut = (unsigned char*) &out_data[12];

    // network byte order
    for (i = 0; i < nBytesPayload; i++) {
      // produce two output samples for each input sample
      *ptrOut++ = *ptrIn; // left sample
      *ptrOut++ = *ptrIn; // right sample
      ptrIn++;
    }

    *bytes_out = 12 + 2 * nBytesPayload;
  } else if (_extOnOff && (_extBitsPerSample == -1)) {
    // stereo emulation (frame based)

    const int nBytesPayload = bytes_in - 12;

    // RTP header (first 12 bytes)
    memcpy(out_data, in_data, 12);

    // skip RTP header
    unsigned char* ptrIn = (unsigned char*) &in_data[12];
    unsigned char* ptrOut = (unsigned char*) &out_data[12];

    // left channel
    for (i = 0; i < nBytesPayload; i++) {
      *ptrOut++ = *ptrIn++;
    }

    ptrIn = (unsigned char*) &in_data[12];

    // right channel
    for (i = 0; i < nBytesPayload; i++) {
      *ptrOut++ = *ptrIn++;
    }

    *bytes_out = 12 + 2 * nBytesPayload;
  }
}

void VoEUnitTest::decrypt(int channel_no, unsigned char * in_data,
                          unsigned char * out_data, int bytes_in,
                          int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = in_data[i];
  *bytes_out = bytes_in;
}

void VoEUnitTest::encrypt_rtcp(int channel_no, unsigned char * in_data,
                               unsigned char * out_data, int bytes_in,
                               int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = in_data[i];
  *bytes_out = bytes_in;
}

void VoEUnitTest::decrypt_rtcp(int channel_no, unsigned char * in_data,
                               unsigned char * out_data, int bytes_in,
                               int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = in_data[i];
  *bytes_out = bytes_in;
}

void VoEUnitTest::SetStereoExternalEncryption(int channel, bool onOff,
                                              int bitsPerSample) {
  _extOnOff = onOff;
  _extChannel = channel;
  _extBitsPerSample = bitsPerSample;
}

// VoEVEMediaProcess
FakeMediaProcess mpobj;

// ----------------------------------------------------------------------------
//                               VoEUnitTest
// ----------------------------------------------------------------------------

VoEUnitTest::VoEUnitTest(VoETestManager& mgr) :
  _mgr(mgr), _extOnOff(false), _extBitsPerSample(-1), _extChannel(0) {
  for (int i = 0; i < 32; i++) {
    _listening[i] = false;
    _playing[i] = false;
    _sending[i] = false;
  }
}

// ----------------------------------------------------------------------------
//  DoTest
// ----------------------------------------------------------------------------

int VoEUnitTest::DoTest() {
  int test(-1);
  int ret(0);
  while ((test != 0) && (ret != -1)) {
    test = MenuSelection();
    switch (test) {
      case 0:
        // Quit stress test
        break;
      case 1:
        ret = MixerTest();
        break;
      case 2:
        ret = MixerTest();
        break;
      default:
        // Should not be possible
        printf("Invalid selection! (Test code error)\n");
        assert(false);
    } // switch
  } // while

  return ret;
}

// ----------------------------------------------------------------------------
//  MenuSelection
// ----------------------------------------------------------------------------

int VoEUnitTest::MenuSelection() {
  printf("------------------------------------------------\n");
  printf("Select unit test\n\n");
  printf(" (0)  Quit\n");
  printf(" (1)  All\n");
  printf("- - - - - - - - - - - - - - - - - - - - - - - - \n");
  printf(" (2)  Mixer\n");

  const int maxMenuSelection = 2;
  int selection(-1);

  while ((selection < 0) || (selection > maxMenuSelection)) {
    printf("\n: ");
    int retval = scanf("%d", &selection);
    if ((retval != 1) || (selection < 0) || (selection > maxMenuSelection)) {
      printf("Invalid selection!\n");
    }
  }

  return selection;
}

// ----------------------------------------------------------------------------
//  StartMedia
// ----------------------------------------------------------------------------

int VoEUnitTest::StartMedia(int channel, int rtpPort, bool listen, bool playout,
                            bool send, bool fileAsMic, bool localFile) {
  VoEBase* base = _mgr.BasePtr();
  VoEFile* file = _mgr.FilePtr();
  VoENetwork* voe_network = _mgr.NetworkPtr();

  _listening[channel] = false;
  _playing[channel] = false;
  _sending[channel] = false;
  voice_channel_transports_[channel].reset(
      new VoiceChannelTransport(voe_network, channel));

  CHECK(voice_channel_transports_[channel]->SetLocalReceiver(rtpPort));
  CHECK(voice_channel_transports_[channel]->SetSendDestination("127.0.0.1",
                                                               rtpPort));
  if (listen) {
    _listening[channel] = true;
    CHECK(base->StartReceive(channel));
  }
  if (playout) {
    _playing[channel] = true;
    CHECK(base->StartPlayout(channel));
  }
  if (send) {
    _sending[channel] = true;
    CHECK(base->StartSend(channel));
  }
  if (fileAsMic) {
    // play mic as file, mix with microphone to ensure that SWB can be
    //tested as well
    const bool mixWithMic(true);
    CHECK(file->StartPlayingFileAsMicrophone(channel, _mgr.AudioFilename(),
            true, mixWithMic));
  }
  if (localFile) {
    std::string inputFile = webrtc::test::OutputPath() + "audio_short16.pcm";
    CHECK(file->StartPlayingFileLocally(channel,
            inputFile.c_str(),
            false,
            kFileFormatPcm16kHzFile));
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  StopMedia
// ----------------------------------------------------------------------------

int VoEUnitTest::StopMedia(int channel) {
  VoEBase* base = _mgr.BasePtr();
  VoEFile* file = _mgr.FilePtr();

  if (file->IsPlayingFileAsMicrophone(channel)) {
    CHECK(file->StopPlayingFileAsMicrophone(channel));
  }
  if (file->IsPlayingFileLocally(channel)) {
    CHECK(file->StopPlayingFileLocally(channel));
  }
  if (_listening[channel]) {
    _listening[channel] = false;
    CHECK(base->StopReceive(channel));
  }
  if (_playing[channel]) {
    _playing[channel] = false;
    CHECK(base->StopPlayout(channel));
  }
  if (_sending[channel]) {
    _sending[channel] = false;
    CHECK(base->StopSend(channel));
  }

  return 0;
}

void VoEUnitTest::Sleep(unsigned int timeMillisec, bool addMarker) {
  if (addMarker) {
    float dtSec = (float) ((float) timeMillisec / 1000.0);
    printf("[dT=%.1f]", dtSec);
    fflush(NULL);
  }
  webrtc::SleepMs(timeMillisec);
}

void VoEUnitTest::Wait() {
#if defined(_WIN32)
  printf("\npress any key..."); fflush(NULL);
  _getch();
#endif
}

void VoEUnitTest::Test(const char* msg) {
  printf("%s", msg);
  fflush(NULL);
  printf("\n");
  fflush(NULL);
}

int VoEUnitTest::MixerTest() {
  // Set up test parameters first
  //
  const int testTime(dTBetweenEachTest);

  printf("\n\n================================================\n");
  printf(" Mixer Unit Test\n");
  printf("================================================\n\n");

  // Get sub-API pointers
  //
  VoEBase* base = _mgr.BasePtr();
  VoECodec* codec = _mgr.CodecPtr();
  VoEFile* file = _mgr.FilePtr();
  VoEVolumeControl* volume = _mgr.VolumeControlPtr();
  VoEEncryption* encrypt = _mgr.EncryptionPtr();
  VoEDtmf* dtmf = _mgr.DtmfPtr();
  VoEExternalMedia* xmedia = _mgr.ExternalMediaPtr();

  // Set trace
  //
  std::string outputDir = webrtc::test::OutputPath();
  std::string traceFile = outputDir + "UnitTest_Mixer_trace.txt";
  VoiceEngine::SetTraceFile(outputDir.c_str());
  VoiceEngine::SetTraceFilter(kTraceStateInfo | kTraceWarning | kTraceError |
                              kTraceCritical | kTraceApiCall | kTraceMemory |
                              kTraceInfo);

  // Init
  //
  CHECK(base->Init());

  // 8 kHz
  //    CodecInst l16_8 = { 123, "L16", 8000, 160, 1, 128000 };
  CodecInst pcmu_8 = { 0, "pcmu", 8000, 160, 1, 64000 };
  //    CodecInst g729_8 = { 18, "g729", 8000, 160, 1, 8000 };

  // 16 kHz
  CodecInst ipcmwb_16 = { 97, "ipcmwb", 16000, 320, 1, 80000 };
  CodecInst l16_16 = { 124, "L16", 16000, 320, 1, 256000 };

  // 32 kHz
  CodecInst l16_32 = { 125, "L16", 32000, 320, 1, 512000 };
  CodecInst g722_1c_32 = { 126, "G7221", 32000, 640, 1, 32000 };// 20ms@32kHz

  // ------------------------
  // Verify mixing frequency
  // ------------------------

  base->CreateChannel();

  Test(">> Verify correct mixing frequency:\n");

  Test("(ch 0) Sending file at 8kHz <=> mixing at 8kHz...");
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);

  Test("(ch 0) Sending file at 16kHz <=> mixing at 16kHz...");
  CHECK(codec->SetSendCodec(0, ipcmwb_16));
  Sleep(testTime);

  Test("(ch 0) Sending speech at 32kHz <=> mixing at 32Hz...");
  CHECK(codec->SetSendCodec(0, l16_32));
  Sleep(testTime);

  Test("(ch 0) Sending file at 8kHz <=> mixing at 8kHz...");
  CHECK(codec->SetSendCodec(0, pcmu_8));
  Sleep(testTime);

  Test("(ch 0) Playing 16kHz file locally <=> mixing at 16kHz...");
  std::string inputFile = outputDir + "audio_long16.pcm";
  CHECK(file->StartPlayingFileLocally(0, inputFile.c_str(),
          false, kFileFormatPcm16kHzFile));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));

  base->CreateChannel();

  Test("(ch 0) Sending file at 8kHz <=> mixing at 8kHz...");
  CHECK(codec->SetSendCodec(0, pcmu_8));
  Sleep(testTime);

  Test("(ch 0) Sending speech at 32kHz <=> mixing at 32Hz...");
  CHECK(codec->SetSendCodec(0, l16_32));
  Sleep(testTime);

  Test("(ch 1) Playing 16kHz file locally <=> mixing at 32kHz...");
  CHECK(StartMedia(1, 54321, false, true, false, false, true));
  Sleep(testTime);

  CHECK(StopMedia(1));
  CHECK(StopMedia(0));

  base->DeleteChannel(1);
  base->DeleteChannel(0);
  ANL();

  // -------------------------
  // Verify stereo mode mixing
  // -------------------------

  base->CreateChannel();
  base->CreateChannel();

  // SetOutputVolumePan
  //
  // Ensure that all cases sound OK and that the mixer changes state between
  // mono and stereo as it should. A debugger is required to trace the state
  // transitions.

  Test(">> Verify correct mixing in stereo using SetOutputVolumePan():\n");

  Test("(ch 0) Playing 16kHz file locally <=> mixing in mono @ 16kHz...");
  CHECK(StartMedia(0, 12345, false, true, false, false, true));
  Sleep(testTime);
  Test("Panning volume to the left <=> mixing in stereo @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 1.0, 0.0));
  Sleep(testTime);
  Test("Panning volume to the right <=> mixing in stereo @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 0.0, 1.0));
  Sleep(testTime);
  Test("Back to center volume again <=> mixing in mono @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 1.0, 1.0));
  Sleep(testTime);
  Test("(ch 1) Playing 16kHz file locally <=> mixing in mono @ 16kHz...");
  CHECK(StartMedia(1, 54321, false, true, false, false, true));
  Sleep(testTime);
  Test("Panning volume to the left <=> mixing in stereo @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 1.0, 0.0));
  Sleep(testTime);
  Test("Back to center volume again <=> mixing in mono @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 1.0, 1.0));
  Sleep(testTime);
  Test("(ch 1) Stopped playing file <=> mixing in mono @ 16kHz...");
  CHECK(StopMedia(1));
  Sleep(testTime);
  CHECK(StopMedia(0));
  Test("(ch 0) Sending file at 8kHz <=> mixing at 8kHz...");
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);
  Test("(ch 0) Sending speech at 32kHz <=> mixing at 32kHz...");
  CHECK(codec->SetSendCodec(0, l16_32));
  Sleep(testTime);
  Test("Panning volume to the right <=> mixing in stereo @ 32kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 0.0, 1.0));
  Sleep(testTime);
  Test("Back to center volume again <=> mixing in mono @ 32kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 1.0, 1.0));
  Sleep(testTime);
  CHECK(StopMedia(0));
  ANL();

  base->DeleteChannel(0);
  base->DeleteChannel(1);

  // SetChannelOutputVolumePan
  //
  // Ensure that all cases sound OK and that the mixer changes state between
  // mono and stereo as it should. A debugger is required to trace the state
  // transitions.

  base->CreateChannel();
  base->CreateChannel();

  Test(">> Verify correct mixing in stereo using"
    " SetChannelOutputVolumePan():\n");

  Test("(ch 0) Playing 16kHz file locally <=> mixing in mono @ 16kHz...");
  CHECK(StartMedia(0, 12345, false, true, false, false, true));
  Sleep(testTime);
  Test("(ch 0) Panning channel volume to the left <=> mixing in stereo @ "
    "16kHz...");
  CHECK(volume->SetOutputVolumePan(0, 1.0, 0.0));
  Sleep(testTime);
  Test("(ch 0) Panning channel volume to the right <=> mixing in stereo"
    " @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(0, 0.0, 1.0));
  Sleep(testTime);
  Test("(ch 0) Back to center volume again <=> mixing in mono @"
    " 16kHz...");
  CHECK(volume->SetOutputVolumePan(0, 1.0, 1.0));
  Sleep(testTime);
  Test("(ch 1) Playing 16kHz file locally <=> mixing in mono @ 16kHz...");
  CHECK(StartMedia(1, 54321, false, true, false, false, true));
  Sleep(testTime);
  Test("(ch 1) Panning channel volume to the left <=> mixing in stereo "
    "@ 16kHz...");
  CHECK(volume->SetOutputVolumePan(1, 1.0, 0.0));
  Sleep(testTime);
  Test("(ch 1) Back to center volume again <=> mixing in mono @ 16kHz...");
  CHECK(volume->SetOutputVolumePan(1, 1.0, 1.0));
  Sleep(testTime);
  Test("(ch 1) Stopped playing file <=> mixing in mono @ 16kHz...");
  CHECK(StopMedia(1));
  Sleep(testTime);
  CHECK(StopMedia(0));
  ANL();

  base->DeleteChannel(0);
  base->DeleteChannel(1);

  // Emulate stereo-encoding using Encryption
  //
  // Modify the transmitted RTP stream by using external encryption.
  // Supports frame based and sample based "stereo-encoding schemes".

  base->CreateChannel();

  Test(">> Verify correct mixing in stereo using emulated stereo input:\n");

  // enable external encryption
  CHECK(encrypt->RegisterExternalEncryption(0, *this));
  Test("(ch 0) External Encryption is now enabled:");

  Test("(ch 0) Sending file at 8kHz <=> mixing in mono @ 8kHz...");
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);

  // switch to 16kHz (L16) sending codec
  CHECK(codec->SetSendCodec(0, l16_16));
  Test("(ch 0) Sending file at 16kHz (L16) <=> mixing in mono @ 16kHz...");
  Sleep(testTime);

  // register L16 as 2-channel codec on receiving side =>
  // should sound bad since RTP module splits all received packets in half
  // (sample based)
  CHECK(base->StopPlayout(0));
  CHECK(base->StopReceive(0));
  l16_16.channels = 2;
  CHECK(codec->SetRecPayloadType(0, l16_16));
  CHECK(base->StartReceive(0));
  CHECK(base->StartPlayout(0));
  Test("(ch 0) 16kHz L16 is now registered as 2-channel codec on RX side => "
    "should sound bad...");
  Sleep(testTime);

  // emulate sample-based stereo encoding
  Test("(ch 0) Emulate sample-based stereo encoding on sending side => "
    "should sound OK...");
  SetStereoExternalEncryption(0, true, 16);
  Sleep(testTime);
  Test("(ch 0) Stop emulating sample-based stereo encoding on sending side =>"
    " should sound bad...");
  SetStereoExternalEncryption(0, false, 16);
  Sleep(testTime);
  Test("(ch 0) Emulate sample-based stereo encoding on sending side => "
    "should sound OK...");
  SetStereoExternalEncryption(0, true, 16);
  Sleep(testTime);

  // switch to 32kHz (L16) sending codec and disable stereo encoding
  CHECK(codec->SetSendCodec(0, l16_32));
  SetStereoExternalEncryption(0, false, 16);
  Test("(ch 0) Sending file and spech at 32kHz (L16) <=> mixing in mono @ "
    "32kHz...");
  Sleep(testTime);

  // register L16 32kHz as 2-channel codec on receiving side
  CHECK(base->StopPlayout(0));
  CHECK(base->StopReceive(0));
  l16_32.channels = 2;
  CHECK(codec->SetRecPayloadType(0, l16_32));
  CHECK(base->StartReceive(0));
  CHECK(base->StartPlayout(0));
  Test("(ch 0) 32kHz L16 is now registered as 2-channel codec on RX side =>"
    " should sound bad...");
  Sleep(testTime);

  // emulate sample-based stereo encoding
  Test("(ch 0) Emulate sample-based stereo encoding on sending side =>"
    " should sound OK...");
  SetStereoExternalEncryption(0, true, 16);
  Sleep(testTime);

  StopMedia(0);
  l16_32.channels = 1;

  // disable external encryption
  CHECK(encrypt->DeRegisterExternalEncryption(0));
  ANL();

  base->DeleteChannel(0);

  // ------------------
  // Verify put-on-hold
  // ------------------

  base->CreateChannel();
  base->CreateChannel();

  Test(">> Verify put-on-hold functionality:\n");

  Test("(ch 0) Sending at 8kHz...");
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);

  CHECK(base->SetOnHoldStatus(0, true, kHoldPlayOnly));
  Test("(ch 0) Playout is now on hold...");
  Sleep(testTime);
  CHECK(base->SetOnHoldStatus(0, false, kHoldPlayOnly));
  Test("(ch 0) Playout is now enabled again...");
  Sleep(testTime);

  Test("(ch 0) Sending at 16kHz...");
  l16_16.channels = 1;
  CHECK(codec->SetSendCodec(0, l16_16));
  Sleep(testTime);

  CHECK(base->SetOnHoldStatus(0, true, kHoldPlayOnly));
  Test("(ch 0) Playout is now on hold...");
  Sleep(testTime);
  CHECK(base->SetOnHoldStatus(0, false, kHoldPlayOnly));
  Test("(ch 0) Playout is now enabled again...");
  Sleep(testTime);

  Test("(ch 0) Perform minor panning to the left to force mixing in"
    " stereo...");
  CHECK(volume->SetOutputVolumePan(0, (float)1.0, (float)0.7));
  Sleep(testTime);

  CHECK(base->SetOnHoldStatus(0, true, kHoldPlayOnly));
  Test("(ch 0) Playout is now on hold...");
  Sleep(testTime);
  CHECK(base->SetOnHoldStatus(0, false, kHoldPlayOnly));
  Test("(ch 0) Playout is now enabled again...");
  Sleep(testTime);

  Test("(ch 0) Back to center volume again...");
  CHECK(volume->SetOutputVolumePan(0, 1.0, 1.0));
  Sleep(testTime);

  Test("(ch 1) Add 16kHz local file to the mixer...");
  CHECK(StartMedia(1, 54321, false, true, false, false, true));
  Sleep(testTime);

  CHECK(base->SetOnHoldStatus(0, true, kHoldPlayOnly));
  Test("(ch 0) Playout is now on hold...");
  Sleep(testTime);
  CHECK(base->SetOnHoldStatus(1, true, kHoldPlayOnly));
  Test("(ch 1) Playout is now on hold => should be silent...");
  Sleep(testTime);
  CHECK(base->SetOnHoldStatus(0, false, kHoldPlayOnly));
  Test("(ch 0) Playout is now enabled again...");
  CHECK(base->SetOnHoldStatus(1, false, kHoldPlayOnly));
  Test("(ch 1) Playout is now enabled again...");
  Sleep(testTime);
  StopMedia(1);
  Test("(ch 1) Stopped playing file...");
  Sleep(testTime);
  StopMedia(0);
  ANL();

  base->DeleteChannel(0);
  base->DeleteChannel(1);

  // -----------------------------------
  // Verify recording of playout to file
  // -----------------------------------

  // StartRecordingPlayout
  //
  // Verify that the correct set of signals is recorded in the mixer.
  // Record each channel and all channels (-1) to ensure that post and pre
  // mixing recording works.

  base->CreateChannel();
  base->CreateChannel();

  Test(">> Verify file-recording functionality:\n");

  Test("(ch 0) Sending at 8kHz...");
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);

  Test("(ch 0) Recording of playout to 16kHz PCM file...");

  std::string recordedPlayoutFile = webrtc::test::OutputPath() +
        "RecordedPlayout16kHz.pcm";
  CHECK(file->StartRecordingPlayout(
          0, recordedPlayoutFile.c_str(), NULL));
  Sleep(testTime);
  CHECK(file->StopRecordingPlayout(0));

  Test("(ch 0) Playing out the recorded file...");
  CHECK(volume->SetInputMute(0, true));
  CHECK(file->StartPlayingFileLocally(
          0, recordedPlayoutFile.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));
  CHECK(volume->SetInputMute(0, false));

  CHECK(codec->SetSendCodec(0, l16_16));
  Test("(ch 0) Sending at 16kHz (L16)...");
  Sleep(testTime);

  Test("(ch 0) Recording of playout to 16kHz PCM file...");
  CHECK(file->StartRecordingPlayout(
          0, recordedPlayoutFile.c_str(), NULL));
  Sleep(testTime);
  CHECK(file->StopRecordingPlayout(0));

  Test("(ch 0) Playing out the recorded file...");
  CHECK(volume->SetInputMute(0, true));
  CHECK(file->StartPlayingFileLocally(
          0, recordedPlayoutFile.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));
  CHECK(volume->SetInputMute(0, false));

  CHECK(codec->SetSendCodec(0, l16_32));
  Test("(ch 0) Sending at 32kHz (L16)...");
  Sleep(testTime);

  Test("(ch 0) Recording of playout to 16kHz PCM file...");
  CHECK(file->StartRecordingPlayout(
          0, recordedPlayoutFile.c_str(), NULL));
  Sleep(testTime);
  CHECK(file->StopRecordingPlayout(0));

  Test("(ch 0) Playing out the recorded file...");
  CHECK(volume->SetInputMute(0, true));
  CHECK(file->StartPlayingFileLocally(
          0, recordedPlayoutFile.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));
  CHECK(volume->SetInputMute(0, false));

  Test("(ch 0) Sending at 16kHz without file as mic but file added on the"
    " playout side instead...");
  CHECK(StopMedia(0));
  CHECK(StartMedia(0, 12345, false, true, false, false, true));
  CHECK(codec->SetSendCodec(0, l16_16));
  Sleep(testTime);

  Test("(ch 0) Recording of playout to 16kHz PCM file...");
  CHECK(file->StartRecordingPlayout(
          0, recordedPlayoutFile.c_str(), NULL));
  Sleep(testTime);
  CHECK(file->StopRecordingPlayout(0));
  CHECK(file->StopPlayingFileLocally(0));

  Test("(ch 0) Playing out the recorded file...");
  CHECK(file->StartPlayingFileLocally(
          0, recordedPlayoutFile.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));

  CHECK(StopMedia(0));
  CHECK(StopMedia(1));

  Test("(ch 0) Sending at 16kHz...");
  CHECK(StartMedia(0, 12345, true, true, true, false, false));
  CHECK(codec->SetSendCodec(0, l16_16));
  Test("(ch 1) Adding playout file...");
  CHECK(StartMedia(1, 33333, false, true, false, false, true));
  Sleep(testTime);

  Test("(ch -1) Speak while recording all channels to add mixer input on "
    "channel 0...");
  CHECK(file->StartRecordingPlayout(
          -1, recordedPlayoutFile.c_str(), NULL));
  Sleep(testTime);
  CHECK(file->StopRecordingPlayout(-1));
  CHECK(file->StopPlayingFileLocally(1));

  Test("(ch 0) Playing out the recorded file...");
  CHECK(volume->SetInputMute(0, true));
  CHECK(file->StartPlayingFileLocally(
          0, recordedPlayoutFile.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));
  CHECK(volume->SetInputMute(0, false));

  CHECK(StopMedia(0));
  CHECK(StopMedia(1));
  ANL();

  // StartRecordingPlayoutStereo

  Test(">> Verify recording of playout in stereo:\n");

  Test("(ch 0) Sending at 32kHz...");
  CHECK(codec->SetSendCodec(0, l16_16));
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);

  Test("Modified master balance (L=10%%, R=100%%) to force stereo mixing...");
  CHECK(volume->SetOutputVolumePan(-1, (float)0.1, (float)1.0));
  Sleep(testTime);

  /*
   Test("Recording of left and right channel playout to two 16kHz PCM "
   "files...");
   file->StartRecordingPlayoutStereo(
   GetFilename("RecordedPlayout_Left_16kHz.pcm"),
   GetFilename("RecordedPlayout_Right_16kHz.pcm"), StereoBoth);
   Sleep(testTime);
   Test("Back to center volume again...");
   CHECK(volume->SetOutputVolumePan(-1, (float)1.0, (float)1.0));
   */

  Test("(ch 0) Playing out the recorded file for the left channel (10%%)...");
  CHECK(volume->SetInputMute(0, true));
  std::string leftFilename = outputDir + "RecordedPlayout_Left_16kHz.pcm";
  CHECK(file->StartPlayingFileLocally(0, leftFilename.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));

  Test("(ch 0) Playing out the recorded file for the right channel (100%%) =>"
    " should sound louder than the left channel...");
  std::string rightFilename = outputDir + "RecordedPlayout_Right_16kHz.pcm";
  CHECK(file->StartPlayingFileLocally(0, rightFilename.c_str()));
  Sleep(testTime);
  CHECK(file->StopPlayingFileLocally(0));
  CHECK(volume->SetInputMute(0, false));

  base->DeleteChannel(0);
  base->DeleteChannel(1);
  ANL();

  // ---------------------------
  // Verify inserted Dtmf tones
  // ---------------------------

  Test(">> Verify Dtmf feedback functionality:\n");

  base->CreateChannel();

  for (int i = 0; i < 2; i++) {
    if (i == 0)
      Test("Dtmf direct feedback is now enabled...");
    else
      Test("Dtmf direct feedback is now disabled...");

    CHECK(dtmf->SetDtmfFeedbackStatus(true, (i==0)));

    Test("(ch 0) Sending at 32kHz using G.722.1C...");
    CHECK(codec->SetRecPayloadType(0, g722_1c_32));
    CHECK(codec->SetSendCodec(0, g722_1c_32));
    CHECK(StartMedia(0, 12345, true, true, true, false, false));
    Sleep(500);

    Test("(ch 0) Sending outband Dtmf events => ensure that they are added"
      " to the mixer...");
    // ensure that receiver will not play out outband Dtmf
    CHECK(dtmf->SetSendTelephoneEventPayloadType(0, 118));
    CHECK(dtmf->SendTelephoneEvent(0, 9, true, 390));
    Sleep(500);
    CHECK(dtmf->SendTelephoneEvent(0, 1, true, 390));
    Sleep(500);
    CHECK(dtmf->SendTelephoneEvent(0, 5, true, 390));
    Sleep(500);
    Sleep(testTime - 1500);

    Test("(ch 0) Changing codec to 8kHz PCMU...");
    CHECK(codec->SetSendCodec(0, pcmu_8));
    Sleep(500);

    Test("(ch 0) Sending outband Dtmf events => ensure that they are added"
      " to the mixer...");
    CHECK(dtmf->SendTelephoneEvent(0, 9, true, 390));
    Sleep(500);
    CHECK(dtmf->SendTelephoneEvent(0, 1, true, 390));
    Sleep(500);
    CHECK(dtmf->SendTelephoneEvent(0, 5, true, 390));
    Sleep(500);
    Sleep(testTime - 1500);

    Test("(ch 0) Changing codec to 16kHz L16...");
    CHECK(codec->SetSendCodec(0, l16_16));
    Sleep(500);

    Test("(ch 0) Sending outband Dtmf events => ensure that they are added"
      " to the mixer...");
    CHECK(dtmf->SendTelephoneEvent(0, 9, true, 390));
    Sleep(500);
    CHECK(dtmf->SendTelephoneEvent(0, 1, true, 390));
    Sleep(500);
    CHECK(dtmf->SendTelephoneEvent(0, 5, true, 390));
    Sleep(500);
    Sleep(testTime - 1500);

    StopMedia(0);
    ANL();
  }

  base->DeleteChannel(0);

  // ---------------------------
  // Verify external processing
  // --------------------------

  base->CreateChannel();

  Test(">> Verify external media processing:\n");

  Test("(ch 0) Playing 16kHz file locally <=> mixing in mono @ 16kHz...");
  CHECK(StartMedia(0, 12345, false, true, false, false, true));
  Sleep(testTime);
  Test("Enabling playout external media processing => played audio should "
    "now be affected");
  CHECK(xmedia->RegisterExternalMediaProcessing(
          0, kPlaybackAllChannelsMixed, mpobj));
  Sleep(testTime);
  Test("(ch 0) Sending speech at 32kHz <=> mixing at 32kHz...");
  CHECK(codec->SetSendCodec(0, l16_32));
  Sleep(testTime);
  printf("Back to normal again\n");
  CHECK(xmedia->DeRegisterExternalMediaProcessing(0,
          kPlaybackAllChannelsMixed));
  Sleep(testTime);
  printf("Enabling playout external media processing on ch 0 => "
    "played audio should now be affected\n");
  CHECK(xmedia->RegisterExternalMediaProcessing(0, kPlaybackPerChannel,
          mpobj));
  Sleep(testTime);
  Test("Panning volume to the right <=> mixing in stereo @ 32kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 0.0, 1.0));
  Sleep(testTime);
  Test("Back to center volume again <=> mixing in mono @ 32kHz...");
  CHECK(volume->SetOutputVolumePan(-1, 1.0, 1.0));
  Sleep(testTime);
  printf("Back to normal again\n");
  CHECK(xmedia->DeRegisterExternalMediaProcessing(0, kPlaybackPerChannel));
  Sleep(testTime);
  CHECK(StopMedia(0));
  ANL();

  base->DeleteChannel(0);

  // --------------------------------------------------
  // Extended tests of emulated stereo encoding schemes
  // --------------------------------------------------

  CodecInst PCMU;
  CodecInst G729;
  CodecInst L16_8;
  CodecInst L16_16;
  CodecInst L16_32;

  base->CreateChannel();

  Test(">> Verify emulated stereo encoding for differenct codecs:\n");

  // enable external encryption
  CHECK(encrypt->RegisterExternalEncryption(0, *this));
  Test("(ch 0) External Encryption is now enabled:");

  // register all codecs on the receiving side
  strcpy(PCMU.plname, "PCMU");
  PCMU.channels = 2;
  PCMU.pacsize = 160;
  PCMU.plfreq = 8000;
  PCMU.pltype = 125;
  PCMU.rate = 64000;
  CHECK(codec->SetRecPayloadType(0, PCMU));

  strcpy(G729.plname, "G729");
  G729.channels = 2;
  G729.pacsize = 160;
  G729.plfreq = 8000;
  G729.pltype = 18;
  G729.rate = 8000;
  CHECK(codec->SetRecPayloadType(0, G729));

  strcpy(L16_8.plname, "L16");
  L16_8.channels = 2;
  L16_8.pacsize = 160;
  L16_8.plfreq = 8000;
  L16_8.pltype = 120;
  L16_8.rate = 128000;
  CHECK(codec->SetRecPayloadType(0, L16_8));

  strcpy(L16_16.plname, "L16");
  L16_16.channels = 2;
  L16_16.pacsize = 320;
  L16_16.plfreq = 16000;
  L16_16.pltype = 121;
  L16_16.rate = 256000;
  CHECK(codec->SetRecPayloadType(0, L16_16));

  // NOTE - we cannot send larger than 1500 bytes per RTP packet
  strcpy(L16_32.plname, "L16");
  L16_32.channels = 2;
  L16_32.pacsize = 320;
  L16_32.plfreq = 32000;
  L16_32.pltype = 122;
  L16_32.rate = 512000;
  CHECK(codec->SetRecPayloadType(0, L16_32));

  // sample-based, 8-bits per sample

  Test("(ch 0) Sending using G.711 (sample based, 8 bits/sample)...");
  PCMU.channels = 1;
  CHECK(codec->SetSendCodec(0, PCMU));
  SetStereoExternalEncryption(0, true, 8);
  CHECK(StartMedia(0, 12345, true, true, true, true, false));
  Sleep(testTime);

  // sample-based, 16-bits per sample

  Test("(ch 0) Sending using L16 8kHz (sample based, 16 bits/sample)...");
  L16_8.channels = 1;
  CHECK(codec->SetSendCodec(0, L16_8));
  SetStereoExternalEncryption(0, true, 16);
  Sleep(testTime);

  Test("(ch 0) Sending using L16 16kHz (sample based, 16 bits/sample)...");
  L16_16.channels = 1;
  CHECK(codec->SetSendCodec(0, L16_16));
  Sleep(testTime);

  Test("(ch 0) Sending using L16 32kHz (sample based, 16 bits/sample)...");
  L16_32.channels = 1;
  CHECK(codec->SetSendCodec(0, L16_32));
  Sleep(testTime);

  Test("(ch 0) Sending using G.729 (frame based)...");
  G729.channels = 1;
  CHECK(codec->SetSendCodec(0, G729));
  Sleep(testTime);

  StopMedia(0);

  // disable external encryption
  CHECK(encrypt->DeRegisterExternalEncryption(0));

  base->DeleteChannel(0);

  // ------------------------------------------------------------------------
  CHECK(base->Terminate());

  printf("\n\n------------------------------------------------\n");
  printf(" Test passed!\n");
  printf("------------------------------------------------\n\n");

  return 0;
}

} // namespace voetest
