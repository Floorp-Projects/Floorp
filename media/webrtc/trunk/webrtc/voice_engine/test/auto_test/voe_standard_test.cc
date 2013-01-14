/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/test/auto_test/voe_standard_test.h"

#include <stdio.h>
#include <string.h>

#include "engine_configurations.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "voice_engine/include/voe_neteq_stats.h"
#include "voice_engine/test/auto_test/automated_mode.h"
#include "voice_engine/test/auto_test/voe_cpu_test.h"
#include "voice_engine/test/auto_test/voe_extended_test.h"
#include "voice_engine/test/auto_test/voe_stress_test.h"
#include "voice_engine/test/auto_test/voe_unit_test.h"
#include "voice_engine/voice_engine_defines.h"

DEFINE_bool(include_timing_dependent_tests, true,
            "If true, we will include tests / parts of tests that are known "
            "to break in slow execution environments (such as valgrind).");
DEFINE_bool(automated, false,
            "If true, we'll run the automated tests we have in noninteractive "
            "mode.");

using namespace webrtc;

namespace voetest {

int dummy = 0;  // Dummy used in different functions to avoid warnings

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
  printf(" (8)  Hardware");
  if (_hardware)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (9) NetEqStats");
  if (_netEqStats)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (10) Network");
  if (_network)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (11) RTP_RTCP");
  if (_rtp_rtcp)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (12) VideoSync");
  if (_videoSync)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (13) VolumeControl");
  if (_volumeControl)
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
      if (_hardware)
        xsel = XSEL_Hardware;
      break;
    case 9:
      if (_netEqStats)
        xsel = XSEL_NetEqStats;
      break;
    case 10:
      if (_network)
        xsel = XSEL_Network;
      break;
    case 11:
      if (_rtp_rtcp)
        xsel = XSEL_RTP_RTCP;
      break;
    case 12:
      if (_videoSync)
        xsel = XSEL_VideoSync;
      break;
    case 13:
      if (_volumeControl)
        xsel = XSEL_VolumeControl;
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
  bool releaseOK(true);

  if (voe_base_) {
    voe_base_->Release();
    voe_base_ = NULL;
  }
  if (voe_codec_) {
    voe_codec_->Release();
    voe_codec_ = NULL;
  }
  if (voe_volume_control_) {
    voe_volume_control_->Release();
    voe_volume_control_ = NULL;
  }
  if (voe_dtmf_) {
    voe_dtmf_->Release();
    voe_dtmf_ = NULL;
  }
  if (voe_rtp_rtcp_) {
    voe_rtp_rtcp_->Release();
    voe_rtp_rtcp_ = NULL;
  }
  if (voe_apm_) {
    voe_apm_->Release();
    voe_apm_ = NULL;
  }
  if (voe_network_) {
    voe_network_->Release();
    voe_network_ = NULL;
  }
  if (voe_file_) {
    voe_file_->Release();
    voe_file_ = NULL;
  }
#ifdef _TEST_VIDEO_SYNC_
  if (voe_vsync_) {
    voe_vsync_->Release();
    voe_vsync_ = NULL;
  }
#endif
  if (voe_encrypt_) {
    voe_encrypt_->Release();
    voe_encrypt_ = NULL;
  }
  if (voe_hardware_) {
    voe_hardware_->Release();
    voe_hardware_ = NULL;
  }
#ifdef _TEST_XMEDIA_
  if (voe_xmedia_) {
    voe_xmedia_->Release();
    voe_xmedia_ = NULL;
  }
#endif
#ifdef _TEST_CALL_REPORT_
  if (voe_call_report_) {
    voe_call_report_->Release();
    voe_call_report_ = NULL;
  }
#endif
#ifdef _TEST_NETEQ_STATS_
  if (voe_neteq_stats_) {
    voe_neteq_stats_->Release();
    voe_neteq_stats_ = NULL;
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

int run_auto_test(TestType test_type, ExtendedSelection ext_selection) {
  assert(test_type != Standard);

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

  ////////////////////////////////////
  // Create VoiceEngine and sub API:s

  voetest::VoETestManager test_manager;
  if (!test_manager.Init()) {
    return -1;
  }
  test_manager.GetInterfaces();

  int result(-1);
  if (test_type == Extended) {
    VoEExtendedTest xtend(test_manager);

    result = 0;
    while (ext_selection != XSEL_None) {
      if (ext_selection == XSEL_Base || ext_selection == XSEL_All) {
        if ((result = xtend.TestBase()) == -1)
          break;
        xtend.TestPassed("Base");
      }
      if (ext_selection == XSEL_CallReport || ext_selection == XSEL_All) {
        if ((result = xtend.TestCallReport()) == -1)
          break;
        xtend.TestPassed("CallReport");
      }
      if (ext_selection == XSEL_Codec || ext_selection == XSEL_All) {
        if ((result = xtend.TestCodec()) == -1)
          break;
        xtend.TestPassed("Codec");
      }
      if (ext_selection == XSEL_DTMF || ext_selection == XSEL_All) {
        if ((result = xtend.TestDtmf()) == -1)
          break;
        xtend.TestPassed("Dtmf");
      }
      if (ext_selection == XSEL_Encryption || ext_selection == XSEL_All) {
        if ((result = xtend.TestEncryption()) == -1)
          break;
        xtend.TestPassed("Encryption");
      }
      if (ext_selection == XSEL_ExternalMedia || ext_selection == XSEL_All) {
        if ((result = xtend.TestExternalMedia()) == -1)
          break;
        xtend.TestPassed("ExternalMedia");
      }
      if (ext_selection == XSEL_File || ext_selection == XSEL_All) {
        if ((result = xtend.TestFile()) == -1)
          break;
        xtend.TestPassed("File");
      }
      if (ext_selection == XSEL_Hardware || ext_selection == XSEL_All) {
        if ((result = xtend.TestHardware()) == -1)
          break;
        xtend.TestPassed("Hardware");
      }
      if (ext_selection == XSEL_NetEqStats || ext_selection == XSEL_All) {
        if ((result = xtend.TestNetEqStats()) == -1)
          break;
        xtend.TestPassed("NetEqStats");
      }
      if (ext_selection == XSEL_Network || ext_selection == XSEL_All) {
        if ((result = xtend.TestNetwork()) == -1)
          break;
        xtend.TestPassed("Network");
      }
      if (ext_selection == XSEL_RTP_RTCP || ext_selection == XSEL_All) {
        if ((result = xtend.TestRTP_RTCP()) == -1)
          break;
        xtend.TestPassed("RTP_RTCP");
      }
      if (ext_selection == XSEL_VideoSync || ext_selection == XSEL_All) {
        if ((result = xtend.TestVideoSync()) == -1)
          break;
        xtend.TestPassed("VideoSync");
      }
      if (ext_selection == XSEL_VolumeControl || ext_selection == XSEL_All) {
        if ((result = xtend.TestVolumeControl()) == -1)
          break;
        xtend.TestPassed("VolumeControl");
      }
      api_manager.GetExtendedMenuSelection(ext_selection);
    } // while (extendedSel != XSEL_None)
  } else if (test_type == Stress) {
    VoEStressTest stressTest(test_manager);
    result = stressTest.DoTest();
  } else if (test_type == Unit) {
    VoEUnitTest unitTest(test_manager);
    result = unitTest.DoTest();
  } else if (test_type == CPU) {
    VoECpuTest cpuTest(test_manager);
    result = cpuTest.DoTest();
  } else {
    // Should never end up here
    assert(false);
  }

  //////////////////
  // Release/Delete

  int release_ok = test_manager.ReleaseInterfaces();

  if ((0 == result) && (release_ok != -1)) {
    TEST_LOG("\n\n*** All tests passed *** \n\n");
  } else {
    TEST_LOG("\n\n*** Test failed! *** \n");
  }

  return 0;
}
} // namespace voetest

int RunInManualMode() {
  using namespace voetest;

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

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

  ExtendedSelection ext_selection = XSEL_Invalid;
  TestType test_type = Invalid;

  switch (selection) {
    case 0:
      return 0;
    case 1:
      test_type = Standard;
      break;
    case 2:
      test_type = Extended;
      while (!api_manager.GetExtendedMenuSelection(ext_selection))
        continue;
      break;
    case 3:
      test_type = Stress;
      break;
    case 4:
      test_type = Unit;
      break;
    case 5:
      test_type = CPU;
      break;
    default:
      TEST_LOG("Invalid selection!\n");
      return 0;
  }

  if (test_type == Standard) {
    TEST_LOG("\n\n+++ Running standard tests +++\n\n");

    // Currently, all googletest-rewritten tests are in the "automated" suite.
    return RunInAutomatedMode();
  }

  // Function that can be called from other entry functions.
  return run_auto_test(test_type, ext_selection);
}

// ----------------------------------------------------------------------------
//                                       main
// ----------------------------------------------------------------------------

#if !defined(WEBRTC_IOS)
int main(int argc, char** argv) {
  // This function and RunInAutomatedMode is defined in automated_mode.cc
  // to avoid macro clashes with googletest (for instance ASSERT_TRUE).
  InitializeGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_automated) {
    return RunInAutomatedMode();
  }

  return RunInManualMode();
}
#endif //#if !defined(WEBRTC_IOS)
