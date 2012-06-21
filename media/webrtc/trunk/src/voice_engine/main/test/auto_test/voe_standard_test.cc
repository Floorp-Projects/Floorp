/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "engine_configurations.h"
#if defined(_WIN32)
#include <conio.h>     // exists only on windows
#include <tchar.h>
#endif

#include "voe_standard_test.h"

#if defined (_ENABLE_VISUAL_LEAK_DETECTOR_) && defined(_DEBUG) && \
    defined(_WIN32) && !defined(_INSTRUMENTATION_TESTING_)
#include "vld.h"
#endif

#ifdef MAC_IPHONE
#include "../../source/voice_engine_defines.h"  // defines build macros
#else
#include "../../source/voice_engine_defines.h"  // defines build macros
#endif

#include "automated_mode.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"

#ifdef _TEST_NETEQ_STATS_
#include "../../interface/voe_neteq_stats.h" // Not available in delivery folder
#endif

#include "voe_extended_test.h"
#include "voe_stress_test.h"
#include "voe_unit_test.h"
#include "voe_cpu_test.h"

using namespace webrtc;

namespace voetest {

#ifdef MAC_IPHONE
// Defined in iPhone specific test file
int GetDocumentsDir(char* buf, int bufLen);
char* GetFilename(char* filename);
const char* GetFilename(const char* filename);
int GetResource(char* resource, char* dest, int destLen);
char* GetResource(char* resource);
const char* GetResource(const char* resource);
// #ifdef MAC_IPHONE
#elif defined(WEBRTC_ANDROID)
char filenameStr[2][256];
int currentStr = 0;

char* GetFilename(char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", filename);
  return filenameStr[currentStr];
}

const char* GetFilename(const char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", filename);
  return filenameStr[currentStr];
}

int GetResource(char* resource, char* dest, int destLen) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
  strncpy(dest, filenameStr[currentStr], destLen-1);
  return 0;
}

char* GetResource(char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
  return filenameStr[currentStr];
}

const char* GetResource(const char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
  return filenameStr[currentStr];
}

#else
char filenameStr[2][256];
int currentStr = 0;

char* GetFilename(char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", filename);
  return filenameStr[currentStr];
}
const char* GetFilename(const char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", filename);
  return filenameStr[currentStr];
}
int GetResource(char* resource, char* dest, int destLen) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", resource);
  strncpy(dest, filenameStr[currentStr], destLen - 1);
  return 0;
}
char* GetResource(char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", resource);
  return filenameStr[currentStr];
}
const char* GetResource(const char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", resource);
  return filenameStr[currentStr];
}
#endif

#if !defined(MAC_IPHONE)
const char* summaryFilename = "/tmp/VoiceEngineSummary.txt";
#endif
// For iPhone the summary filename is created in createSummary

int dummy = 0;  // Dummy used in different functions to avoid warnings

TestRtpObserver::TestRtpObserver() {
  Reset();
}

TestRtpObserver::~TestRtpObserver() {
}

void TestRtpObserver::Reset() {
  for (int i = 0; i < 2; i++) {
    ssrc_[i] = 0;
    csrc_[i][0] = 0;
    csrc_[i][1] = 0;
    added_[i][0] = false;
    added_[i][1] = false;
    size_[i] = 0;
  }
}

void TestRtpObserver::OnIncomingCSRCChanged(const int channel,
                                            const unsigned int CSRC,
                                            const bool added) {
  char msg[128];
  sprintf(msg, "=> OnIncomingCSRCChanged(channel=%d, CSRC=%u, added=%d)\n",
          channel, CSRC, added);
  TEST_LOG("%s", msg);

  if (channel > 1)
    return;  // Not enough memory.

  csrc_[channel][size_[channel]] = CSRC;
  added_[channel][size_[channel]] = added;

  size_[channel]++;
  if (size_[channel] == 2)
    size_[channel] = 0;
}

void TestRtpObserver::OnIncomingSSRCChanged(const int channel,
                                            const unsigned int SSRC) {
  char msg[128];
  sprintf(msg, "\n=> OnIncomingSSRCChanged(channel=%d, SSRC=%u)\n", channel,
          SSRC);
  TEST_LOG("%s", msg);

  ssrc_[channel] = SSRC;
}

void MyDeadOrAlive::OnPeriodicDeadOrAlive(const int /*channel*/,
                                          const bool alive) {
  if (alive) {
    TEST_LOG("ALIVE\n");
  } else {
    TEST_LOG("DEAD\n");
  }
  fflush(NULL);
}

FakeExternalTransport::FakeExternalTransport(VoENetwork* ptr)
    : my_network_(ptr),
      thread_(NULL),
      lock_(NULL),
      event_(NULL),
      length_(0),
      channel_(0),
      delay_is_enabled_(0),
      delay_time_in_ms_(0) {
  const char* threadName = "external_thread";
  lock_ = CriticalSectionWrapper::CreateCriticalSection();
  event_ = EventWrapper::Create();
  thread_ = ThreadWrapper::CreateThread(Run, this, kHighPriority, threadName);
  if (thread_) {
    unsigned int id;
    thread_->Start(id);
  }
}

FakeExternalTransport::~FakeExternalTransport() {
  if (thread_) {
    thread_->SetNotAlive();
    event_->Set();
    if (thread_->Stop()) {
      delete thread_;
      thread_ = NULL;
      delete event_;
      event_ = NULL;
      delete lock_;
      lock_ = NULL;
    }
  }
}

bool FakeExternalTransport::Run(void* ptr) {
  return static_cast<FakeExternalTransport*> (ptr)->Process();
}

bool FakeExternalTransport::Process() {
  switch (event_->Wait(500)) {
    case kEventSignaled:
      lock_->Enter();
      my_network_->ReceivedRTPPacket(channel_, packet_buffer_, length_);
      lock_->Leave();
      return true;
    case kEventTimeout:
      return true;
    case kEventError:
      break;
  }
  return true;
}

int FakeExternalTransport::SendPacket(int channel, const void *data, int len) {
  lock_->Enter();
  if (len < 1612) {
    memcpy(packet_buffer_, (const unsigned char*) data, len);
    length_ = len;
    channel_ = channel;
  }
  lock_->Leave();
  event_->Set(); // triggers ReceivedRTPPacket() from worker thread
  return len;
}

int FakeExternalTransport::SendRTCPPacket(int channel, const void *data, int len) {
  if (delay_is_enabled_) {
    Sleep(delay_time_in_ms_);
  }
  my_network_->ReceivedRTCPPacket(channel, data, len);
  return len;
}

void FakeExternalTransport::SetDelayStatus(bool enable, unsigned int delayInMs) {
  delay_is_enabled_ = enable;
  delay_time_in_ms_ = delayInMs;
}

ErrorObserver::ErrorObserver() {
  code = -1;
}
void ErrorObserver::CallbackOnError(const int channel, const int errCode) {
  code = errCode;
#ifndef _INSTRUMENTATION_TESTING_
  TEST_LOG("\n************************\n");
  TEST_LOG(" RUNTIME ERROR: %d \n", errCode);
  TEST_LOG("************************\n");
#endif
}

void MyTraceCallback::Print(const TraceLevel level,
                            const char *traceString,
                            const int length) {
  if (traceString) {
    char* tmp = new char[length];
    memcpy(tmp, traceString, length);
    TEST_LOG("%s", tmp);
    TEST_LOG("\n");
    delete[] tmp;
  }
}

void RtcpAppHandler::OnApplicationDataReceived(
    const int /*channel*/, const unsigned char sub_type,
    const unsigned int name, const unsigned char* data,
    const unsigned short length_in_bytes) {
  length_in_bytes_ = length_in_bytes;
  memcpy(data_, &data[0], length_in_bytes);
  sub_type_ = sub_type;
  name_ = name;
}

void RtcpAppHandler::Reset() {
  length_in_bytes_ = 0;
  memset(data_, 0, sizeof(data_));
  sub_type_ = 0;
  name_ = 0;
}

void SubAPIManager::DisplayStatus() const {
  TEST_LOG("Supported sub APIs:\n\n");
  if (_base)
    TEST_LOG("  Base\n");
  if (_callReport)
    TEST_LOG("  CallReport\n");
  if (_codec)
    TEST_LOG("  Codec\n");
  if (_dtmf)
    TEST_LOG("  Dtmf\n");
  if (_encryption)
    TEST_LOG("  Encryption\n");
  if (_externalMedia)
    TEST_LOG("  ExternalMedia\n");
  if (_file)
    TEST_LOG("  File\n");
  if (_hardware)
    TEST_LOG("  Hardware\n");
  if (_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (_network)
    TEST_LOG("  Network\n");
  if (_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (_videoSync)
    TEST_LOG("  VideoSync\n");
  if (_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
  TEST_LOG("Excluded sub APIs:\n\n");
  if (!_base)
    TEST_LOG("  Base\n");
  if (!_callReport)
    TEST_LOG("  CallReport\n");
  if (!_codec)
    TEST_LOG("  Codec\n");
  if (!_dtmf)
    TEST_LOG("  Dtmf\n");
  if (!_encryption)
    TEST_LOG("  Encryption\n");
  if (!_externalMedia)
    TEST_LOG("  ExternamMedia\n");
  if (!_file)
    TEST_LOG("  File\n");
  if (!_hardware)
    TEST_LOG("  Hardware\n");
  if (!_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (!_network)
    TEST_LOG("  Network\n");
  if (!_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (!_videoSync)
    TEST_LOG("  VideoSync\n");
  if (!_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (!_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
}

bool SubAPIManager::GetExtendedMenuSelection(ExtendedSelection& sel) {
  printf("------------------------------------------------\n");
  printf("Select extended test\n\n");
  printf(" (0)  None\n");
  printf("- - - - - - - - - - - - - - - - - - - - - - - - \n");
  printf(" (1)  Base");
  if (_base)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (2)  CallReport");
  if (_callReport)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (3)  Codec");
  if (_codec)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (4)  Dtmf");
  if (_dtmf)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (5)  Encryption");
  if (_encryption)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (6)  VoEExternalMedia");
  if (_externalMedia)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (7)  File");
  if (_file)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (8)  Mixing");
  if (_file)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (9)  Hardware");
  if (_hardware)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (10) NetEqStats");
  if (_netEqStats)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (11) Network");
  if (_network)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (12) RTP_RTCP");
  if (_rtp_rtcp)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (13) VideoSync");
  if (_videoSync)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (14) VolumeControl");
  if (_volumeControl)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (15) AudioProcessing");
  if (_apm)
    printf("\n");
  else
    printf(" (NA)\n");
  printf("\n: ");

  ExtendedSelection xsel(XSEL_Invalid);
  int selection(0);
  dummy = scanf("%d", &selection);

  switch (selection) {
    case 0:
      xsel = XSEL_None;
      break;
    case 1:
      if (_base)
        xsel = XSEL_Base;
      break;
    case 2:
      if (_callReport)
        xsel = XSEL_CallReport;
      break;
    case 3:
      if (_codec)
        xsel = XSEL_Codec;
      break;
    case 4:
      if (_dtmf)
        xsel = XSEL_DTMF;
      break;
    case 5:
      if (_encryption)
        xsel = XSEL_Encryption;
      break;
    case 6:
      if (_externalMedia)
        xsel = XSEL_ExternalMedia;
      break;
    case 7:
      if (_file)
        xsel = XSEL_File;
      break;
    case 8:
      if (_file)
        xsel = XSEL_Mixing;
      break;
    case 9:
      if (_hardware)
        xsel = XSEL_Hardware;
      break;
    case 10:
      if (_netEqStats)
        xsel = XSEL_NetEqStats;
      break;
    case 11:
      if (_network)
        xsel = XSEL_Network;
      break;
    case 12:
      if (_rtp_rtcp)
        xsel = XSEL_RTP_RTCP;
      break;
    case 13:
      if (_videoSync)
        xsel = XSEL_VideoSync;
      break;
    case 14:
      if (_volumeControl)
        xsel = XSEL_VolumeControl;
      break;
    case 15:
      if (_apm)
        xsel = XSEL_AudioProcessing;
      break;
    default:
      xsel = XSEL_Invalid;
      break;
  }
  if (xsel == XSEL_Invalid)
    printf("Invalid selection!\n");

  sel = xsel;
  _xsel = xsel;

  return (xsel != XSEL_Invalid);
}

VoETestManager::VoETestManager()
    : initialized_(false),
      voice_engine_(NULL),
      voe_base_(0),
      voe_call_report_(0),
      voe_codec_(0),
      voe_dtmf_(0),
      voe_encrypt_(0),
      voe_xmedia_(0),
      voe_file_(0),
      voe_hardware_(0),
      voe_network_(0),
#ifdef _TEST_NETEQ_STATS_
      voe_neteq_stats_(NULL),
#endif
      voe_rtp_rtcp_(0),
      voe_vsync_(0),
      voe_volume_control_(0),
      voe_apm_(0)
{
}

VoETestManager::~VoETestManager() {
}

bool VoETestManager::Init() {
  if (initialized_)
    return true;

  if (VoiceEngine::SetTraceFile(NULL) != -1) {
    // should not be possible to call a Trace method before the VoE is
    // created
    TEST_LOG("\nError at line: %i (VoiceEngine::SetTraceFile()"
      "should fail)!\n", __LINE__);
    return false;
  }

  voice_engine_ = VoiceEngine::Create();
  if (!voice_engine_) {
    TEST_LOG("Failed to create VoiceEngine\n");
    return false;
  }

  return true;
}

void VoETestManager::GetInterfaces() {
  if (voice_engine_) {
    voe_base_ = VoEBase::GetInterface(voice_engine_);
    voe_codec_ = VoECodec::GetInterface(voice_engine_);
    voe_volume_control_ = VoEVolumeControl::GetInterface(voice_engine_);
    voe_dtmf_ = VoEDtmf::GetInterface(voice_engine_);
    voe_rtp_rtcp_ = VoERTP_RTCP::GetInterface(voice_engine_);
    voe_apm_ = VoEAudioProcessing::GetInterface(voice_engine_);
    voe_network_ = VoENetwork::GetInterface(voice_engine_);
    voe_file_ = VoEFile::GetInterface(voice_engine_);
#ifdef _TEST_VIDEO_SYNC_
    voe_vsync_ = VoEVideoSync::GetInterface(voice_engine_);
#endif
    voe_encrypt_ = VoEEncryption::GetInterface(voice_engine_);
    voe_hardware_ = VoEHardware::GetInterface(voice_engine_);
    // Set the audio layer to use in all tests
    if (voe_hardware_) {
      int res = voe_hardware_->SetAudioDeviceLayer(TESTED_AUDIO_LAYER);
      if (res < 0) {
        printf("\nERROR: failed to set audio layer to use in "
          "testing\n");
      } else {
        printf("\nAudio layer %d will be used in testing\n",
               TESTED_AUDIO_LAYER);
      }
    }
#ifdef _TEST_XMEDIA_
    voe_xmedia_ = VoEExternalMedia::GetInterface(voice_engine_);
#endif
#ifdef _TEST_CALL_REPORT_
    voe_call_report_ = VoECallReport::GetInterface(voice_engine_);
#endif
#ifdef _TEST_NETEQ_STATS_
    voe_neteq_stats_ = VoENetEqStats::GetInterface(voice_engine_);
#endif
  }
}

int VoETestManager::ReleaseInterfaces() {
  int err(0), remInt(1), j(0);
  bool releaseOK(true);

  if (voe_base_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_base_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d base interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    // try to release one addition time (should fail)
    TEST_MUSTPASS(-1 != voe_base_->Release());
    err = voe_base_->LastError();
    // it is considered safe to delete even if Release has been called
    // too many times
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_codec_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_codec_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d codec interfaces"
        " (should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_codec_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_volume_control_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_volume_control_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d volume interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_volume_control_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_dtmf_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_dtmf_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d dtmf interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_dtmf_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_rtp_rtcp_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_rtp_rtcp_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d rtp/rtcp interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_rtp_rtcp_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_apm_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_apm_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d apm interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_apm_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_network_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_network_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d network interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_network_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_file_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_file_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d file interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_file_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#ifdef _TEST_VIDEO_SYNC_
  if (voe_vsync_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_vsync_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d video sync interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_vsync_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
  if (voe_encrypt_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_encrypt_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d encryption interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_encrypt_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_hardware_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_hardware_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d hardware interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_hardware_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#ifdef _TEST_XMEDIA_
  if (voe_xmedia_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_xmedia_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d external media interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_xmedia_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
#ifdef _TEST_CALL_REPORT_
  if (voe_call_report_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_call_report_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d call report interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_call_report_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
#ifdef _TEST_NETEQ_STATS_
  if (voe_neteq_stats_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_neteq_stats_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d neteq stat interfaces "
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_neteq_stats_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
  if (false == VoiceEngine::Delete(voice_engine_)) {
    TEST_LOG("\n\nVoiceEngine::Delete() failed. \n");
    releaseOK = false;
  }

  if (VoiceEngine::SetTraceFile(NULL) != -1) {
    TEST_LOG("\nError at line: %i (VoiceEngine::SetTraceFile()"
      "should fail)!\n", __LINE__);
  }

  return (releaseOK == true) ? 0 : -1;
}

int VoETestManager::SetUp(ErrorObserver* error_observer) {
  char char_buffer[1024];

  TEST_MUSTPASS(voe_base_->Init());

#if defined(WEBRTC_ANDROID)
  TEST_MUSTPASS(voe_hardware_->SetLoudspeakerStatus(false));
#endif

  TEST_MUSTPASS(voe_base_->RegisterVoiceEngineObserver(*error_observer));

  TEST_LOG("Get version \n");
  TEST_MUSTPASS(voe_base_->GetVersion(char_buffer));
  TEST_LOG("--------------------\n%s\n--------------------\n", char_buffer);

  TEST_LOG("Create channel \n");
  int nChannels = voe_base_->MaxNumOfChannels();
  TEST_MUSTPASS(!(nChannels > 0));
  TEST_LOG("Max number of channels = %d \n", nChannels);
  TEST_MUSTPASS(voe_base_->CreateChannel());

  return 0;
}

int VoETestManager::TestStartStreaming(FakeExternalTransport& channel0_transport) {
  TEST_LOG("\n\n+++ Starting streaming +++\n\n");

#ifdef WEBRTC_EXTERNAL_TRANSPORT
  TEST_LOG("Enabling external transport \n");
  TEST_MUSTPASS(voe_network_->RegisterExternalTransport(0, channel0_transport));
#else
  TEST_LOG("Setting send and receive parameters \n");
  TEST_MUSTPASS(voe_base_->SetSendDestination(0, 8000, "127.0.0.1"));
  // No IP specified => "0.0.0.0" will be stored.
  TEST_MUSTPASS(voe_base_->SetLocalReceiver(0,8000));

  CodecInst Jing_inst;
  Jing_inst.channels = 1;
  Jing_inst.pacsize = 160;
  Jing_inst.plfreq = 8000;
  Jing_inst.pltype = 0;
  Jing_inst.rate = 64000;
  strcpy(Jing_inst.plname, "PCMU");
  TEST_MUSTPASS(voe_codec_->SetSendCodec(0, Jing_inst));

  int port = -1;
  int src_port = -1;
  int rtcp_port = -1;
  char ip_address[64] = { 0 };
  strcpy(ip_address, "10.10.10.10");
  TEST_MUSTPASS(voe_base_->GetSendDestination(0, port, ip_address, src_port,
                                         rtcp_port));
  TEST_MUSTPASS(8000 != port);
  TEST_MUSTPASS(8000 != src_port);
  TEST_MUSTPASS(8001 != rtcp_port);
  TEST_MUSTPASS(_stricmp(ip_address, "127.0.0.1"));

  port = -1;
  rtcp_port = -1;
  TEST_MUSTPASS(voe_base_->GetLocalReceiver(0, port, rtcp_port, ip_address));
  TEST_MUSTPASS(8000 != port);
  TEST_MUSTPASS(8001 != rtcp_port);
  TEST_MUSTPASS(_stricmp(ip_address, "0.0.0.0"));
#endif
  return 0;
}

int VoETestManager::TestStartPlaying() {
  TEST_LOG("Start listening, playout and sending \n");
  TEST_MUSTPASS(voe_base_->StartReceive(0));
  TEST_MUSTPASS(voe_base_->StartPlayout(0));
  TEST_MUSTPASS(voe_base_->StartSend(0));

  // Run in full duplex.
  TEST_LOG("You should now hear yourself, running default codec (PCMU)\n");
  SLEEP(2000);

  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone, so you don't need to"
      " speak all the time\n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
    SLEEP(1000);
  }
  return 0;
}

int VoETestManager::DoStandardTest() {
  // Ensure we have all input files:
  TEST_MUSTPASS(!strcmp("", AudioFilename()));

  TEST_LOG("\n\n+++ Base tests +++\n\n");

  ErrorObserver error_observer;
  if (SetUp(&error_observer) != 0) return -1;

  voe_network_->SetSourceFilter(0, 0);

  FakeExternalTransport channel0_transport(voe_network_);
  if (TestStartStreaming(channel0_transport) != 0) return -1;
  if (TestStartPlaying() != 0) return -1;

  //////////////////
  // Stop streaming
  TEST_LOG("\n\n+++ Stop streaming +++\n\n");

  TEST_LOG("Stop playout, sending and listening \n");
  TEST_MUSTPASS(voe_base_->StopPlayout(0));
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_base_->StopReceive(0));

  // Exit:
  TEST_LOG("Delete channel and terminate VE \n");
  TEST_MUSTPASS(voe_base_->DeleteChannel(0));
  TEST_MUSTPASS(voe_base_->Terminate());

  return 0;
}

int runAutoTest(TestType testType, ExtendedSelection extendedSel) {
  SubAPIManager apiMgr;
  apiMgr.DisplayStatus();

  ////////////////////////////////////
  // Create VoiceEngine and sub API:s

  voetest::VoETestManager tm;
  if (!tm.Init()) {
    return -1;
  }
  tm.GetInterfaces();

  //////////////////////
  // Run standard tests

  int mainRet(-1);
  if (testType == Standard) {
    mainRet = tm.DoStandardTest();

    ////////////////////////////////
    // Create configuration summary
    TEST_LOG("\n\n+++ Creating configuration summary file +++\n");
    createSummary(tm.VoiceEnginePtr());
  } else if (testType == Extended) {
    VoEExtendedTest xtend(tm);

    mainRet = 0;
    while (extendedSel != XSEL_None) {
      if (extendedSel == XSEL_Base || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestBase()) == -1)
          break;
        xtend.TestPassed("Base");
      }
      if (extendedSel == XSEL_CallReport || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestCallReport()) == -1)
          break;
        xtend.TestPassed("CallReport");
      }
      if (extendedSel == XSEL_Codec || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestCodec()) == -1)
          break;
        xtend.TestPassed("Codec");
      }
      if (extendedSel == XSEL_DTMF || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestDtmf()) == -1)
          break;
        xtend.TestPassed("Dtmf");
      }
      if (extendedSel == XSEL_Encryption || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestEncryption()) == -1)
          break;
        xtend.TestPassed("Encryption");
      }
      if (extendedSel == XSEL_ExternalMedia || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestExternalMedia()) == -1)
          break;
        xtend.TestPassed("ExternalMedia");
      }
      if (extendedSel == XSEL_File || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestFile()) == -1)
          break;
        xtend.TestPassed("File");
      }
      if (extendedSel == XSEL_Mixing || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestMixing()) == -1)
          break;
        xtend.TestPassed("Mixing");
      }
      if (extendedSel == XSEL_Hardware || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestHardware()) == -1)
          break;
        xtend.TestPassed("Hardware");
      }
      if (extendedSel == XSEL_NetEqStats || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestNetEqStats()) == -1)
          break;
        xtend.TestPassed("NetEqStats");
      }
      if (extendedSel == XSEL_Network || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestNetwork()) == -1)
          break;
        xtend.TestPassed("Network");
      }
      if (extendedSel == XSEL_RTP_RTCP || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestRTP_RTCP()) == -1)
          break;
        xtend.TestPassed("RTP_RTCP");
      }
      if (extendedSel == XSEL_VideoSync || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestVideoSync()) == -1)
          break;
        xtend.TestPassed("VideoSync");
      }
      if (extendedSel == XSEL_VolumeControl || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestVolumeControl()) == -1)
          break;
        xtend.TestPassed("VolumeControl");
      }
      if (extendedSel == XSEL_AudioProcessing || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestAPM()) == -1)
          break;
        xtend.TestPassed("AudioProcessing");
      }
      apiMgr.GetExtendedMenuSelection(extendedSel);
    } // while (extendedSel != XSEL_None)
  } else if (testType == Stress) {
    VoEStressTest stressTest(tm);
    mainRet = stressTest.DoTest();
  } else if (testType == Unit) {
    VoEUnitTest unitTest(tm);
    mainRet = unitTest.DoTest();
  } else if (testType == CPU) {
    VoECpuTest cpuTest(tm);
    mainRet = cpuTest.DoTest();
  } else {
    // Should never end up here
    TEST_LOG("INVALID SELECTION \n");
  }

  //////////////////
  // Release/Delete

  int releaseOK = tm.ReleaseInterfaces();

  if ((0 == mainRet) && (releaseOK != -1)) {
    TEST_LOG("\n\n*** All tests passed *** \n\n");
  } else {
    TEST_LOG("\n\n*** Test failed! *** \n");
  }

  return 0;
}

void createSummary(VoiceEngine* ve) {
  int len;
  char str[256];

#ifdef MAC_IPHONE
  char summaryFilename[256];
  GetDocumentsDir(summaryFilename, 256);
  strcat(summaryFilename, "/summary.txt");
#endif

  VoEBase* voe_base_ = VoEBase::GetInterface(ve);
  FILE* stream = fopen(summaryFilename, "wt");

  sprintf(str, "WebRTc VoiceEngine ");
#if defined(_WIN32)
  strcat(str, "Win");
#elif defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
  strcat(str, "Linux");
#elif defined(WEBRTC_MAC) && !defined(MAC_IPHONE)
  strcat(str, "Mac");
#elif defined(WEBRTC_ANDROID)
  strcat(str, "Android");
#elif defined(MAC_IPHONE)
  strcat(str, "iPhone");
#endif
  // Add for other platforms as needed

  fprintf(stream, "%s\n", str);
  len = (int) strlen(str);
  for (int i = 0; i < len; i++) {
    fprintf(stream, "=");
  }
  fprintf(stream, "\n\n");

  char version[1024];
  char veVersion[24];
  voe_base_->GetVersion(version);
  // find first NL <=> end of VoiceEngine version string
  int pos = (int) strcspn(version, "\n");
  strncpy(veVersion, version, pos);
  veVersion[pos] = '\0';
  sprintf(str, "Version:                    %s\n", veVersion);
  fprintf(stream, "%s\n", str);

  sprintf(str, "Build date & time:          %s\n", BUILDDATE " " BUILDTIME);
  fprintf(stream, "%s\n", str);

  strcpy(str, "G.711 A-law");
  fprintf(stream, "\nSupported codecs:           %s\n", str);
  strcpy(str, "                            G.711 mu-law");
  fprintf(stream, "%s\n", str);
#ifdef WEBRTC_CODEC_EG711
  strcpy(str, "                            Enhanced G.711 A-law");
  fprintf(stream, "%s\n", str);
  strcpy(str, "                            Enhanced G.711 mu-law");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_IPCMWB
  strcpy(str, "                            iPCM-wb");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_ILBC
  strcpy(str, "                            iLBC");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_ISAC
  strcpy(str, "                            iSAC");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_ISACLC
  strcpy(str, "                            iSAC-LC");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G722
  strcpy(str, "                            G.722");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G722_1
  strcpy(str, "                            G.722.1");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G722_1C
  strcpy(str, "                            G.722.1C");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G723
  strcpy(str, "                            G.723");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G726
  strcpy(str, "                            G.726");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G729
  strcpy(str, "                            G.729");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G729_1
  strcpy(str, "                            G.729.1");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMFR
  strcpy(str, "                            GSM-FR");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMAMR
  strcpy(str, "                            AMR");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMAMRWB
  strcpy(str, "                            AMR-WB");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMEFR
  strcpy(str, "                            GSM-EFR");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_SPEEX
  strcpy(str, "                            Speex");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_SILK
  strcpy(str, "                            Silk");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_PCM16
  strcpy(str, "                            L16");
  fprintf(stream, "%s\n", str);
#endif
#ifdef NETEQFIX_VOXWARE_SC3
  strcpy(str, "                            Voxware SC3");
  fprintf(stream, "%s\n", str);
#endif
  // Always included
  strcpy(str, "                            AVT (RFC2833)");
  fprintf(stream, "%s\n", str);
#ifdef WEBRTC_CODEC_RED
  strcpy(str, "                            RED (forward error correction)");
  fprintf(stream, "%s\n", str);
#endif

  fprintf(stream, "\nEcho Control:               ");
#ifdef WEBRTC_VOICE_ENGINE_ECHO
  fprintf(stream, "Yes\n");
#else
  fprintf(stream, "No\n");
#endif

  fprintf(stream, "\nAutomatic Gain Control:     ");
#ifdef WEBRTC_VOICE_ENGINE_AGC
  fprintf(stream, "Yes\n");
#else
  fprintf(stream, "No\n");
#endif

  fprintf(stream, "\nNoise Reduction:            ");
#ifdef WEBRTC_VOICE_ENGINE_NR
  fprintf(stream, "Yes\n");
#else
  fprintf(stream, "No\n");
#endif

  fprintf(stream, "\nSRTP:                       ");
#ifdef WEBRTC_SRTP
  fprintf(stream, "Yes\n");
#else
  fprintf(stream, "No\n");
#endif

  fprintf(stream, "\nExternal transport only:    ");
#ifdef WEBRTC_EXTERNAL_TRANSPORT
  fprintf(stream, "Yes\n");
#else
  fprintf(stream, "No\n");
#endif

  fprintf(stream, "\nTelephone event detection:  ");
#ifdef WEBRTC_DTMF_DETECTION
  fprintf(stream, "Yes\n");
#else
  fprintf(stream, "No\n");
#endif

  strcpy(str, "VoEBase");
  fprintf(stream, "\nSupported sub-APIs:         %s\n", str);
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
  strcpy(str, "                            VoECodec");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
  strcpy(str, "                            VoEDtmf");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
  strcpy(str, "                            VoEFile");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
  strcpy(str, "                            VoEHardware");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
  strcpy(str, "                            VoENetwork");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
  strcpy(str, "                            VoERTP_RTCP");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
  strcpy(str, "                            VoEVolumeControl");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
  strcpy(str, "                            VoEAudioProcessing");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
  strcpy(str, "                            VoeExternalMedia");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
  strcpy(str, "                            VoENetEqStats");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
  strcpy(str, "                            VoEEncryption");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
  strcpy(str, "                            VoECallReport");
  fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
  strcpy(str, "                            VoEVideoSync");
  fprintf(stream, "%s\n", str);
#endif

  fclose(stream);
  voe_base_->Release();
}

/*********Knowledge Base******************/

//An example for creating threads and calling VE API's from that thread.
// Using thread.  A generic API/Class for all platforms.
#ifdef THEADTEST // find first NL <=> end of VoiceEngine version string
//Definition of Thread Class
class ThreadTest
{
public:
  ThreadTest(
      VoEBase* voe_base_);
  ~ThreadTest()
  {
    delete _myThread;
  }
  void Stop();
private:
  static bool StartSend(
      void* obj);
  bool StartSend();

  ThreadWrapper* _myThread;
  VoEBase* _base;

  bool _stopped;
};

//Main function from where StartSend is invoked as a seperate thread.
ThreadTest::ThreadTest(
    VoEBase* voe_base_)
:
_stopped(false),
_base(voe_base_)
{
  //Thread Creation
  _myThread = ThreadWrapper::CreateThread(StartSend, this, kLowPriority);
  unsigned int id = 0;
  //Starting the thread
  _myThread->Start(id);
}

//Calls the StartSend.  This is to avoid the static declaration issue.
bool
ThreadTest::StartSend(
    void* obj)
{
  return ((ThreadTest*)obj)->StartSend();
}

bool
ThreadTest::StartSend()
{
  _myThread->SetNotAlive(); //Ensures this function is called only once.
  _base->StartSend(0);
  return true;
}

void ThreadTest::Stop()
{
  _stopped = true;
}

//  Use the following to invoke ThreatTest from the main function.
//  ThreadTest* threadtest = new ThreadTest(voe_base_);
#endif

// An example to create a thread and call VE API's call from that thread.
// Specific to Windows Platform
#ifdef THREAD_TEST_WINDOWS
//Thread Declaration.  Need to be added in the class controlling/dictating
// the main code.
/**
 private:
 static unsigned int WINAPI StartSend(void* obj);
 unsigned int WINAPI StartSend();
 **/

//Thread Definition
unsigned int WINAPI mainTest::StartSend(void *obj)
{
  return ((mainTest*)obj)->StartSend();
}
unsigned int WINAPI mainTest::StartSend()
{
  //base
  voe_base_->StartSend(0);

  //  TEST_MUSTPASS(voe_base_->StartSend(0));
  TEST_LOG("hi hi hi");
  return 0;
}

//Thread invoking.  From the main code
/*****
 unsigned int threadID=0;
 if ((HANDLE)_beginthreadex(NULL,
 0,
 StartSend,
 (void*)this,
 0,
 &threadID) == NULL)
 return false;
 ****/

#endif

} // namespace voetest

int RunInManualMode(int argc, char** argv) {
  using namespace voetest;

  SubAPIManager apiMgr;
  apiMgr.DisplayStatus();

  printf("----------------------------\n");
  printf("Select type of test\n\n");
  printf(" (0)  Quit\n");
  printf(" (1)  Standard test\n");
  printf(" (2)  Extended test(s)...\n");
  printf(" (3)  Stress test(s)...\n");
  printf(" (4)  Unit test(s)...\n");
  printf(" (5)  CPU & memory reference test [Windows]...\n");
  printf("\n: ");

  int selection(0);

  dummy = scanf("%d", &selection);

  ExtendedSelection extendedSel(XSEL_Invalid);

  enum TestType testType(Invalid);

  switch (selection) {
    case 0:
      return 0;
    case 1:
      testType = Standard;
      break;
    case 2:
      testType = Extended;
      while (!apiMgr.GetExtendedMenuSelection(extendedSel))
        continue;
      break;
    case 3:
      testType = Stress;
      break;
    case 4:
      testType = Unit;
      break;
    case 5:
      testType = CPU;
      break;
    default:
      TEST_LOG("Invalid selection!\n");
      return 0;
  }

  if (testType == Standard) {
    TEST_LOG("\n\n+++ Running gtest-rewritten standard tests first +++\n\n");

    // Run the automated tests too in standard mode since we are gradually
    // rewriting the standard test to be automated. Running this will give
    // the standard suite the same completeness.
    RunInAutomatedMode(argc, argv);
  }

  // Function that can be called from other entry functions.
  return runAutoTest(testType, extendedSel);
}

// ----------------------------------------------------------------------------
//                                       main
// ----------------------------------------------------------------------------

#if !defined(MAC_IPHONE)
int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--automated") {
    // This function is defined in automated_mode.cc to avoid macro clashes
    // with googletest (for instance the ASSERT_TRUE macro).
    return RunInAutomatedMode(argc, argv);
  }

  return RunInManualMode(argc, argv);
}
#endif //#if !defined(MAC_IPHONE)
